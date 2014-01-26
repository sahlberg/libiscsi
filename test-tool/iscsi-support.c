/*
   iscsi-test tool support

   Copyright (C) 2012 by Lee Duncan <leeman.duncan@gmail.com>

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

#define _GNU_SOURCE
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

const char *tgt_url;

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
					SLIST_REMOVE(&iscsi->outqueue, pdu);
				}
				while ((pdu = iscsi->waitpdu)) {
					SLIST_REMOVE(&iscsi->waitpdu, pdu);
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
orwrite(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send ORWRITE LBA:%" PRIu64 " blocks:%d "
	       "wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_orwrite_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send ORWRITE command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] ORWRITE is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] ORWRITE returned SUCCESS.");
	return 0;
}

int
orwrite_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send ORWRITE (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_orwrite_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send ORWRITE command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] ORWRITE is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] ORWRITE returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
orwrite_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send ORWRITE (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_orwrite_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send ORWRITE command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] ORWRITE is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] ORWRITE returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
orwrite_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send ORWRITE (Expecting WRITE_PROTECTED) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_orwrite_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send ORWRITE command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] ORWRITE is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] ORWRITE returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
orwrite_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send ORWRITE (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_orwrite_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send ORWRITE command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] ORWRITE is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] ORWRITE command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] ORWRITE Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] ORWRITE returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
prin_task(struct iscsi_context *iscsi, int lun, int service_action,
    int success_expected)
{
	const int buf_sz = 16384;
	struct scsi_task *task;
	int ret = 0;


	logging(LOG_VERBOSE, "Send PRIN/SA=0x%02x, expect %s", service_action,
	    success_expected ? "success" : "failure");

	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    service_action, buf_sz);
	if (task == NULL) {
	        logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (success_expected) {
		if (task->status != SCSI_STATUS_GOOD) {
			logging(LOG_NORMAL,
			    "[FAILED] PRIN/SA=0x%x failed: %s",
			    service_action, iscsi_get_error(iscsi));
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
prin_read_keys(struct iscsi_context *iscsi, int lun, struct scsi_task **tp,
	struct scsi_persistent_reserve_in_read_keys **rkp)
{
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_keys *rk = NULL;


	logging(LOG_VERBOSE, "Send PRIN/READ_KEYS");

	*tp = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_KEYS, buf_sz);
	if (*tp == NULL) {
	        logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if ((*tp)->status        == SCSI_STATUS_CHECK_CONDITION
	    && (*tp)->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && (*tp)->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}
	if ((*tp)->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense. %s",
		    iscsi_get_error(iscsi));
		return -1;
	}

	rk = scsi_datain_unmarshall(*tp);
	if (rk == NULL) {
		logging(LOG_NORMAL,
		    "[FAIL] failed to unmarshall PRIN/READ_KEYS data. %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (rkp != NULL)
		*rkp = rk;

	return 0;
}

int
prout_register_and_ignore(struct iscsi_context *iscsi, int lun,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	/* register our reservation key with the target */
	logging(LOG_VERBOSE,
	    "Send PROUT/REGISTER_AND_IGNORE to register init=%s",
	    iscsi->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION &&
	    task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST &&
	    task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PROUT Not Supported");
		ret = -2;
		goto dun;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense. %s",
		    iscsi_get_error(iscsi));
		ret = -1;
	}

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
prout_register_key(struct iscsi_context *iscsi, int lun,
    unsigned long long sark, unsigned long long rk)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	/* register/unregister our reservation key with the target */

	logging(LOG_VERBOSE, "Send PROUT/REGISTER to %s init=%s",
	    sark != 0 ? "register" : "unregister",
	    iscsi->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	poc.reservation_key = rk;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense: %s",
		    iscsi_get_error(iscsi));
		ret = -1;
	}

	scsi_free_scsi_task(task);

	return ret;
}

int
prin_verify_key_presence(struct iscsi_context *iscsi, int lun,
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
	    iscsi->initiator_name);

	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_KEYS, buf_sz);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense. %s",
		    iscsi_get_error(iscsi));
		ret = -1;
		goto dun;
	}

	rk = scsi_datain_unmarshall(task);
	if (rk == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] failed to unmarshall PRIN/READ_KEYS data. %s",
		    iscsi_get_error(iscsi));
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
prout_reregister_key_fails(struct iscsi_context *iscsi, int lun,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	logging(LOG_VERBOSE,
	    "Send PROUT/REGISTER to ensure reregister fails init=%s",
	    iscsi->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
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
prout_reserve(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	/* reserve the target using specified reservation type */
	logging(LOG_VERBOSE,
	    "Send PROUT/RESERVE to reserve, type=%d (%s) init=%s",
	    pr_type, scsi_pr_type_str(pr_type),
	    iscsi->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.reservation_key = key;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_RESERVE,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    pr_type, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense. %s",
		    iscsi_get_error(iscsi));
		ret = -1;
	}

	scsi_free_scsi_task(task);
	return ret;
}

int
prout_release(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;
	int ret = 0;


	logging(LOG_VERBOSE,
	    "Send PROUT/RELEASE to release reservation, type=%d init=%s",
	    pr_type, iscsi->initiator_name);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping PROUT\n");
		return -1;
	}

	memset(&poc, 0, sizeof (poc));
	poc.reservation_key = key;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_RELEASE,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    pr_type, &poc);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PROUT command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE OUT is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PROUT command: failed with sense. %s",
		    iscsi_get_error(iscsi));
		ret = -1;
	}

	scsi_free_scsi_task(task);
	return ret;
}

int
prin_verify_reserved_as(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_reservation *rr = NULL;
	int ret = 0;


	logging(LOG_VERBOSE,
	    "Send PRIN/READ_RESERVATION to verify type=%d init=%s... ",
	    pr_type, iscsi->initiator_name);

	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_RESERVATION, buf_sz);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense: %s",
		    iscsi_get_error(iscsi));
		ret = -1;
		goto dun;
	}
	rr = scsi_datain_unmarshall(task);
	if (rr == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to unmarshall PRIN/READ_RESERVATION data. %s",
		    iscsi_get_error(iscsi));
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
prin_verify_not_reserved(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_reservation *rr = NULL;
	int ret = 0;


	logging(LOG_VERBOSE,
	    "Send PRIN/READ_RESERVATION to verify not reserved init=%s",
	    iscsi->initiator_name);

	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_RESERVATION, buf_sz);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send PRIN command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] PERSISTENT RESERVE IN is not implemented.");
		return -2;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] PRIN command: failed with sense: %s",
		    iscsi_get_error(iscsi));
		ret = -1;
		goto dun;
	}
	rr = scsi_datain_unmarshall(task);
	if (rr == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to unmarshall PRIN/READ_RESERVATION data: %s",
		    iscsi_get_error(iscsi));
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
verify_read_works(struct iscsi_context *iscsi, int lun, unsigned char *buf)
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
	    iscsi->initiator_name);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send READ10 command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}

	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] READ10 command: failed with sense: %s",
		    iscsi_get_error(iscsi));
		ret = -1;
		goto dun;
	}
	memcpy(buf, task->datain.data, task->datain.size);

  dun:
	scsi_free_scsi_task(task);
	return ret;
}

int
verify_write_works(struct iscsi_context *iscsi, int lun, unsigned char *buf)
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
	    iscsi->initiator_name);

	task = iscsi_write10_sync(iscsi, lun, lba, buf, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send WRITE10 command: %s",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
		    "[FAILED] WRITE10 command: failed with sense: %s",
		    iscsi_get_error(iscsi));
		ret = -1;
	}
	scsi_free_scsi_task(task);
	return ret;
}

int
verify_read_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf)
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
	    iscsi->initiator_name);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send READ10 command: %s",
		    iscsi_get_error(iscsi));
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
verify_write_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf)
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
	    iscsi->initiator_name);

	task = iscsi_write10_sync(iscsi, lun, lba, buf, datalen, blksize,
	    0, 0, 0, 0, 0);
	if (task == NULL) {
		logging(LOG_NORMAL,
		    "[FAILED] Failed to send WRITE10 command: %s",
		    iscsi_get_error(iscsi));
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
synchronizecache10(struct iscsi_context *iscsi, int lun, uint32_t lba, int num, int sync_nv, int immed)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SYNCHRONIZECACHE10 LBA:%d blocks:%d"
		" sync_nv:%d immed:%d",
		lba, num, sync_nv, immed);

	task = iscsi_synchronizecache10_sync(iscsi, lun, lba, num,
					     sync_nv, immed);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send SYNCHRONIZECAHCE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] SYNCHRONIZECAHCE10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] SYNCHRONIZECACHE10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SYNCHRONIZECAHCE10 returned SUCCESS.");
	return 0;
}

int
synchronizecache10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, int num, int sync_nv, int immed)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SYNCHRONIZECACHE10 (Expecting MEDIUM_NOT_PRESENT) LBA:%d blocks:%d"
		" sync_nv:%d immed:%d",
		lba, num, sync_nv, immed);

	task = iscsi_synchronizecache10_sync(iscsi, lun, lba, num,
					     sync_nv, immed);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send SYNCHRONIZECAHCE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] SYNCHRONIZECAHCE10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] SYNCHRONIZECACHE10 command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] SYNCHRONIZECAHCE10 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SYNCHRONIZECAHCE10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
synchronizecache16(struct iscsi_context *iscsi, int lun, uint64_t lba, int num, int sync_nv, int immed)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SYNCHRONIZECACHE16 LBA:%" PRIu64 " blocks:%d"
		" sync_nv:%d immed:%d",
		lba, num, sync_nv, immed);

	task = iscsi_synchronizecache16_sync(iscsi, lun, lba, num,
					     sync_nv, immed);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send SYNCHRONIZECAHCE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] SYNCHRONIZECAHCE16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] SYNCHRONIZECACHE16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SYNCHRONIZECAHCE16 returned SUCCESS.");
	return 0;
}

int
synchronizecache16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, int num, int sync_nv, int immed)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SYNCHRONIZECACHE16 (Expecting MEDIUM_NOT_PRESENT) LBA:%" PRIu64 " blocks:%d"
		" sync_nv:%d immed:%d",
		lba, num, sync_nv, immed);

	task = iscsi_synchronizecache16_sync(iscsi, lun, lba, num,
					     sync_nv, immed);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send SYNCHRONIZECAHCE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] SYNCHRONIZECAHCE16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] SYNCHRONIZECACHE16 command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] SYNCHRONIZECACHE16 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SYNCHRONIZECAHCE16 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int sanitize(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SANITIZE IMMED:%d AUSE:%d SA:%d "
		"PARAM_LEN:%d",
		immed, ause, sa, param_len);

	task = iscsi_sanitize_sync(iscsi, lun, immed, ause, sa, param_len,
				   data);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send SANITIZE command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] SANITIZE command: failed with sense. %s",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SANITIZE returned SUCCESS.");
	return 0;
}

int sanitize_invalidfieldincdb(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SANITIZE (Expecting INVALID_FIELD_IN_CDB) "
		"IMMED:%d AUSE:%d SA:%d "
		"PARAM_LEN:%d",
		immed, ause, sa, param_len);

	task = iscsi_sanitize_sync(iscsi, lun, immed, ause, sa, param_len,
				   data);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send SANITIZE command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] SANITIZE successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] SANITIZE failed with wrong "
			"sense. Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SANITIZE returned ILLEGAL_REQUEST/"
		"INVALID_FIELD_IB_CDB.");
	return 0;
}

int sanitize_conflict(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SANITIZE (Expecting RESERVATION_CONFLICT) "
		"IMMED:%d AUSE:%d SA:%d "
		"PARAM_LEN:%d",
		immed, ause, sa, param_len);

	task = iscsi_sanitize_sync(iscsi, lun, immed, ause, sa, param_len,
				   data);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send SANITIZE command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] SANITIZE successful but should have failed with RESERVATION_CONFLICT");
		scsi_free_scsi_task(task);
		return -1;
	}

	if (task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		logging(LOG_NORMAL, "[FAILED] Expected RESERVATION CONFLICT. "
			"Sense:%s", iscsi_get_error(iscsi));
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SANITIZE returned RESERVATION_CONFLICT.");
	return 0;
}

int sanitize_writeprotected(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send SANITIZE (Expecting WRITE_PROTECTED) "
		"IMMED:%d AUSE:%d SA:%d "
		"PARAM_LEN:%d",
		immed, ause, sa, param_len);

	task = iscsi_sanitize_sync(iscsi, lun, immed, ause, sa, param_len,
				   data);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send SANITIZE command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] SANITIZE successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] SANITIZE failed with wrong "
			"sense. Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] SANITIZE returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int startstopunit(struct iscsi_context *iscsi, int lun, int immed, int pcm, int pc, int no_flush, int loej, int start)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send STARTSTOPUNIT IMMED:%d PCM:%d PC:%d NO_FLUSH:%d LOEJ:%d START:%d", immed, pcm, pc, no_flush, loej, start);
	task = iscsi_startstopunit_sync(iscsi, lun, immed, pcm, pc, no_flush,
					loej, start);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send STARTSTOPUNIT command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] STARTSTOPUNIT command: failed with sense. %s",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] STARTSTOPUNIT returned SUCCESS.");
	return 0;
}

int startstopunit_preventremoval(struct iscsi_context *iscsi, int lun, int immed, int pcm, int pc, int no_flush, int loej, int start)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send STARTSTOPUNIT (Expecting MEDIUM_REMOVAL_PREVENTED) "
		"IMMED:%d PCM:%d PC:%d NO_FLUSH:%d LOEJ:%d START:%d",
		immed, pcm, pc, no_flush, loej, start);

	task = iscsi_startstopunit_sync(iscsi, lun, immed, pcm, pc, no_flush,
					loej, start);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send STARTSTOPUNIT command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] STARTSTOPUNIT successful but should have failed with ILLEGAL_REQUEST/MEDIUM_REMOVAL_PREVENTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED) {
		logging(LOG_NORMAL, "[FAILED] STARTSTOPUNIT Should have failed "
			"with ILLEGAL_REQUEST/MEDIUM_REMOVAL_PREVENTED But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] STARTSTOPUNIT returned MEDIUM_REMOVAL_PREVENTED.");
	return 0;
}

int startstopunit_sanitize(struct iscsi_context *iscsi, int lun, int immed, int pcm, int pc, int no_flush, int loej, int start)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send STARTSTOPUNIT (Expecting SANITIZE_IN_PROGRESS) "
		"IMMED:%d PCM:%d PC:%d NO_FLUSH:%d LOEJ:%d START:%d",
		immed, pcm, pc, no_flush, loej, start);

	task = iscsi_startstopunit_sync(iscsi, lun, immed, pcm, pc, no_flush,
					loej, start);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send STARTSTOPUNIT command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] STARTSTOPUNIT successful but should have failed with NOT_READY/SANITIZE_IN_PROGRESS");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || task->sense.ascq != SCSI_SENSE_ASCQ_SANITIZE_IN_PROGRESS) {
		logging(LOG_NORMAL, "[FAILED] STARTSTOPUNIT Should have failed "
			"with NOT_READY/SANITIZE_IN_PROGRESS But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] STARTSTOPUNIT returned SANITIZE_IN_PROGRESS.");
	return 0;
}

int
testunitready(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send TESTUNITREADY");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send TESTUNITREADY command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_TIMEOUT) {
		logging(LOG_NORMAL,
			"TESTUNITREADY timed out");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] TESTUNITREADY command: failed with sense. %s",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] TESTUNITREADY returned SUCCESS.");
	return 0;
}

int
testunitready_clear_ua(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	int ret = -1;

	logging(LOG_VERBOSE,
	    "Send TESTUNITREADY (To Clear Possible UA) init=%s",
		iscsi->initiator_name);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send TESTUNITREADY command: %s",
			iscsi_get_error(iscsi));
		goto out;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[INFO] TESTUNITREADY command: failed with sense. %s",
			iscsi_get_error(iscsi));
		goto out;
	}
	logging(LOG_VERBOSE, "[OK] TESTUNITREADY does not return unit "
		"attention.");
	ret = 0;

out:
	scsi_free_scsi_task(task);
	return ret;
}

int
testunitready_nomedium(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send TESTUNITREADY (Expecting MEDIUM_NOT_PRESENT)");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send TESTUNITREADY "
			"command: %s", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] TESTUNITREADY command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] TESTUNITREADY Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] TESTUNITREADY returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
testunitready_sanitize(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send TESTUNITREADY (Expecting SANITIZE_IN_PROGRESS)");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send TESTUNITREADY "
			"command: %s", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] TESTUNITREADY command successful. But should have failed with NOT_READY/SANITIZE_IN_PROGRESS");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || task->sense.ascq != SCSI_SENSE_ASCQ_SANITIZE_IN_PROGRESS) {
		logging(LOG_NORMAL, "[FAILED] TESTUNITREADY Should have failed "
			"with NOT_READY/SANITIZE_IN_PROGRESS But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] TESTUNITREADY returned SANITIZE_IN_PROGRESS.");
	return 0;
}

int
testunitready_conflict(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send TESTUNITREADY (Expecting RESERVATION_CONFLICT)");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send TESTUNITREADY "
			"command: %s", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		logging(LOG_NORMAL, "[FAILED] Expected RESERVATION CONFLICT");
		return -1;
	}
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] TESTUNITREADY returned RESERVATION_CONFLICT.");
	return 0;
}

int compareandwrite(struct iscsi_context *iscsi, int lun, uint64_t lba,
		    unsigned char *data, uint32_t len, int blocksize,
		    int wrprotect, int dpo,
		    int fua, int group_number)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send COMPARE_AND_WRITE LBA:%" PRIu64
		" LEN:%d WRPROTECT:%d",
		lba, len, wrprotect);

	task = iscsi_compareandwrite_sync(iscsi, lun, lba,
		   data, len, blocksize,
		   wrprotect, dpo, fua, 0, group_number);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send COMPARE_AND_WRITE "
			"command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] COMPARE_AND_WRITE is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] COMPARE_AND_WRITE command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] COMPARE_AND_WRITE returned SUCCESS.");
	return 0;
}

int compareandwrite_miscompare(struct iscsi_context *iscsi, int lun,
			       uint64_t lba, unsigned char *data,
			       uint32_t len, int blocksize,
			       int wrprotect, int dpo,
			       int fua, int group_number)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send COMPARE_AND_WRITE LBA:%" PRIu64
		" LEN:%d WRPROTECT:%d (expecting MISCOMPARE)",
		lba, len, wrprotect);

	task = iscsi_compareandwrite_sync(iscsi, lun, lba,
		   data, len, blocksize,
		   wrprotect, dpo, fua, 0, group_number);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send COMPARE_AND_WRITE "
			"command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] COMPARE_AND_WRITE is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] COMPARE_AND_WRITE successful "
			"but should have failed with MISCOMPARE.");
		scsi_free_scsi_task(task);
		return -1;
	}

	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_MISCOMPARE
		|| task->sense.ascq != SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY) {
		logging(LOG_NORMAL, "[FAILED] COMPARE_AND_WRITE failed with "
			"the wrong sense code. Should have failed with "
			"MISCOMPARE/MISCOMPARE_DURING_VERIFY but failed with "
			"sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] COMPARE_AND_WRITE returned MISCOMPARE.");
	return 0;
}

struct scsi_task *get_lba_status_task(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send GET_LBA_STATUS LBA:%" PRIu64 " alloc_len:%d",
		lba, len);

	task = iscsi_get_lba_status_sync(iscsi, lun, lba, len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send GET_LBA_STATUS "
			"command: %s",
			iscsi_get_error(iscsi));
		return NULL;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] GET_LBA_STATUS is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return NULL;
	}

	logging(LOG_VERBOSE, "[OK] GET_LBA_STATUS returned SUCCESS.");
	return task;
}

int get_lba_status(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send GET_LBA_STATUS LBA:%" PRIu64 " alloc_len:%d",
		lba, len);

	task = iscsi_get_lba_status_sync(iscsi, lun, lba, len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send GET_LBA_STATUS "
			"command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] GET_LBA_STATUS is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] GET_LBA_STATUS returned SUCCESS.");
	return 0;
}

int get_lba_status_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send GET_LBA_STATUS (Expecting LBA_OUT_OF_RANGE) LBA:%" PRIu64 " alloc_len:%d",
		lba, len);

	task = iscsi_get_lba_status_sync(iscsi, lun, lba, len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send GET_LBA_STATUS "
			"command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] GET_LBA_STATUS is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS returned SUCCESS. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS failed with the wrong sense code. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE but failed with sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] GET_LBA_STATUS returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int get_lba_status_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send GET_LBA_STATUS (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " alloc_len:%d",
		lba, len);

	task = iscsi_get_lba_status_sync(iscsi, lun, lba, len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send GET_LBA_STATUS "
			"command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] GET_LBA_STATUS is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS returned SUCCESS. Should have failed with MEDIUM_NOT_PRESENT.");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS failed with the wrong sense code. Should have failed with NOT_READY/MEDIUM_NOT_PRESENT but failed with sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] GET_LBA_STATUS returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
prefetch10(struct iscsi_context *iscsi, int lun, uint32_t lba, int num, int immed, int group)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREFETCH10 LBA:%d blocks:%d"
			     " immed:%d group:%d",
			     lba, num, immed, group);

	task = iscsi_prefetch10_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREFETCH10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREFETCH10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREFETCH10 returned SUCCESS.");
	return 0;
}

int
prefetch10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
    int num, int immed, int group)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREFETCH10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d immed:%d group:%d",
		lba, num, immed, group);

	task = iscsi_prefetch10_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREFETCH10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREFETCH10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH10 returned SUCCESS. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH10 failed with the wrong sense code. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE but failed with sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREFETCH10 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
prefetch10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
    int num, int immed, int group)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREFETCH10 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d immed:%d group:%d",
		lba, num, immed, group);

	task = iscsi_prefetch10_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREFETCH10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREFETCH10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH10 returned SUCCESS. Should have failed with NOT_READY/MEDIUM_NOT_PRESENT.");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH10 failed with the wrong sense code. Should have failed with NOT_READY/MEDIUM_NOT_PRESENT but failed with sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREFETCH10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
prefetch16(struct iscsi_context *iscsi, int lun, uint64_t lba, int num, int immed, int group)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREFETCH16 LBA:%" PRIu64 " blocks:%d"
			     " immed:%d group:%d",
			     lba, num, immed, group);

	task = iscsi_prefetch16_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREFETCH16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREFETCH16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREFETCH16 returned SUCCESS.");
	return 0;
}

int
prefetch16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba,
    int num, int immed, int group)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREFETCH16 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRIu64 " blocks:%d immed:%d group:%d",
		lba, num, immed, group);

	task = iscsi_prefetch16_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREFETCH16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREFETCH16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH16 returned SUCCESS. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH16 failed with the wrong sense code. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE but failed with sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREFETCH16 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
prefetch16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba,
    int num, int immed, int group)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREFETCH16 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " blocks:%d immed:%d group:%d",
		lba, num, immed, group);

	task = iscsi_prefetch16_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREFETCH16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREFETCH16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH16 returned SUCCESS. Should have failed with NOT_READY/MEDIUM_NOT_PRESENT.");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] PREFETCH16 failed with the wrong sense code. Should have failed with NOT_READY/MEDIUM_NOT_PRESENT but failed with sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREFETCH16 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
preventallow(struct iscsi_context *iscsi, int lun, int prevent)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send PREVENTALLOW prevent:%d", prevent);
	task = iscsi_preventallow_sync(iscsi, lun, prevent);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send PREVENTALLOW "
			"command: %s", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] PREVENTALLOW is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] PREVENTALLOW command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] PREVENTALLOW returned SUCCESS.");
	return 0;
}

int
read6(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ6 LBA:%d blocks:%d",
		lba, datalen / blocksize);

	task = iscsi_read6_sync(iscsi, lun, lba, datalen, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ6 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] READ6 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ6 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ6 returned SUCCESS.");
	return 0;
}

int
read6_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
		    uint32_t datalen, int blocksize,
		    unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ6 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d",
		lba, datalen / blocksize);

	task = iscsi_read6_sync(iscsi, lun, lba, datalen, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ6 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] READ6 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ6 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] READ6 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ6 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

struct scsi_task*
read10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 LBA:%d blocks:%d rdprotect:%d "
	       "dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, rdprotect,
	       dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return NULL;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	logging(LOG_VERBOSE, "[OK] READ10 returned SUCCESS.");
	return task;
}

int
read10(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 LBA:%d blocks:%d rdprotect:%d "
	       "dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, rdprotect,
	       dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned SUCCESS.");
	return 0;
}

int
read10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] READ10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
read10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] READ10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
read10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
		uint32_t datalen, int blocksize, int rdprotect, 
		int dpo, int fua, int fua_nv, int group,
		unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10  (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] READ10 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
read12(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ12 LBA:%d blocks:%d rdprotect:%d "
	       "dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, rdprotect,
	       dpo, fua, fua_nv, group);

	task = iscsi_read12_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] READ12 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ12 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ12 returned SUCCESS.");
	return 0;
}

int
read12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ12 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read12_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] READ12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ12 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] READ12 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ12 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
read12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ12 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read12_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] READ12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ12 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] READ12 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ12 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
read12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
		uint32_t datalen, int blocksize, int rdprotect, 
		int dpo, int fua, int fua_nv, int group,
		unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ12 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read12_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] READ12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ12 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] READ12 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ12 returned MEDIUM_NOT_PRESENT*.");
	return 0;
}

int
read16(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ16 LBA:%" PRId64 " blocks:%d "
	       "rdprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, rdprotect,
	       dpo, fua, fua_nv, group);

	task = iscsi_read16_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READ16 is not available but the device claims SBC-3 support.");
			return -1;
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READ16 is not implemented and SBC-3 is not claimed.");
			return -2;
		}
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ16 returned SUCCESS.");
	return 0;
}

int
read16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ16 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%" PRId64 " blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read16_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READ16 is not available but the device claims SBC-3 support.");
			return -1;
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READ16 is not implemented and SBC-3 is not claimed.");
			return -2;
		}
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ16 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] READ16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ16 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
read16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ16 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRId64 " blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read16_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READ16 is not available but the device claims SBC-3 support.");
			return -1;
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READ16 is not implemented and SBC-3 is not claimed.");
			return -2;
		}
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ16 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] READ16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ16 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
read16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba,
		uint32_t datalen, int blocksize, int rdprotect, 
		int dpo, int fua, int fua_nv, int group,
		unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ16 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRId64 " blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read16_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READ16 is not available but the device claims SBC-3 support.");
			return -1;
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READ16 is not implemented and SBC-3 is not claimed.");
			return -2;
		}
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ16 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] READ16 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ16 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
read16_sanitize(struct iscsi_context *iscsi, int lun, uint64_t lba,
		uint32_t datalen, int blocksize, int rdprotect, 
		int dpo, int fua, int fua_nv, int group,
		unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ16 (Expecting SANITIZE_IN_PROGRESS) "
		"LBA:%" PRId64 " blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read16_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ16 successful but should "
			"have failed with NOT_READY/SANITIZE_IN_PROGRESS");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || task->sense.ascq != SCSI_SENSE_ASCQ_SANITIZE_IN_PROGRESS) {
		logging(LOG_NORMAL, "[FAILED] READ16 Should have failed "
			"with NOT_READY/SANITIZE_IN_PROGRESS But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ16 returned SANITIZE_IN_PROGRESS");
	return 0;
}

int
readcapacity10(struct iscsi_context *iscsi, int lun, uint32_t lba, int pmi)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READCAPACITY10 LBA:%d pmi:%d",
		lba, pmi);

	task = iscsi_readcapacity10_sync(iscsi, lun, lba, pmi);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READCAPACITY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READCAPACITY10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READCAPACITY10 returned SUCCESS.");
	return 0;
}

int
readcapacity10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, int pmi)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READCAPACITY10 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d pmi:%d",
		lba, pmi);

	task = iscsi_readcapacity10_sync(iscsi, lun, lba, pmi);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READCAPACITY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] READCAPACITY10 command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] READCAPACITY10 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READCAPACITY10 returned  MEDIUM_NOT_PRESENT.");
	return 0;
}

int
readcapacity16(struct iscsi_context *iscsi, int lun, int alloc_len)
{
	struct scsi_task *task;


	logging(LOG_VERBOSE, "Send READCAPACITY16 alloc_len:%d", alloc_len);

	task = scsi_cdb_serviceactionin16(SCSI_READCAPACITY16, alloc_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "Out-of-memory: Failed to create "
				"READCAPACITY16 cdb.");
		return -1;
	}
	task = iscsi_scsi_command_sync(iscsi, lun, task, NULL);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READCAPACITY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		if (inq->protect) {
			logging(LOG_NORMAL, "[FAILED] READCAPACITY16 is not "
				"available but INQ->PROTECT is set. "
				"ReadCapacity16 is mandatory when INQ->PROTECT "
				"is set.");
			return -1;
		}
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READCAPACITY16 is not available but the device claims SBC-3 support.");
			return -1;
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 is not implemented and SBC-3 is not claimed.");
			return -2;
		}
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READCAPACITY16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READCAPACITY16 returned SUCCESS.");
	return 0;
}

int
readcapacity16_nomedium(struct iscsi_context *iscsi, int lun, int alloc_len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READCAPACITY16 (Expecting MEDIUM_NOT_PRESENT) "
		"alloc_len:%d", alloc_len);

	task = scsi_cdb_serviceactionin16(SCSI_READCAPACITY16, alloc_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READCAPACITY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	task = iscsi_scsi_command_sync(iscsi, lun, task, NULL);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READCAPACITY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READCAPACITY16 is not available but the device claims SBC-3 support.");
			return -1;
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 is not implemented and SBC-3 is not claimed.");
			return -2;
		}
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] READCAPACITY16 command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] READCAPACITY16 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READCAPACITY16 returned  MEDIUM_NOT_PRESENT.");
	return 0;
}

int
release6(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	int i, res = 0;

	logging(LOG_VERBOSE, "Send RELEASE6");

	for (i = 0; i < 3 && res == 0; ++i) {
		task = iscsi_release6_sync(iscsi, lun);
		if (task == NULL) {
			logging(LOG_NORMAL,
				"[FAILED] Failed to send RELEASE6 command: %s",
				iscsi_get_error(iscsi));
			res = -1;
			break;
		}
		if (task->status != SCSI_STATUS_GOOD &&
		    !(task->status        == SCSI_STATUS_CHECK_CONDITION
		      && task->sense.key  == SCSI_SENSE_UNIT_ATTENTION
		      && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET)) {
			logging(LOG_NORMAL, "[FAILED] RELEASE6 command: "
				"failed with sense. %s",
				iscsi_get_error(iscsi));
			res = -1;
		}
		scsi_free_scsi_task(task);
	}

	if (res == 0)
		logging(LOG_VERBOSE, "[OK] RELEASE6 returned SUCCESS.");
	return res;
}

int report_supported_opcodes(struct iscsi_context *iscsi, int lun, int rctd, int options, int opcode, int sa, int alloc_len, struct scsi_task **save_task)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send REPORT_SUPPORTED_OPCODE RCTD:%d OPTIONS:%d "
		"OPCODE:0x%02x SA:%d ALLOC_LEN:%d",
		rctd, options, opcode, sa, alloc_len);

	task = iscsi_report_supported_opcodes_sync(iscsi, lun,
		rctd, options, opcode, sa, alloc_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send "
			"REPORT_SUPPORTED_OPCODES command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] REPORT_SUPPORTED_OPCODES is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] REPORT_SUPPORTED_OPCODES "
			"command: failed with sense. %s",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (save_task != NULL) {
		*save_task = task;
	} else {
		scsi_free_scsi_task(task);
	}

	logging(LOG_VERBOSE, "[OK] REPORT_SUPPORTED_OPCODES returned SUCCESS.");
	return 0;
}

int report_supported_opcodes_invalidfieldincdb(struct iscsi_context *iscsi, int lun, int rctd, int options, int opcode, int sa, int alloc_len, struct scsi_task **save_task)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send REPORT_SUPPORTED_OPCODE (expecting INVALID_FIELD_IN_CDB) RCTD:%d OPTIONS:%d "
		"OPCODE:0x%02x SA:%d ALLOC_LEN:%d",
		rctd, options, opcode, sa, alloc_len);

	task = iscsi_report_supported_opcodes_sync(iscsi, lun,
		rctd, options, opcode, sa, alloc_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send "
			"REPORT_SUPPORTED_OPCODES command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] REPORT_SUPPORTED_OPCODES is not "
			"implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] REPORT_SUPPORTED_OPCODES should have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (save_task != NULL) {
		*save_task = task;
	} else {
		scsi_free_scsi_task(task);
	}

	logging(LOG_VERBOSE, "[OK] REPORT_SUPPORTED_OPCODES returned "
		"INVALID_FIELD_IN_CDB.");
	return 0;
}

int
reserve6(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	int i, res = 0;

	logging(LOG_VERBOSE, "Send RESERVE6");

	for (i = 0; i < 3 && res == 0; ++i) {
		task = iscsi_reserve6_sync(iscsi, lun);
		if (task == NULL) {
			logging(LOG_NORMAL,
				"[FAILED] Failed to send RESERVE6 command: %s",
				iscsi_get_error(iscsi));
			res = -1;
			break;
		}
		if (task->status        == SCSI_STATUS_CHECK_CONDITION
		    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
		    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
			logging(LOG_NORMAL, "[SKIPPED] RESERVE6 is not "
				"implemented on target");
			res = -2;
		} else if (task->status != SCSI_STATUS_GOOD &&
		    !(task->status        == SCSI_STATUS_CHECK_CONDITION
		      && task->sense.key  == SCSI_SENSE_UNIT_ATTENTION
		      && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET)) {
			logging(LOG_NORMAL, "[FAILED] RESERVE6 command: "
				"failed with sense. %s",
				iscsi_get_error(iscsi));
			res = -1;
		}
		scsi_free_scsi_task(task);
	}

	if (res == 0)
		logging(LOG_VERBOSE, "[OK] RESERVE6 returned SUCCESS.");
	return res;
}

int
reserve6_conflict(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	int i, res = 0;

	logging(LOG_VERBOSE, "Send RESERVE6 (Expecting RESERVATION_CONFLICT)");

	for (i = 0; i < 3 && res == 0; ++i) {
		task = iscsi_reserve6_sync(iscsi, lun);
		if (task == NULL) {
			logging(LOG_NORMAL,
				"[FAILED] Failed to send RESERVE6 command: %s",
				iscsi_get_error(iscsi));
			res = -1;
			break;
		}
		if (task->status        == SCSI_STATUS_CHECK_CONDITION
		    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
		    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
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
unmap(struct iscsi_context *iscsi, int lun, int anchor, struct unmap_list *list, int list_len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send UNMAP list_len:%d anchor:%d", list_len, anchor);
	task = iscsi_unmap_sync(iscsi, lun, anchor, 0, list, list_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send UNMAP command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] UNMAP is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] UNMAP command: failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] UNMAP returned SUCCESS.");
	return 0;
}

int
unmap_writeprotected(struct iscsi_context *iscsi, int lun, int anchor, struct unmap_list *list, int list_len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send UNMAP (Expecting WRITE_PROTECTED) "
		"list_len:%d anchor:%d", list_len, anchor);

	task = iscsi_unmap_sync(iscsi, lun, anchor, 0, list, list_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send UNMAP command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] UNMAP is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] UNMAP successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] UNMAP failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] UNMAP returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
unmap_nomedium(struct iscsi_context *iscsi, int lun, int anchor, struct unmap_list *list, int list_len)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send UNMAP (Expecting MEDIUM_NOT_PRESENT) "
		"list_len:%d anchor:%d", list_len, anchor);

	task = iscsi_unmap_sync(iscsi, lun, anchor, 0, list, list_len);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send UNMAP command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] UNMAP is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] UNMAP successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] UNMAP Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] UNMAP returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
verify10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY10 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY10 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 command: failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY10 returned SUCCESS.");
	return 0;
}

int
verify10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY10 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY10 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 successful but should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}	
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
	  logging(LOG_NORMAL, "[FAILED] VERIFY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT* but failed with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
verify10_miscompare(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY10 (Expecting MISCOMPARE) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY10 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 successful but should have failed with MISCOMPARE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->sense.key != SCSI_SENSE_MISCOMPARE) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 command returned wrong sense key. MISCOMPARE MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s", SCSI_SENSE_MISCOMPARE, task->sense.key, iscsi_get_error(iscsi)); 
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY10 returned MISCOMPARE.");
	return 0;
}

int
verify10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY10 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 successful but should have failed with LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY10 returned LBA_OUT_OF_RANGE.");
	return 0;
}

int
verify10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY10 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY10 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 successful but should have failed with LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] VERIFY10 should have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY10 returned INVALID_FIELD_IN_CDB.");
	return 0;
}

int
verify12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY12 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY12 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY12 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 command: failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY12 returned SUCCESS.");
	return 0;
}

int
verify12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY12 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY12 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY12 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 successful but should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}	
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
	  logging(LOG_NORMAL, "[FAILED] VERIFY12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT* but failed with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY12 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
verify12_miscompare(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY12 (expecting MISCOMPARE) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY12 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY12 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 successful but should have failed with MISCOMPARE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->sense.key != SCSI_SENSE_MISCOMPARE) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 command returned wrong sense key. MISCOMPARE MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s", SCSI_SENSE_MISCOMPARE, task->sense.key, iscsi_get_error(iscsi)); 
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY12 returned MISCOMPARE.");
	return 0;
}

int
verify12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY12 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY12 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY12 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 successful but should have failed with LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY12 returned LBA_OUT_OF_RANGE.");
	return 0;
}

int
verify12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY12 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY12 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY12 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 successful but should have failed with LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] VERIFY12 should have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY12 returned INVALID_FIELD_IN_CDB.");
	return 0;
}

int
verify16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY16 LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY16 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 command: failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY16 returned SUCCESS.");
	return 0;
}

int
verify16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY16 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY16 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 successful but should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}	
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
	  logging(LOG_NORMAL, "[FAILED] VERIFY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT* but failed with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY16 returned NOT_MEDIUM_NOT_PRESENT.");
	return 0;
}

int
verify16_miscompare(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY16 (Expecting MISCOMPARE) "
		"LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY16 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 successful but should have failed with MISCOMPARE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->sense.key != SCSI_SENSE_MISCOMPARE) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 command returned wrong sense key. MISCOMPARE MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s", SCSI_SENSE_MISCOMPARE, task->sense.key, iscsi_get_error(iscsi)); 
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY16 returned MISCOMPARE.");
	return 0;
}

int
verify16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY16 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY16 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 successful but should have failed with LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY16 returned LBA_OUT_OF_RANGE.");
	return 0;
}

int
verify16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send VERIFY16 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d",
		lba, datalen / blocksize, vprotect, dpo, bytchk);

	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send VERIFY16 command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] VERIFY16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 successful but should have failed with LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] VERIFY16 should have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB. Sense:%s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] VERIFY16 returned INVALID_FIELD_IN_CDB.");
	return 0;
}

int
write10(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE10 LBA:%d blocks:%d "
	       "wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write10_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE10 returned SUCCESS.");
	return 0;
}

int
write10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE10 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE10 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
write10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE10 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
write10_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE10 (Expecting WRITE_PROTECTED) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE10 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
write10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
		 uint32_t datalen, int blocksize, int wrprotect, 
		 int dpo, int fua, int fua_nv, int group,
		 unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE10 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITE10 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
write12(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE12 LBA:%d blocks:%d "
	       "wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write12_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE12 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE12 returned SUCCESS.");
	return 0;
}

int
write12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE12 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE12 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
write12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE12 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE12 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
write12_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE12 (Expecting WRITE_PROTECTED) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE12 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
write12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
		 uint32_t datalen, int blocksize, int wrprotect, 
		 int dpo, int fua, int fua_nv, int group,
		 unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE12 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITE12 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE12 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
write16(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE16 LBA:%" PRId64 " blocks:%d "
	       "wrprotect:%d dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write16_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE16 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE16 returned SUCCESS.");
	return 0;
}

int
write16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE16 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%" PRId64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE16 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE16 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
write16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE16 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRId64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE16 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE16 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
write16_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE16 (Expecting WRITE_PROTECTED) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE16 is not implemented.");
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE16 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
write16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba,
		 uint32_t datalen, int blocksize, int wrprotect, 
		 int dpo, int fua, int fua_nv, int group,
		 unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITE16 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, fua, fua_nv, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_write16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITE16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITE16 is not implemented.");
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITE16 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITE16 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
writesame10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;
	uint64_t realdatalen;

	logging(LOG_VERBOSE, "Send WRITESAME10 LBA:%d blocks:%d "
	       "wrprotect:%d anchor:%d unmap:%d group:%d",
	       lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame10_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		if (inq_bl->wsnz == 1 && datalen == 0) {
			logging(LOG_NORMAL, "[SKIPPED] Target does not support WRITESAME10 with NUMBER OF LOGICAL BLOCKS == 0");
			scsi_free_scsi_task(task);
			return -3;
		}

		if (datalen == 0) {
			realdatalen = num_blocks;
		} else {
			realdatalen = datalen;
		}
		if (inq_bl->max_ws_len > 0 && realdatalen > inq_bl->max_ws_len) {
			logging(LOG_NORMAL, "[SKIPPED] Number of WRITESAME10 logical blocks to be written exceeds MAXIMUM WRITE SAME LENGTH");
			scsi_free_scsi_task(task);
			return -4;
		}
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME10 returned SUCCESS.");
	return 0;
}

int
writesame10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame10_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME10 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
writesame10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME10 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame10_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME10 returned ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB.");
	return 0;
}

int
writesame10_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME10 (Expecting WRITE_PROTECTED) "
		"LBA:%d blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame10_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 failed with wrong sense. "
			"Should have failed with DATA_PROTECTION/"
			"WRITE_PROTECTED. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME10 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
writesame10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME10 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame10_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME10 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] WRITESAME10 command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME10 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
writesame16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;
	uint64_t realdatalen;

	logging(LOG_VERBOSE, "Send WRITESAME16 LBA:%" PRIu64 " blocks:%d "
	       "wrprotect:%d anchor:%d unmap:%d group:%d",
	       lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame16_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		if (inq_bl->wsnz == 1 && datalen == 0) {
			logging(LOG_NORMAL, "[SKIPPED] Target does not support WRITESAME16 with NUMBER OF LOGICAL BLOCKS == 0");
			scsi_free_scsi_task(task);
			return -3;
		}

		if (datalen == 0) {
			realdatalen = num_blocks;
		} else {
			realdatalen = datalen;
		}
		if (inq_bl->max_ws_len > 0 && realdatalen > inq_bl->max_ws_len) {
			logging(LOG_NORMAL, "[SKIPPED] Number of WRITESAME16 logical blocks to be written exceeds MAXIMUM WRITE SAME LENGTH");
			scsi_free_scsi_task(task);
			return -4;
		}
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME16 returned SUCCESS.");
	return 0;
}

int
writesame16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME16 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRIu64 " blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame16_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME16 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
writesame16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME16 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%" PRIu64 " blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame16_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME16 returned ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB.");
	return 0;
}

int
writesame16_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME16 (Expecting WRITE_PROTECTED) "
		"LBA:%" PRIu64 " blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame16_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 failed with wrong sense. "
			"Should have failed with DATA_PROTECTION/"
			"WRITE_PROTECTED. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME16 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
writesame16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num, int anchor, int unmap_flag, int wrprotect, int group, unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITESAME16 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " blocks:%d "
		"wrprotect:%d anchor:%d unmap:%d group:%d",
		lba, num, wrprotect,
		anchor, unmap_flag, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writesame16_sync(iscsi, lun, lba, 
				      data, datalen, num,
				      anchor, unmap_flag, wrprotect, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITESAME16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 is not implemented on target");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	  logging(LOG_NORMAL, "[FAILED] WRITESAME16 command successful. But should have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITESAME16 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITESAME16 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
writeverify10(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY10 LBA:%d blocks:%d "
	       "wrprotect:%d dpo:%d bytchk:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify10_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY10 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY10 returned SUCCESS.");
	return 0;
}

int
writeverify10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY10 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY10 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
writeverify10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY10 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
writeverify10_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY10 (Expecting WRITE_PROTECTED) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY10 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
writeverify10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY10 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify10_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY10 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY10 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY10 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
writeverify12(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY12 LBA:%d blocks:%d "
	       "wrprotect:%d dpo:%d bytchk:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify12_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY12 returned SUCCESS.");
	return 0;
}

int
writeverify12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY12 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY12 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
writeverify12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY12 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY12 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
writeverify12_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY12 (Expecting WRITE_PROTECTED) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY12 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
writeverify12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY12 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%d blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify12_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY12 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY12 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY12 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
writeverify16(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY16 LBA:%" PRIu64 " blocks:%d "
	       "wrprotect:%d dpo:%d bytchk:%d group:%d",
	       lba, datalen / blocksize, wrprotect,
	       dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify16_sync(iscsi, lun, lba, 
				  data, datalen, blocksize,
				  wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY16 is not implemented.");
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY16 returned SUCCESS.");
	return 0;
}

int
writeverify16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY16 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY16 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY16 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
writeverify16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY16 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY16 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY16 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
writeverify16_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY16 (Expecting WRITE_PROTECTED) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY16 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 successful but should "
			"have failed with DATA_PROTECTION/WRITE_PROTECTED");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 failed with wrong sense. "
			"Should have failed with DATA_PRTOTECTION/"
			"WRITE_PROTECTED. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY16 returned DATA_PROTECTION/WRITE_PROTECTED.");
	return 0;
}

int
writeverify16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba,
       uint32_t datalen, int blocksize, int wrprotect, 
       int dpo, int bytchk, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send WRITEVERIFY16 (Expecting MEDIUM_NOT_PRESENT) "
		"LBA:%" PRIu64 " blocks:%d wrprotect:%d "
		"dpo:%d bytchk:%d group:%d",
		lba, datalen / blocksize, wrprotect,
		dpo, bytchk, group);

	if (!data_loss) {
		printf("--dataloss flag is not set in. Skipping write\n");
		return -1;
	}

	task = iscsi_writeverify16_sync(iscsi, lun, lba, data, datalen, blocksize,
				 wrprotect, dpo, bytchk, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send WRITEVERIFY16 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		scsi_free_scsi_task(task);
		logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY16 is not implemented.");
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 successful but should "
			"have failed with NOT_READY/MEDIUM_NOT_PRESENT*");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		logging(LOG_NORMAL, "[FAILED] WRITEVERIFY16 Should have failed "
			"with NOT_READY/MEDIUM_NOT_PRESENT* But failed "
			"with %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}	

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] WRITEVERIFY16 returned MEDIUM_NOT_PRESENT.");
	return 0;
}

int
inquiry(struct iscsi_context *iscsi, int lun, int evpd, int page_code, int maxsize, struct scsi_task **save_task)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send INQUIRY evpd:%d page_code:%02x alloc_len:%d",
		evpd, page_code, maxsize);
	task = iscsi_inquiry_sync(iscsi, lun, evpd, page_code, maxsize);
	if (task == NULL) {
	        logging(LOG_NORMAL, "[FAILED] Failed to send INQUIRY command: "
			"%s", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        logging(LOG_NORMAL, "[FAILED] INQUIRY command: failed with "
			"sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (save_task != NULL) {
		*save_task = task;
	} else {
		scsi_free_scsi_task(task);
	}

	logging(LOG_VERBOSE, "[OK] INQUIRY returned SUCCESS.");
	return 0;
}

int
inquiry_invalidfieldincdb(struct iscsi_context *iscsi, int lun, int evpd, int page_code, int maxsize)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send INQUIRY (Expecting INVALID_FIELD_IN_CDB) evpd:%d page_code:%02x alloc_len:%d",
		evpd, page_code, maxsize);
	task = iscsi_inquiry_sync(iscsi, lun, evpd, page_code, maxsize);
	if (task == NULL) {
	        logging(LOG_NORMAL, "[FAILED] Failed to send INQUIRY command: "
			"%s", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] INQUIRY successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}

	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] INQUIRY failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] INQUIRY returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
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

int set_swp(struct iscsi_context *iscsi, int lun)
{
	int ret = 0;
	struct scsi_task *sense_task = NULL;
	struct scsi_task *select_task = NULL;
	struct scsi_mode_sense *ms;
	struct scsi_mode_page *mp;

	logging(LOG_VERBOSE, "Read CONTROL page");
	sense_task = iscsi_modesense6_sync(iscsi, lun,
		1, SCSI_MODESENSE_PC_CURRENT,
		SCSI_MODEPAGE_CONTROL,
		0, 255);
	if (sense_task == NULL) {
		logging(LOG_NORMAL, "Failed to send MODE_SENSE6 command: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (sense_task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "MODE_SENSE6 failed: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
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

	logging(LOG_VERBOSE, "Turn SWP ON");
	mp->control.swp = 1;

	select_task = iscsi_modeselect6_sync(iscsi, lun,
		    1, 0, mp);
	if (select_task == NULL) {
		logging(LOG_NORMAL, "Failed to send MODE_SELECT6 command: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (select_task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "MODE_SELECT6 failed: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}

finished:
	if (sense_task != NULL) {
		scsi_free_scsi_task(sense_task);
	}
	if (select_task != NULL) {
		scsi_free_scsi_task(select_task);
	}
	return ret;
}

int clear_swp(struct iscsi_context *iscsi, int lun)
{
	int ret = 0;
	struct scsi_task *sense_task = NULL;
	struct scsi_task *select_task = NULL;
	struct scsi_mode_sense *ms;
	struct scsi_mode_page *mp;

	logging(LOG_VERBOSE, "Read CONTROL page");
	sense_task = iscsi_modesense6_sync(iscsi, lun,
		1, SCSI_MODESENSE_PC_CURRENT,
		SCSI_MODEPAGE_CONTROL,
		0, 255);
	if (sense_task == NULL) {
		logging(LOG_NORMAL, "Failed to send MODE_SENSE6 command: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (sense_task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "MODE_SENSE6 failed: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
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

	logging(LOG_VERBOSE, "Turn SWP OFF");
	mp->control.swp = 0;

	select_task = iscsi_modeselect6_sync(iscsi, lun,
		    1, 0, mp);
	if (select_task == NULL) {
		logging(LOG_NORMAL, "Failed to send MODE_SELECT6 command: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (select_task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "MODE_SELECT6 failed: %s",
			iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}

finished:
	if (sense_task != NULL) {
		scsi_free_scsi_task(sense_task);
	}
	if (select_task != NULL) {
		scsi_free_scsi_task(select_task);
	}
	return ret;
}
