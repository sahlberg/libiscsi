/*
   iscsi-test tool support

   Copyright (C) 2012 by Lee Duncan <leeman.duncan@gmail.com>
   Copyright (C) 2014 Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"

#define _GNU_SOURCE
#include <assert.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <fnmatch.h>

#ifdef HAVE_SG_IO
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#endif

#include "slist.h"
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"
#include "iscsi-support.h"


/*****************************************************************
 * globals
 *****************************************************************/
const char *initiatorname1 =
	"iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test";
const char *initiatorname2 =
	"iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test-2";

int no_medium_ascqs[3] = {
	SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT,
	SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN,
	SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED
};
int lba_oob_ascqs[1] = {
	SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE
};
int invalid_cdb_ascqs[1] = {
	SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB
};
int write_protect_ascqs[3] = {
	SCSI_SENSE_ASCQ_WRITE_PROTECTED,
	SCSI_SENSE_ASCQ_HARDWARE_WRITE_PROTECTED,
	SCSI_SENSE_ASCQ_SOFTWARE_WRITE_PROTECTED
};
int sanitize_ascqs[1] = {
	SCSI_SENSE_ASCQ_SANITIZE_IN_PROGRESS
};
int removal_ascqs[1] = {
	SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED
};
int miscompare_ascqs[1] = {
	SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY
};

struct scsi_inquiry_standard *inq;
struct scsi_inquiry_logical_block_provisioning *inq_lbp;
struct scsi_inquiry_block_device_characteristics *inq_bdc;
struct scsi_inquiry_block_limits *inq_bl;
struct scsi_readcapacity16 *rc16;
struct scsi_report_supported_op_codes *rsop;

size_t block_size;
uint64_t num_blocks;
int lbppb;
enum scsi_inquiry_peripheral_device_type device_type;
int data_loss;
int allow_sanitize;
int readonly;
int sbc3_support;
int maximum_transfer_length;

int (*real_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

static const char *scsi_status_str(int status)
{
	switch(status) {
	case SCSI_STATUS_GOOD:			return "SUCCESS";
	case SCSI_STATUS_CHECK_CONDITION:	return "CHECK_CONDITION";
	case SCSI_STATUS_CONDITION_MET:		return "CONDITIONS_MET";
	case SCSI_STATUS_BUSY:			return "BUSY";
	case SCSI_STATUS_RESERVATION_CONFLICT:	return "RESERVATION_CONFLICT";
	case SCSI_STATUS_TASK_SET_FULL:		return "TASK_SET_FULL";
	case SCSI_STATUS_ACA_ACTIVE:		return "ACA_ACTIVE";
	case SCSI_STATUS_TASK_ABORTED:		return "TASK_ABORTED";
	}
	return "UNKNOWN";
}

/*
 * There is no agreement among the T10 committee whether a SCSI target should
 * report "invalid opcode", "invalid field in CDB" or "invalid field in
 * parameter list" if the opcode consists of two bytes. Hence accept all three
 * sense codes for two-byte opcodes. For more information see also Frederick
 * Knight, RE: INVALID COMMAND OPERATION CODE, T10 Reflector, 16 May 2008
 * (http://t10.org/ftp/t10/t10r/2008/r0805167.htm).
 */
static int status_is_invalid_opcode(struct scsi_task *task)
{
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST) {
		if (task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE)
			return 1;
		switch (task->cdb[0]) {
		case SCSI_OPCODE_MAINTENANCE_IN:
		case SCSI_OPCODE_SERVICE_ACTION_IN:
			switch (task->sense.ascq) {
			case SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB:
			case SCSI_SENSE_ASCQ_INVALID_FIELD_IN_PARAMETER_LIST:
				return 1;
			}
		}
	}
	return 0;
}

static int check_result(const char *opcode, struct scsi_device *sdev,
			struct scsi_task *task,
			int status, enum scsi_sense_key key,
			int *ascq, int num_ascq)
{
	int ascq_ok = 0;

	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send %s command: "
			"%s", opcode, sdev->error_str);
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		logging(LOG_NORMAL, "[SKIPPED] %s is not implemented.",
			opcode);
		return -2;
	}
	if (status == SCSI_STATUS_GOOD && task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] %s command failed with "
			"sense. %s", opcode, sdev->error_str);
		return -1;
	}
	if (status != SCSI_STATUS_GOOD && task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] %s successful but should "
			"have failed with %s(0x%02x)/%s(0x%04x)",
			opcode,
			scsi_sense_key_str(key), key,
			num_ascq ? scsi_sense_ascq_str(ascq[0]) : "NO ASCQ",
			num_ascq ? ascq[0] : 0);
		return -1;
	}
	if (status == SCSI_STATUS_RESERVATION_CONFLICT
	    && task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		logging(LOG_NORMAL, "[FAILED] %s command should have failed "
			"with RESERVATION_CONFLICT.", opcode);
		return -1;
	}
	/* did we get any of the expected ASCQs ?*/
	if (status == SCSI_STATUS_CHECK_CONDITION) {
		int i;
		for (i = 0; i < num_ascq; i++) {
			if (ascq[i] == task->sense.ascq) {
				ascq_ok = 1;
			}
		}
		if (num_ascq == 0) {
			ascq_ok = 1;
		}
	}
	if (status == SCSI_STATUS_CHECK_CONDITION &&
	    (task->status != status
	     || task->sense.key  != key
	     || !ascq_ok)) {
		logging(LOG_NORMAL, "[FAILED] %s failed with wrong sense. "
			"Should have failed with %s(0x%02x)/%s(0x%04x)"
			"but failed with Sense:%s\n",
			opcode,
			scsi_sense_key_str(key), key,
			num_ascq ? scsi_sense_ascq_str(ascq[0]) : "NO ASCQ",
			num_ascq ? ascq[0] : 0,
			sdev->error_str);
		return -1;
	}
	logging(LOG_VERBOSE, "[OK] %s returned %s %s(0x%02x) %s(0x%04x)",
		opcode, scsi_status_str(status),
		scsi_sense_key_str(task->sense.key), task->sense.key,
		scsi_sense_ascq_str(task->sense.ascq), task->sense.ascq);
	return 0;
}

static struct scsi_task *send_scsi_command(struct scsi_device *sdev, struct scsi_task *task, struct iscsi_data *d)
{
	if (sdev->iscsi_url) {
		if (sdev->error_str != NULL) {
			free(discard_const(sdev->error_str));
			sdev->error_str = NULL;
		}
		task = iscsi_scsi_command_sync(sdev->iscsi_ctx, sdev->iscsi_lun, task, d);
		if (task == NULL) {
			sdev->error_str = strdup(iscsi_get_error(sdev->iscsi_ctx));
		}

		return task;
	}

#ifdef HAVE_SG_IO
	if (sdev->sgio_dev) {
		sg_io_hdr_t io_hdr;
		unsigned int sense_len=32;
		unsigned char sense[sense_len];
		char buf[1024];

		memset(sense, 0, sizeof(sense));
		memset(&io_hdr, 0, sizeof(sg_io_hdr_t));
		io_hdr.interface_id = 'S';

		/* CDB */
		io_hdr.cmdp = task->cdb;
		io_hdr.cmd_len = task->cdb_size;

		/* Where to store the sense_data, if there was an error */
		io_hdr.sbp = sense;
		io_hdr.mx_sb_len = sense_len;
		sense_len=0;

		/* Transfer direction, either in or out. Linux does not yet
		   support bidirectional SCSI transfers ?
		*/
		switch (task->xfer_dir) {
		case SCSI_XFER_WRITE:
		  io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
		  io_hdr.dxferp = d->data;
		  io_hdr.dxfer_len = d->size;
		  break;
		case SCSI_XFER_READ:
		  io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
		  task->datain.size = task->expxferlen;
		  task->datain.data = malloc(task->datain.size);
		  memset(task->datain.data, 0, task->datain.size);
		  io_hdr.dxferp = task->datain.data;
		  io_hdr.dxfer_len = task->datain.size;
		  break;
		}

		/* SCSI timeout in ms */
		io_hdr.timeout = 5000;

		if(ioctl(sdev->sgio_fd, SG_IO, &io_hdr) < 0){
			if (sdev->error_str != NULL) {
				free(discard_const(sdev->error_str));
			}
			sdev->error_str = strdup("SG_IO ioctl failed");
			return NULL;
		}

		task->residual_status = SCSI_RESIDUAL_NO_RESIDUAL;
		task->residual = 0;

		if (io_hdr.resid) {
			task->residual_status = SCSI_RESIDUAL_UNDERFLOW;
			task->residual = io_hdr.resid;
		}

		if (task->xfer_dir == SCSI_XFER_READ)
			task->datain.size -= task->residual;

		/* now for the error processing */
		if(io_hdr.sb_len_wr > 0){
			task->status = SCSI_STATUS_CHECK_CONDITION;
			task->sense.error_type = sense[0] & 0x7f;
			switch (task->sense.error_type) {
			case 0x70:
			case 0x71:
				task->sense.key = sense[2] & 0x0f;
				task->sense.ascq  = scsi_get_uint16(&sense[12]);
				break;
			case 0x72:
			case 0x73:
				task->sense.key = sense[1] & 0x0f;
				task->sense.ascq = scsi_get_uint16(&sense[2]);
				break;
			}
			sense_len=io_hdr.sb_len_wr;
			snprintf(buf, sizeof(buf), "SENSE KEY:%s(%d) ASCQ:%s(0x%04x)",
				 scsi_sense_key_str(task->sense.key),
				 task->sense.key,
				 scsi_sense_ascq_str(task->sense.ascq),
				 task->sense.ascq);
			if (sdev->error_str != NULL) {
				free(discard_const(sdev->error_str));
			}
			sdev->error_str = strdup(buf);
			return task;
		}

		if(io_hdr.masked_status){
			task->status = SCSI_STATUS_ERROR;
			task->sense.key = 0x0f;
			task->sense.ascq  = 0xffff;

			if (sdev->error_str != NULL) {
				free(discard_const(sdev->error_str));
			}
			sdev->error_str = strdup("SCSI masked error");
			return NULL;
		}
		if(io_hdr.host_status){
			task->status = SCSI_STATUS_ERROR;
			task->sense.key = 0x0f;
			task->sense.ascq  = 0xffff;

			snprintf(buf, sizeof(buf), "SCSI host error. Status=0x%x", io_hdr.host_status);
			if (sdev->error_str != NULL) {
				free(discard_const(sdev->error_str));
			}
			sdev->error_str = strdup(buf);
			return task;
		}
		if(io_hdr.driver_status){
			task->status = SCSI_STATUS_ERROR;
			task->sense.key = 0x0f;
			task->sense.ascq  = 0xffff;

			if (sdev->error_str != NULL) {
				free(discard_const(sdev->error_str));
			}
			sdev->error_str = strdup("SCSI driver error");
			return NULL;
		}
		return task;
	}
#endif
	return NULL;
}

void logging(int level, const char *format, ...)
{
        va_list ap;
	static char message[1024];
	int ret;

	if (loglevel < level) {
		return;
	}

	if (strncmp(LOG_BLANK_LINE, format, LOG_BLANK_LINE_CMP_LEN)==0) {
		printf("\n");
		return;
	}

        va_start(ap, format);
	ret = vsnprintf(message, 1024, format, ap);
        va_end(ap);

	if (ret < 0) {
		return;
	}

	printf("    %s\n", message);
}

struct iscsi_context *
iscsi_context_login(const char *initiatorname, const char *url, int *lun)
{
	struct iscsi_context *iscsi;
	struct iscsi_url *iscsi_url;

	iscsi = iscsi_create_context(initiatorname);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		return NULL;
	}

	iscsi_url = iscsi_parse_full_url(iscsi, url);
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n",
			iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return NULL;
	}

	iscsi_set_targetname(iscsi, iscsi_url->target);
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_url->user != NULL) {
		if (iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
			fprintf(stderr, "Failed to set initiator username and password\n");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			return NULL;
		}
	}

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		return NULL;
	}
	if (lun != NULL) {
		*lun = iscsi_url->lun;
	}

	iscsi_destroy_url(iscsi_url);
	return iscsi;
}

void
wait_until_test_finished(struct iscsi_context *iscsi, struct iscsi_async_state *state)
{
	struct pollfd pfd;
	int count = 0;
	int ret;

	while (state->finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		ret = poll(&pfd, 1, 1000);
		if (ret < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (ret == 0) {
			if (count++ > 5) {
				struct iscsi_pdu *pdu;

				state->finished     = 1;
				state->status       = SCSI_STATUS_CANCELLED;
				state->task->status = SCSI_STATUS_CANCELLED;
				/* this may leak memory since we don't free the pdu */
				while ((pdu = iscsi->outqueue)) {
					ISCSI_LIST_REMOVE(&iscsi->outqueue, pdu);
				}
				while ((pdu = iscsi->waitpdu)) {
					ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				}
				return;
			}
			continue;
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			printf("iscsi_service failed with : %s\n", iscsi_get_error(iscsi));
			break;
		}
	}
}

int
iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	if (local_iscsi_queue_pdu != NULL) {
		local_iscsi_queue_pdu(iscsi, pdu);
	}
	return real_iscsi_queue_pdu(iscsi, pdu);
}

int
orwrite(struct scsi_device *sdev, uint64_t lba,
	uint32_t datalen, int blocksize, int wrprotect, 
	int dpo, int fua, int fua_nv, int group,
	unsigned char *data,
	int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send ORWRITE (Expecting %s) LBA:%" PRIu64
		" blocks:%d wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_orwrite(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("ORWRITE", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
prin_task(struct scsi_device *sdev, int service_action,
    int success_expected)
{
	const int buf_sz = 16384;
	struct scsi_task *task;
	int ret = 0;


	logging(LOG_VERBOSE, "Send PRIN/SA=0x%02x, expect %s", service_action,
	    success_expected ? "success" : "failure");

	task = iscsi_persistent_reserve_in_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    service_action, buf_sz);
	if (task == NULL) {
	        logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (success_expected) {
		if (task->status != SCSI_STATUS_GOOD) {
			logging(LOG_NORMAL,
			    "[FAILED] PRIN/SA=0x%x failed: %s",
			    service_action, iscsi_get_error(sdev->iscsi_ctx));
			ret = -1;
		}
	} else {
		if (task->status == SCSI_STATUS_GOOD) {
			logging(LOG_NORMAL,
			    "[FAILED] PRIN/SA=0x%x succeeded with invalid serviceaction",
				service_action);
			ret = -1;
		}
	}

	scsi_free_scsi_task(task);
	task = NULL;

	return ret;
}

int
prin_read_keys(struct scsi_device *sdev, struct scsi_task **tp,
	struct scsi_persistent_reserve_in_read_keys **rkp)
{
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_keys *rk = NULL;


	logging(LOG_VERBOSE, "Send PRIN/READ_KEYS");

	*tp = iscsi_persistent_reserve_in_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_READ_KEYS, buf_sz);
	if (*tp == NULL) {
	        logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(*tp)) {
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}
	if ((*tp)->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}

	rk = scsi_datain_unmarshall(*tp);
	if (rk == NULL) {
		logging(LOG_NORMAL,
		    "[FAIL] failed to unmarshall PRIN/READ_KEYS data. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (rkp != NULL)
		*rkp = rk;

	return 0;
}

int
prout_register_and_ignore(struct scsi_device *sdev,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	/* register our reservation key with the target */
	logging(LOG_VERBOSE,
		"Send PROUT/REGISTER_AND_IGNORE to register init=%s",
		sdev->iscsi_ctx->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		logging(LOG_NORMAL, "[SKIPPED] PROUT Not Supported");
		ret = -2;
		goto dun;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
	}

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
prout_register_key(struct scsi_device *sdev,
    unsigned long long sark, unsigned long long rk)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	/* register/unregister our reservation key with the target */

	logging(LOG_VERBOSE, "Send PROUT/REGISTER to %s init=%s",
		sark != 0 ? "register" : "unregister",
		sdev->iscsi_ctx->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	poc.reservation_key = rk;
	task = iscsi_persistent_reserve_out_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
	}

	scsi_free_scsi_task(task);

	return ret;
}

int
prin_verify_key_presence(struct scsi_device *sdev,
    unsigned long long key, int present)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	int i;
	int key_found;
	struct scsi_persistent_reserve_in_read_keys *rk = NULL;
	int ret = 0;


	logging(LOG_VERBOSE,
		"Send PRIN/READ_KEYS to verify key %s init=%s... ",
		present ? "present" : "absent",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_persistent_reserve_in_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_READ_KEYS, buf_sz);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}

	rk = scsi_datain_unmarshall(task);
	if (rk == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] failed to unmarshall PRIN/READ_KEYS data. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}

	key_found = 0;
	for (i = 0; i < rk->num_keys; i++) {
		if (rk->keys[i] == key)
			key_found = 1;
	}

	if ((present && !key_found) || (!present && key_found)) {
		if (present)
			logging(LOG_NORMAL,
			    "[FAILED] Key found when none expected");
		else
			logging(LOG_NORMAL,
			    "[FAILED] Key not found when expected");
		ret = -1;
	}

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
prout_reregister_key_fails(struct scsi_device *sdev,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	logging(LOG_VERBOSE,
		"Send PROUT/REGISTER to ensure reregister fails init=%s",
		sdev->iscsi_ctx->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		logging(LOG_NORMAL,
		    "[FAILED] Expected RESERVATION CONFLICT");
		ret = -1;
	}

	scsi_free_scsi_task(task);
	return ret;
}

int
prout_reserve(struct scsi_device *sdev,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	/* reserve the target using specified reservation type */
	logging(LOG_VERBOSE,
		"Send PROUT/RESERVE to reserve, type=%d (%s) init=%s",
		pr_type, scsi_pr_type_str(pr_type),
		sdev->iscsi_ctx->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.reservation_key = key;
	task = iscsi_persistent_reserve_out_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_RESERVE,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    pr_type, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
	}

	scsi_free_scsi_task(task);
	return ret;
}

int
prout_release(struct scsi_device *sdev,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	logging(LOG_VERBOSE,
		"Send PROUT/RELEASE to release reservation, type=%d init=%s",
		pr_type, sdev->iscsi_ctx->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.reservation_key = key;
	task = iscsi_persistent_reserve_out_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_RELEASE,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    pr_type, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
	}

	scsi_free_scsi_task(task);
	return ret;
}

int
prin_verify_reserved_as(struct scsi_device *sdev,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_reservation *rr = NULL;
	int ret = 0;


	logging(LOG_VERBOSE,
		"Send PRIN/READ_RESERVATION to verify type=%d init=%s... ",
		pr_type, sdev->iscsi_ctx->initiator_name);

	task = iscsi_persistent_reserve_in_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_READ_RESERVATION, buf_sz);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}
	rr = scsi_datain_unmarshall(task);
	if (rr == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to unmarshall PRIN/READ_RESERVATION data. %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}
	if (!rr->reserved) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to find Target reserved as expected.");
		ret = -1;
		goto dun;
	}
	if (rr->reservation_key != key) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to find reservation key 0x%llx: found 0x%"
		    PRIu64 ".",
		    key, rr->reservation_key);
		ret = -1;
		goto dun;
	}
	if (rr->pr_type != pr_type) {
		logging(LOG_NORMAL,
		    "Failed to find reservation type %d: found %d.",
		    pr_type, rr->pr_type);
		return -1;
		ret = -1;
		goto dun;
	}

  dun:
	/* ??? free rr? */
	scsi_free_scsi_task(task);
	return ret;
}

int
prin_verify_not_reserved(struct scsi_device *sdev)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_reservation *rr = NULL;
	int ret = 0;


	logging(LOG_VERBOSE,
		"Send PRIN/READ_RESERVATION to verify not reserved init=%s",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_persistent_reserve_in_sync(sdev->iscsi_ctx, sdev->iscsi_lun,
	    SCSI_PERSISTENT_RESERVE_READ_RESERVATION, buf_sz);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}
	rr = scsi_datain_unmarshall(task);
	if (rr == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to unmarshall PRIN/READ_RESERVATION data: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}
	if (rr->reserved) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to find Target not reserved as expected.");
		ret = -1;
		goto dun;
	}

  dun:
	/* ??? free rr? */
	scsi_free_scsi_task(task);
	return ret;
}

int
verify_read_works(struct scsi_device *sdev, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;
	int ret = 0;


	/*
	 * try to read the second 512-byte block
	 */

	logging(LOG_VERBOSE, "Send READ10 to verify READ works init=%s",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_read10_sync(sdev->iscsi_ctx, sdev->iscsi_lun, lba, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send READ10 command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] READ10 command: failed with sense: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
		goto dun;
	}
	memcpy(buf, task->datain.data, task->datain.size);

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
verify_write_works(struct scsi_device *sdev, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;
	int ret = 0;


	/*
	 * try to write the second 512-byte block
	 */

	logging(LOG_VERBOSE, "Send WRITE10 to verify WRITE works init=%s",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_write10_sync(sdev->iscsi_ctx, sdev->iscsi_lun, lba, buf, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send WRITE10 command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] WRITE10 command: failed with sense: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		ret = -1;
	}
	scsi_free_scsi_task(task);
	return ret;
}

int
verify_read_fails(struct scsi_device *sdev, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;
	int ret = 0;


	/*
	 * try to read the second 512-byte block -- should fail
	 */

	logging(LOG_VERBOSE,
		"Send READ10 to verify READ does not work init=%s",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_read10_sync(sdev->iscsi_ctx, sdev->iscsi_lun, lba, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send READ10 command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}

	if (task->status == SCSI_STATUS_GOOD) {
		memcpy(buf, task->datain.data, task->datain.size);
		logging(LOG_NORMAL,
		    "[FAILED] READ10 command succeeded when expected to fail");
		ret = -1;
		goto dun;
	}

	/*
	 * XXX should we verify sense data?
	 */

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
verify_write_fails(struct scsi_device *sdev, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;
	int ret = 0;


	/*
	 * try to write the second 512-byte block
	 */

	logging(LOG_VERBOSE,
		"Send WRITE10 to verify WRITE does not work init=%s",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_write10_sync(sdev->iscsi_ctx, sdev->iscsi_lun, lba, buf, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send WRITE10 command: %s",
		    iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}

	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] WRITE10 command: succeeded when exptec to fail");
		ret = -1;
		goto dun;
	}

	/*
	 * XXX should we verify sense data?
	 */

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
synchronizecache10(struct scsi_device *sdev, uint32_t lba, int num, int sync_nv, int immed, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send SYNCHRONIZECACHE10 (Expecting %s) LBA:%d"
		" blocks:%d sync_nv:%d immed:%d",
		scsi_status_str(status),
		lba, num, sync_nv, immed);

	task = scsi_cdb_synchronizecache10(lba, num_blocks, sync_nv, immed);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("SYNCHRONIZECACHE10", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
synchronizecache16(struct scsi_device *sdev, uint64_t lba, int num, int sync_nv, int immed, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send SYNCHRONIZECACHE16 (Expecting %s) LBA:%"
		PRIu64 " blocks:%d sync_nv:%d immed:%d",
		scsi_status_str(status),
		lba, num, sync_nv, immed);

	task = scsi_cdb_synchronizecache16(lba, num_blocks, sync_nv, immed);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("SYNCHRONIZECACHE16", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int sanitize(struct scsi_device *sdev, int immed, int ause, int sa, int param_len, struct iscsi_data *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send SANITIZE (Expecting %s) IMMED:%d AUSE:%d "
		"SA:%d PARAM_LEN:%d",
		scsi_status_str(status),
		immed, ause, sa, param_len);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping sanitize\n");
		return -1;
	}

	task = iscsi_sanitize_sync(sdev->iscsi_ctx, sdev->iscsi_lun, immed, ause, sa, param_len,
				   data);
	task = scsi_cdb_sanitize(immed, ause, sa, param_len);

	assert(task != NULL);

	task = send_scsi_command(sdev, task, data);

	ret = check_result("SANITIZE", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int startstopunit(struct scsi_device *sdev, int immed, int pcm, int pc, int no_flush, int loej, int start, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send STARTSTOPUNIT (Expecting %s) IMMED:%d "
		"PCM:%d PC:%d NO_FLUSH:%d LOEJ:%d START:%d",
		scsi_status_str(status),
		immed, pcm, pc, no_flush, loej, start);

	task = scsi_cdb_startstopunit(immed, pcm, pc, no_flush, loej, start);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("STARTSTOPUNIT", sdev, task, status, key, ascq,
			   num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
testunitready(struct scsi_device *sdev, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send TESTUNITREADY (Expecting %s)",
		scsi_status_str(status));

	task = scsi_cdb_testunitready();
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("TESTUNITREADY", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
testunitready_clear_ua(struct scsi_device *sdev)
{
	struct scsi_task *task;
	int ret = -1;

	logging(LOG_VERBOSE,
		"Send TESTUNITREADY (To Clear Possible UA) init=%s",
		sdev->iscsi_ctx->initiator_name);

	task = iscsi_testunitready_sync(sdev->iscsi_ctx, sdev->iscsi_lun);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send TESTUNITREADY command: %s",
			iscsi_get_error(sdev->iscsi_ctx));
		goto out;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[INFO] TESTUNITREADY command: failed with sense. %s",
			iscsi_get_error(sdev->iscsi_ctx));
		goto out;
	}
	logging(LOG_VERBOSE, "[OK] TESTUNITREADY does not return unit "
		"attention.");
	ret = 0;

out:
	scsi_free_scsi_task(task);
	return ret;
}

/*
 * Returns -1 if allocating a SCSI task failed or if a communication error
 * occurred and a SCSI status if a SCSI response has been received.
 */
int modesense6(struct scsi_device *sdev, struct scsi_task **out_task, int dbd, enum scsi_modesense_page_control pc, enum scsi_modesense_page_code page_code, int sub_page_code, unsigned char alloc_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send MODESENSE6 (Expecting %s) ",
		scsi_status_str(status));

	task = scsi_cdb_modesense6(dbd, pc, page_code, sub_page_code, alloc_len);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("MODESENSE6", sdev, task, status, key, ascq, num_ascq);
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int modeselect6(struct scsi_device *sdev, int pf, int sp, struct scsi_mode_page *mp, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;
	struct scsi_data *data;
	struct iscsi_data d;

	logging(LOG_VERBOSE, "Send MODESELECT6 (Expecting %s) ",
		scsi_status_str(status));

	task = scsi_cdb_modeselect6(pf, sp, 255);
	assert(task != NULL);

	data = scsi_modesense_dataout_marshall(task, mp, 1);
	if (data == NULL) {
		logging(LOG_VERBOSE, "Failed to marshall MODESELECT6 data");
		scsi_free_scsi_task(task);
		return -1;
	}

	d.data = data->data;
	d.size = data->size;
	task->cdb[4] = data->size;
	task->expxferlen = data->size;

	task = send_scsi_command(sdev, task, &d);

	ret = check_result("MODESELECT6", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int compareandwrite(struct scsi_device *sdev, uint64_t lba,
    unsigned char *data, uint32_t datalen, int blocksize,
    int wrprotect, int dpo,
    int fua, int group_number,
    int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send COMPAREANDWRITE (Expecting %s) LBA:%"
		PRIu64 " LEN:%d WRPROTECT:%d",
		scsi_status_str(status),
		lba, datalen, wrprotect);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_compareandwrite(lba, datalen, blocksize, wrprotect,
					dpo, fua, 0, group_number);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("COMPAREANDWRITE", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int get_lba_status(struct scsi_device *sdev, struct scsi_task **out_task, uint64_t lba, uint32_t len, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send GET_LBA_STATUS (Expecting %s) LBA:%" PRIu64
		" alloc_len:%d",
		scsi_status_str(status),
		lba, len);

	task = scsi_cdb_get_lba_status(lba, len);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("GET_LBA_STATUS", sdev, task, status, key, ascq, num_ascq);
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
prefetch10(struct scsi_device *sdev, uint32_t lba, int num, int immed, int group, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send PREFETCH10 (Expecting %s) LBA:%d blocks:%d"
		" immed:%d group:%d",
		scsi_status_str(status),
		lba, num, immed, group);

	task = scsi_cdb_prefetch10(lba, num, immed, group);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("PREFETCH10", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
prefetch16(struct scsi_device *sdev, uint64_t lba, int num, int immed, int group, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send PREFETCH16 (Expecting %s) LBA:%" PRIu64
		" blocks:%d immed:%d group:%d",
		scsi_status_str(status),
		lba, num, immed, group);

	task = scsi_cdb_prefetch16(lba, num, immed, group);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("PREFETCH16", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
preventallow(struct scsi_device *sdev, int prevent)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREVENTALLOW prevent:%d", prevent);
	task = iscsi_preventallow_sync(sdev->iscsi_ctx, sdev->iscsi_lun, prevent);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREVENTALLOW "
			"command: %s", iscsi_get_error(sdev->iscsi_ctx));
		return -1;
	}
	if (status_is_invalid_opcode(task)) {
		logging(LOG_NORMAL, "[SKIPPED] PREVENTALLOW is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREVENTALLOW command: "
			"failed with sense. %s", iscsi_get_error(sdev->iscsi_ctx));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREVENTALLOW returned SUCCESS.");
	return 0;
}

int
read6(struct scsi_device *sdev, struct scsi_task **out_task, uint32_t lba,
      uint32_t datalen, int blocksize,
      unsigned char *data,
      int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send READ6 (Expecting %s) LBA:%d blocks:%d",
		scsi_status_str(status),
		lba, datalen / blocksize);

	task = scsi_cdb_read6(lba, datalen, blocksize);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("READ6", sdev, task, status, key, ascq, num_ascq);
	if (data && task) {
		memcpy(data, task->datain.data, task->datain.size);
	}
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
read10(struct scsi_device *sdev, struct scsi_task **out_task,
       uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data,
       int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send READ10 (Expecting %s) LBA:%d"
		" blocks:%d rdprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = scsi_cdb_read10(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("READ10", sdev, task, status, key, ascq, num_ascq);
	if (data && task) {
		memcpy(data, task->datain.data, task->datain.size);
	}
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
read12(struct scsi_device *sdev, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data,
       int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send READ12 (Expecting %s) LBA:%d"
		" blocks:%d rdprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = scsi_cdb_read12(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("READ12", sdev, task, status, key, ascq, num_ascq);
	if (data && task) {
		memcpy(data, task->datain.data, task->datain.size);
	}
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
read16(struct scsi_device *sdev, uint64_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data,
       int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send READ16 (Expecting %s) LBA:%" PRIu64
		" blocks:%d rdprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = scsi_cdb_read16(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("READ16", sdev, task, status, key, ascq, num_ascq);
	if (data && task) {
		memcpy(data, task->datain.data, task->datain.size);
	}
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
readcapacity10(struct scsi_device *sdev, struct scsi_task **out_task, uint32_t lba, int pmi, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send READCAPACITY10 (Expecting %s) LBA:%d"
		" pmi:%d",
		scsi_status_str(status),
		lba, pmi);

	task = scsi_cdb_readcapacity10(lba, pmi);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("READCAPACITY10", sdev, task, status, key, ascq, num_ascq);
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
readcapacity16(struct scsi_device *sdev, struct scsi_task **out_task, int alloc_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send READCAPACITY16 (Expecting %s)",
		scsi_status_str(status));

	task = scsi_cdb_serviceactionin16(SCSI_READCAPACITY16, alloc_len);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("READCAPACITY16", sdev, task, status, key, ascq, num_ascq);
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
release6(struct scsi_device *sdev)
{
	struct scsi_task *task;
	int i, res = 0;

	logging(LOG_VERBOSE, "Send RELEASE6");

	for (i = 0; i < 3 && res == 0; ++i) {
		task = iscsi_release6_sync(sdev->iscsi_ctx, sdev->iscsi_lun);
		if (task == NULL) {
			logging(LOG_NORMAL,
				"[FAILED] Failed to send RELEASE6 command: %s",
				iscsi_get_error(sdev->iscsi_ctx));
			res = -1;
			break;
		}
		if (task->status != SCSI_STATUS_GOOD &&
		    !(task->status        == SCSI_STATUS_CHECK_CONDITION
		      && task->sense.key  == SCSI_SENSE_UNIT_ATTENTION
		      && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET)) {
			logging(LOG_NORMAL, "[FAILED] RELEASE6 command: "
				"failed with sense. %s",
				iscsi_get_error(sdev->iscsi_ctx));
			res = -1;
		}
		scsi_free_scsi_task(task);
	}

	if (res == 0)
		logging(LOG_VERBOSE, "[OK] RELEASE6 returned SUCCESS.");
	return res;
}

int report_supported_opcodes(struct scsi_device *sdev, struct scsi_task **out_task, int rctd, int options, int opcode, int sa, int alloc_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send REPORT_SUPPORTED_OPCODE (Expecting %s) "
		"RCTD:%d OPTIONS:%d OPCODE:0x%02x SA:%d ALLOC_LEN:%d",
		scsi_status_str(status),
		rctd, options, opcode, sa, alloc_len);

	task = scsi_cdb_report_supported_opcodes(rctd, options, opcode, sa,
						 alloc_len);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("REPORT_SUPPORTED_OPCODES", sdev, task, status, key, ascq, num_ascq);
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
reserve6(struct scsi_device *sdev)
{
	struct scsi_task *task;
	int i, res = 0;

	logging(LOG_VERBOSE, "Send RESERVE6");

	for (i = 0; i < 3 && res == 0; ++i) {
		task = iscsi_reserve6_sync(sdev->iscsi_ctx, sdev->iscsi_lun);
		if (task == NULL) {
			logging(LOG_NORMAL,
				"[FAILED] Failed to send RESERVE6 command: %s",
				iscsi_get_error(sdev->iscsi_ctx));
			res = -1;
			break;
		}
		if (status_is_invalid_opcode(task)) {
			logging(LOG_NORMAL, "[SKIPPED] RESERVE6 is not "
				"implemented on target");
			res = -2;
		} else if (task->status != SCSI_STATUS_GOOD &&
		    !(task->status        == SCSI_STATUS_CHECK_CONDITION
		      && task->sense.key  == SCSI_SENSE_UNIT_ATTENTION
		      && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET)) {
			logging(LOG_NORMAL, "[FAILED] RESERVE6 command: "
				"failed with sense. %s",
				iscsi_get_error(sdev->iscsi_ctx));
			res = -1;
		}
		scsi_free_scsi_task(task);
	}

	if (res == 0)
		logging(LOG_VERBOSE, "[OK] RESERVE6 returned SUCCESS.");
	return res;
}

int
reserve6_conflict(struct scsi_device *sdev)
{
	struct scsi_task *task;
	int i, res = 0;

	logging(LOG_VERBOSE, "Send RESERVE6 (Expecting RESERVATION_CONFLICT)");

	for (i = 0; i < 3 && res == 0; ++i) {
		task = iscsi_reserve6_sync(sdev->iscsi_ctx, sdev->iscsi_lun);
		if (task == NULL) {
			logging(LOG_NORMAL,
				"[FAILED] Failed to send RESERVE6 command: %s",
				iscsi_get_error(sdev->iscsi_ctx));
			res = -1;
			break;
		}
		if (status_is_invalid_opcode(task)) {
			logging(LOG_NORMAL, "[SKIPPED] RESERVE6 is not"
				" implemented on target");
			res = -2;
		} else if (task->status != SCSI_STATUS_RESERVATION_CONFLICT &&
		    !(task->status        == SCSI_STATUS_CHECK_CONDITION
		      && task->sense.key  == SCSI_SENSE_UNIT_ATTENTION
		      && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET)) {
			logging(LOG_NORMAL, "[FAILED] RESERVE6 command: "
				"should have failed with RESERVATION_CONFLICT");
			res = -1;
		}
		scsi_free_scsi_task(task);
	}

	if (res == 0)
		logging(LOG_VERBOSE,
			"[OK] RESERVE6 returned RESERVATION_CONFLICT.");
	return res;
}

int
unmap(struct scsi_device *sdev, int anchor, struct unmap_list *list, int list_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	unsigned char *data;
	struct iscsi_data d;
	int xferlen;
	int i;
	int ret;

	logging(LOG_VERBOSE, "Send UNMAP (Expecting %s) list_len:%d anchor:%d",
		scsi_status_str(status),
		list_len, anchor);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping unmap\n");
		return -1;
	}

	xferlen = 8 + list_len * 16;

	task = scsi_cdb_unmap(anchor, 0, xferlen);
	assert(task != NULL);

	data = scsi_malloc(task, xferlen);
	if (data == NULL) {
		logging(LOG_NORMAL, "Out-of-memory: Failed to create "
			"unmap parameters.");
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_set_uint16(&data[0], xferlen - 2);
	scsi_set_uint16(&data[2], xferlen - 8);
	for (i = 0; i < list_len; i++) {
		scsi_set_uint32(&data[8 + 16 * i], list[i].lba >> 32);
		scsi_set_uint32(&data[8 + 16 * i + 4], list[i].lba & 0xffffffff);
		scsi_set_uint32(&data[8 + 16 * i + 8], list[i].num);
	}

	d.data = data;
	d.size = xferlen;

	task = send_scsi_command(sdev, task, &d);

	ret = check_result("UNMAP", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
verify10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)

{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send VERIFY10 (Expecting %s) LBA:%d "
		"blocks:%d vprotect:%d dpo:%d bytchk:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = scsi_cdb_verify10(lba, datalen, vprotect, dpo, bytchk, blocksize);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("VERIFY10", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
verify12(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send VERIFY12 (Expecting %s) LBA:%d "
		"blocks:%d vprotect:%d dpo:%d bytchk:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = scsi_cdb_verify12(lba, datalen, vprotect, dpo, bytchk, blocksize);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("VERIFY12", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
verify16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send VERIFY16 (Expecting %s) LBA:%" PRIu64
		" blocks:%d vprotect:%d dpo:%d bytchk:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = scsi_cdb_verify16(lba, datalen, vprotect, dpo, bytchk, blocksize);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("VERIFY16", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
write10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITE10 (Expecting %s) LBA:%d blocks:%d "
	       "wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_write10(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITE10", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
write12(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITE12 (Expecting %s) LBA:%d blocks:%d "
	       "wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_write12(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITE12", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
write16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITE16 (Expecting %s) LBA:%" PRIu64
		" blocks:%d wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_write16(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITE16", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
writesame10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITESAME10 (Expecting %s) LBA:%d blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		scsi_status_str(status),
		lba, num, wrprotect, anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_writesame10(wrprotect, anchor, unmap_flag, lba, group,
				    num, datalen);
	assert(task != NULL);

	if (data != NULL) {
		task->expxferlen = datalen;
	} else {
		task->expxferlen = 0;
		task->xfer_dir = SCSI_XFER_NONE;
	}

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITESAME10", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
writesame16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITESAME16 (Expecting %s) LBA:%" PRIu64
		" blocks:%d wrprotect:%d anchor:%d unmap:%d group:%d",
		scsi_status_str(status),
		lba, num, wrprotect, anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_writesame16(wrprotect, anchor, unmap_flag, lba, group,
				    num, datalen);
	assert(task != NULL);

	if (data != NULL) {
		task->expxferlen = datalen;
	} else {
		task->expxferlen = 0;
		task->xfer_dir = SCSI_XFER_NONE;
	}

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITESAME16", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
writeverify10(struct scsi_device *sdev, uint32_t lba,
	      uint32_t datalen, int blocksize, int wrprotect, 
	      int dpo, int bytchk, int group, unsigned char *data,
	      int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITEVERIFY10 (Expecting %s) LBA:%d "
		"blocks:%d wrprotect:%d dpo:%d bytchk:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_writeverify10(lba, datalen, blocksize, wrprotect,
				      dpo, bytchk, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITEVERIFY10", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
writeverify12(struct scsi_device *sdev, uint32_t lba,
	      uint32_t datalen, int blocksize, int wrprotect, 
	      int dpo, int bytchk, int group, unsigned char *data,
	      int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITEVERIFY12 (Expecting %s) LBA:%d "
		"blocks:%d wrprotect:%d dpo:%d bytchk:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_writeverify12(lba, datalen, blocksize, wrprotect,
				      dpo, bytchk, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITEVERIFY12", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
writeverify16(struct scsi_device *sdev, uint64_t lba,
	      uint32_t datalen, int blocksize, int wrprotect, 
	      int dpo, int bytchk, int group, unsigned char *data,
	      int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	struct iscsi_data d;
	int ret;

	logging(LOG_VERBOSE, "Send WRITEVERIFY16 (Expecting %s) LBA:%" PRIu64
		" blocks:%d wrprotect:%d dpo:%d bytchk:%d group:%d",
		scsi_status_str(status),
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = scsi_cdb_writeverify16(lba, datalen, blocksize, wrprotect,
				      dpo, bytchk, group);
	assert(task != NULL);

	d.data = data;
	d.size = datalen;
	task = send_scsi_command(sdev, task, &d);

	ret = check_result("WRITEVERIFY16", sdev, task, status, key, ascq, num_ascq);
	if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

int
inquiry(struct scsi_device *sdev, struct scsi_task **out_task, int evpd, int page_code, int maxsize, int status, enum scsi_sense_key key, int *ascq, int num_ascq)
{
	struct scsi_task *task;
	int ret;

	logging(LOG_VERBOSE, "Send INQUIRY (Expecting %s) evpd:%d "
		"page_code:%02x alloc_len:%d",
		scsi_status_str(status),
		evpd, page_code, maxsize);

	task = scsi_cdb_inquiry(evpd, page_code, maxsize);
	assert(task != NULL);

	task = send_scsi_command(sdev, task, NULL);

	ret = check_result("INQUIRY", sdev, task, status, key, ascq, num_ascq);
	if (out_task) {
		*out_task = task;
	} else if (task) {
		scsi_free_scsi_task(task);
	}
	return ret;
}

struct scsi_command_descriptor *
get_command_descriptor(int opcode, int sa)
{
	int i;

	if (rsop == NULL) {
		return NULL;
	}

	for (i = 0; i < rsop->num_descriptors; i++) {
		if (rsop->descriptors[i].opcode == opcode
		&& rsop->descriptors[i].sa == sa) {
			return &rsop->descriptors[i];
		}
	}
	
	return NULL;
}

int set_swp(struct scsi_device *sdev)
{
	int ret;
	struct scsi_task *sense_task = NULL;
	struct scsi_mode_sense *ms;
	struct scsi_mode_page *mp;

	logging(LOG_VERBOSE, "Read CONTROL page");
	ret = modesense6(sdev, &sense_task, 1, SCSI_MODESENSE_PC_CURRENT,
			 SCSI_MODEPAGE_CONTROL, 0, 255,
			 EXPECT_STATUS_GOOD);
	if (ret) {
		logging(LOG_NORMAL, "Failed to read CONTROL mode page.");
		goto finished;
	}
	logging(LOG_VERBOSE, "[SUCCESS] CONTROL page fetched.");

	ms = scsi_datain_unmarshall(sense_task);
	if (ms == NULL) {
		logging(LOG_NORMAL, "failed to unmarshall mode sense datain "
			"blob");
		ret = -1;
		goto finished;
	}
	mp = scsi_modesense_get_page(ms, SCSI_MODEPAGE_CONTROL, 0);
	if (mp == NULL) {
		logging(LOG_NORMAL, "failed to read control mode page");
		ret = -1;
		goto finished;
	}

	/* For MODE SELECT PS is reserved and hence must be cleared */
	mp->ps = 0;

	logging(LOG_VERBOSE, "Turn SWP ON");
	mp->control.swp = 1;

	ret = modeselect6(sdev, 1, 0, mp,
			 EXPECT_STATUS_GOOD);
	if (ret) {
		logging(LOG_NORMAL, "Failed to write CONTROL mode page.");
		goto finished;
	}
	logging(LOG_VERBOSE, "[SUCCESS] CONTROL page written.");

finished:
	if (sense_task != NULL) {
		scsi_free_scsi_task(sense_task);
	}
	return ret;
}

int clear_swp(struct scsi_device *sdev)
{
	int ret;
	struct scsi_task *sense_task = NULL;
	struct scsi_mode_sense *ms;
	struct scsi_mode_page *mp;

	logging(LOG_VERBOSE, "Read CONTROL page");
	ret = modesense6(sdev, &sense_task, 1, SCSI_MODESENSE_PC_CURRENT,
			 SCSI_MODEPAGE_CONTROL, 0, 255,
			 EXPECT_STATUS_GOOD);
	if (ret) {
		logging(LOG_NORMAL, "Failed to read CONTROL mode page.");
		goto finished;
	}
	logging(LOG_VERBOSE, "[SUCCESS] CONTROL page fetched.");

	ms = scsi_datain_unmarshall(sense_task);
	if (ms == NULL) {
		logging(LOG_NORMAL, "failed to unmarshall mode sense datain "
			"blob");
		ret = -1;
		goto finished;
	}
	mp = scsi_modesense_get_page(ms, SCSI_MODEPAGE_CONTROL, 0);
	if (mp == NULL) {
		logging(LOG_NORMAL, "failed to read control mode page");
		ret = -1;
		goto finished;
	}

	/* For MODE SELECT PS is reserved and hence must be cleared */
	mp->ps = 0;

	logging(LOG_VERBOSE, "Turn SWP OFF");
	mp->control.swp = 0;

	ret = modeselect6(sdev, 1, 0, mp,
			 EXPECT_STATUS_GOOD);
	if (ret) {
		logging(LOG_NORMAL, "Failed to write CONTROL mode page.");
		goto finished;
	}
	logging(LOG_VERBOSE, "[SUCCESS] CONTROL page written.");

finished:
	if (sense_task != NULL) {
		scsi_free_scsi_task(sense_task);
	}
	return ret;
}
