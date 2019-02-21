/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "slist.h"

static void
iscsi_scsi_response_cb(struct iscsi_context *iscsi, int status,
		       void *command_data _U_, void *private_data)
{
	struct iscsi_scsi_cbdata *scsi_cbdata =
	  (struct iscsi_scsi_cbdata *)private_data;

	switch (status) {
	case SCSI_STATUS_RESERVATION_CONFLICT:
	case SCSI_STATUS_CHECK_CONDITION:
	case SCSI_STATUS_GOOD:
	case SCSI_STATUS_BUSY:
	case SCSI_STATUS_CONDITION_MET:
	case SCSI_STATUS_TASK_SET_FULL:
	case SCSI_STATUS_ACA_ACTIVE:
	case SCSI_STATUS_TASK_ABORTED:
	case SCSI_STATUS_ERROR:
	case SCSI_STATUS_CANCELLED:
	case SCSI_STATUS_TIMEOUT:
		scsi_cbdata->task->status = status;
		if (scsi_cbdata->callback) {
			scsi_cbdata->callback(iscsi, status, scsi_cbdata->task,
			                      scsi_cbdata->private_data);
		}
		return;
	default:
		scsi_cbdata->task->status = SCSI_STATUS_ERROR;
		iscsi_set_error(iscsi, "Cant handle  scsi status %d yet.",
		                status);
		if (scsi_cbdata->callback) {
			scsi_cbdata->callback(iscsi, SCSI_STATUS_ERROR, scsi_cbdata->task,
			                      scsi_cbdata->private_data);
		}
	}
}

static int
iscsi_send_data_out(struct iscsi_context *iscsi, struct iscsi_pdu *cmd_pdu,
		    uint32_t ttt, uint32_t offset, uint32_t tot_len)
{
	while (tot_len > 0) {
		uint32_t len = tot_len;
		struct iscsi_pdu *pdu;
		int flags;

		len = MIN(len, iscsi->target_max_recv_data_segment_length);

		pdu = iscsi_allocate_pdu(iscsi,
					 ISCSI_PDU_DATA_OUT,
					 ISCSI_PDU_NO_PDU,
					 cmd_pdu->itt,
					 ISCSI_PDU_DROP_ON_RECONNECT|ISCSI_PDU_DELETE_WHEN_SENT);
		if (pdu == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory, Failed to allocate "
				"scsi data out pdu.");
			goto error;
		}
		pdu->scsi_cbdata.task         = cmd_pdu->scsi_cbdata.task;
		/* set the cmdsn in the pdu struct so we can compare with
		 * maxcmdsn when sending to socket even if data-out pdus
		 * do not carry a cmdsn on the wire */
		pdu->cmdsn                    = cmd_pdu->cmdsn;

		if (tot_len == len) {
			flags = ISCSI_PDU_SCSI_FINAL;
		} else {
			flags = 0;
		}

		/* flags */
		iscsi_pdu_set_pduflags(pdu, flags);

		/* lun */
		iscsi_pdu_set_lun(pdu, cmd_pdu->lun);

		/* ttt */
		iscsi_pdu_set_ttt(pdu, ttt);

		/* data sn */
		iscsi_pdu_set_datasn(pdu, cmd_pdu->datasn++);

		/* buffer offset */
		iscsi_pdu_set_bufferoffset(pdu, offset);

		pdu->payload_offset = offset;
		pdu->payload_len    = len;

		/* update data segment length */
		scsi_set_uint32(&pdu->outdata.data[4], pdu->payload_len);

		if (iscsi_queue_pdu(iscsi, pdu) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"scsi pdu.");
			goto error;
		}

		tot_len -= len;
		offset  += len;
	}
	return 0;

error:
	ISCSI_LIST_REMOVE(&iscsi->outqueue, cmd_pdu);
	ISCSI_LIST_REMOVE(&iscsi->waitpdu, cmd_pdu);
	if (cmd_pdu->callback) {
		cmd_pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
						  cmd_pdu->private_data);
	}
	iscsi->drv->free_pdu(iscsi, cmd_pdu);
	return -1;
}

static int
iscsi_send_unsolicited_data_out(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	uint32_t len = MIN(pdu->expxferlen, iscsi->first_burst_length) - pdu->payload_len;

	return iscsi_send_data_out(iscsi, pdu, 0xffffffff,
				   pdu->payload_len, len);
}

/* Using 'struct iscsi_data *d' for data-out is optional
 * and will be converted into a one element data-out iovector.
 */
int
iscsi_scsi_command_async(struct iscsi_context *iscsi, int lun,
			 struct scsi_task *task, iscsi_command_cb cb,
			 struct iscsi_data *d, void *private_data)
{
	struct iscsi_pdu *pdu;
	int flags;

	if (iscsi->old_iscsi) {
		iscsi = iscsi->old_iscsi;
		ISCSI_LOG(iscsi, 2, "iscsi_scsi_command_async: queuing cmd to old_iscsi while reconnecting");
	}

	if (iscsi->session_type != ISCSI_SESSION_NORMAL) {
		iscsi_set_error(iscsi, "Trying to send command on "
				"discovery session.");
		return -1;
	}

	if (iscsi->is_loggedin == 0 && !iscsi->pending_reconnect) {
		iscsi_set_error(iscsi, "Trying to send command while "
				"not logged in.");
		return -1;
	}

	/* We got an actual buffer from the application. Convert it to
	 * a data-out iovector.
	 */
	if (d != NULL && d->data != NULL) {
		struct scsi_iovec *iov;

		iov = scsi_malloc(task, sizeof(struct scsi_iovec));
		if (iov == NULL) {
			return -1;
		}
		iov->iov_base = d->data;
		iov->iov_len  = d->size;
		scsi_task_set_iov_out(task, iov, 1);
	}

	pdu = iscsi_allocate_pdu(iscsi,
				 ISCSI_PDU_SCSI_REQUEST,
				 ISCSI_PDU_SCSI_RESPONSE,
				 iscsi_itt_post_increment(iscsi),
				 0);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory, Failed to allocate "
				"scsi pdu.");
		return -1;
	}

	pdu->scsi_cbdata.task         = task;
	pdu->scsi_cbdata.callback     = cb;
	pdu->scsi_cbdata.private_data = private_data;

	pdu->payload_offset = 0;
	pdu->payload_len    = 0;

	scsi_set_task_private_ptr(task, &pdu->scsi_cbdata);
	
	/* flags */
	flags = ISCSI_PDU_SCSI_FINAL|ISCSI_PDU_SCSI_ATTR_SIMPLE;
	switch (task->xfer_dir) {
	case SCSI_XFER_NONE:
		break;
	case SCSI_XFER_READ:
		flags |= ISCSI_PDU_SCSI_READ;
		break;
	case SCSI_XFER_WRITE:
		flags |= ISCSI_PDU_SCSI_WRITE;

		/* If we can send immediate data, send as much as we can */
		if (iscsi->use_immediate_data == ISCSI_IMMEDIATE_DATA_YES) {
			uint32_t len = task->expxferlen;

			len = MIN(len, iscsi->first_burst_length);
			len = MIN(len, iscsi->target_max_recv_data_segment_length);

			pdu->payload_offset = 0;
			pdu->payload_len    = len;

			/* update data segment length */
			scsi_set_uint32(&pdu->outdata.data[4], pdu->payload_len);
		}
		/* We have (more) data to send and we are allowed to send
		 * it as unsolicited data-out segments.
		 * Drop the F-flag from the pdu and start sending a train
		 * of data-out further below.
		 */
		if (iscsi->use_initial_r2t == ISCSI_INITIAL_R2T_NO
		    && pdu->payload_len < (uint32_t)task->expxferlen
		    && pdu->payload_len < iscsi->first_burst_length) {
			/* We have more data to send, and we are allowed to send
			 * unsolicited data, so don't flag this PDU as final.
			 */
			flags &= ~ISCSI_PDU_SCSI_FINAL;
		}
		break;
	}
	iscsi_pdu_set_pduflags(pdu, flags);

	/* lun */
	iscsi_pdu_set_lun(pdu, lun);
	pdu->lun = lun;

	/* expxferlen */
	iscsi_pdu_set_expxferlen(pdu, task->expxferlen);

	/* cmdsn */
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn++);

	/* cdb */
	iscsi_pdu_set_cdb(pdu, task);

	pdu->callback     = iscsi_scsi_response_cb;
	pdu->private_data = &pdu->scsi_cbdata;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"scsi pdu.");
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* The F flag is not set. This means we haven't sent all the unsolicited
	 * data yet. Sent as much as we are allowed as a train of DATA-OUT PDUs.
	 * We might already have sent some data as immediate data, which we must
	 * subtract from first_burst_length.
	 */
	if (!(flags & ISCSI_PDU_SCSI_FINAL)) {
		iscsi_send_unsolicited_data_out(iscsi, pdu);
	}

	/* remember cmdsn and itt so we can use task management */
	task->cmdsn = pdu->cmdsn;
	task->itt   = pdu->itt;
	task->lun   = lun;

	return 0;
}

/* Parse a sense key specific sense data descriptor */
static void parse_sense_spec(struct scsi_sense *sense, const uint8_t inf[3])
{
	if (!(inf[0] & 0x80)) /* SKSV */
		return;

	sense->sense_specific = 1;
	sense->ill_param_in_cdb = !!(inf[0] & 0x40); /* C/D flag */
	if (inf[0] & 8) { /* BPV */
		sense->bit_pointer_valid = 1;
		sense->bit_pointer = inf[0] & 7;
	}
	sense->field_pointer = scsi_get_uint16(&inf[1]);
}

/* Parse descriptor format sense data */
static void parse_sense_descriptors(struct scsi_sense *sense, const uint8_t *sb,
				    unsigned sb_len)
{
	const unsigned char *p, *const end = sb + sb_len;

	for (p = sb; p < end; p += p[1]) {
		if (p[1] < 4) /* length */
			break;
		if (!(p[2] & 0x80)) /* VALID bit */
			break;
		switch (p[0]) {
		case 2:
			/* Sense key specific sense data descriptor */
			parse_sense_spec(sense, p + 4);
			break;
		}
	}
}

void scsi_parse_sense_data(struct scsi_sense *sense, const uint8_t *sb)
{
	sense->error_type = sb[0] & 0x7f;
	switch (sense->error_type) {
	case 0x70:
	case 0x71:
		/* Fixed format */
		sense->key  = sb[2] & 0x0f;
		sense->ascq = scsi_get_uint16(&sb[12]);
		parse_sense_spec(sense, sb + 15);
		break;
	case 0x72:
	case 0x73:
		/* Descriptor format */
		sense->key  = sb[1] & 0x0f;
		sense->ascq = scsi_get_uint16(&sb[2]);
		parse_sense_descriptors(sense, sb + 8, sb[7]);
		break;
	}
}

int
iscsi_process_scsi_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			 struct iscsi_in_pdu *in)
{
	uint8_t flags, response, status;
	struct iscsi_scsi_cbdata *scsi_cbdata = &pdu->scsi_cbdata;
	struct scsi_task *task = scsi_cbdata->task;

	flags = in->hdr[1];
	if ((flags&ISCSI_PDU_DATA_FINAL) == 0) {
		iscsi_set_error(iscsi, "scsi response pdu but Final bit is "
				"not set: 0x%02x.", flags);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			              pdu->private_data);
		}
		return -1;
	}
	if ((flags&ISCSI_PDU_DATA_ACK_REQUESTED) != 0) {
		iscsi_set_error(iscsi, "scsi response asked for ACK "
				"0x%02x.", flags);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			              pdu->private_data);
		}
		return -1;
	}

	response = in->hdr[2];

	task->residual_status = SCSI_RESIDUAL_NO_RESIDUAL;
	task->residual = 0;

	if (flags & (ISCSI_PDU_DATA_RESIDUAL_OVERFLOW|
		     ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW)) {
		if (response != 0) {
			iscsi_set_error(iscsi, "protocol error: flags %#02x;"
					" response %#02x.", flags, response);
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
				              pdu->private_data);
			}
			return -1;
		}
		task->residual = scsi_get_uint32(&in->hdr[44]);
		if (flags & ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW) {
			task->residual_status = SCSI_RESIDUAL_UNDERFLOW;
		} else {
			task->residual_status = SCSI_RESIDUAL_OVERFLOW;
		}
	}

	status = in->hdr[3];

	switch (status) {
	case SCSI_STATUS_GOOD:
	case SCSI_STATUS_CONDITION_MET:
		task->datain.data = pdu->indata.data;
		task->datain.size = pdu->indata.size;

		/* the pdu->datain.data was malloc'ed by iscsi_malloc,
		   as long as we have no struct iscsi_task we cannot track
		   the free'ing of this buffer which is currently
		   done in scsi_free_scsi_task() */
		if (pdu->indata.data != NULL) iscsi->frees++;

		pdu->indata.data = NULL;
		pdu->indata.size = 0;

		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_GOOD, task,
			              pdu->private_data);
		}
		break;
	case SCSI_STATUS_CHECK_CONDITION:
		task->datain.size = in->data_pos;
		task->datain.data = malloc(task->datain.size);
		if (task->datain.data == NULL) {
			iscsi_set_error(iscsi, "failed to allocate blob for "
					"sense data");
			break;
		}
		memcpy(task->datain.data, in->data, task->datain.size);

		scsi_parse_sense_data(&task->sense, &task->datain.data[2]);
		iscsi_set_error(iscsi, "SENSE KEY:%s(%d) ASCQ:%s(0x%04x)",
				scsi_sense_key_str(task->sense.key),
				task->sense.key,
				scsi_sense_ascq_str(task->sense.ascq),
				task->sense.ascq);
		if (task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST) {
			iscsi_dump_pdu_header(iscsi, pdu->outdata.data);
		}
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_CHECK_CONDITION, task,
			              pdu->private_data);
		}
		break;
	case SCSI_STATUS_RESERVATION_CONFLICT:
		iscsi_set_error(iscsi, "RESERVATION CONFLICT");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_RESERVATION_CONFLICT,
			              task, pdu->private_data);
		}
		break;
	case SCSI_STATUS_TASK_SET_FULL:
		iscsi_set_error(iscsi, "TASK_SET_FULL");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_TASK_SET_FULL,
			              task, pdu->private_data);
		}
		break;
	case SCSI_STATUS_ACA_ACTIVE:
		iscsi_set_error(iscsi, "ACA_ACTIVE");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ACA_ACTIVE,
			              task, pdu->private_data);
		}
		break;
	case SCSI_STATUS_TASK_ABORTED:
		iscsi_set_error(iscsi, "TASK_ABORTED");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_TASK_ABORTED,
			              task, pdu->private_data);
		}
		break;
	case SCSI_STATUS_BUSY:
		iscsi_set_error(iscsi, "BUSY");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_BUSY,
			              task, pdu->private_data);
		}
		break;
	default:
		iscsi_set_error(iscsi, "Unknown SCSI status :%d.", status);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR,
			              task, pdu->private_data);
		}
		return -1;
	}

	return 0;
}

int
iscsi_process_scsi_data_in(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			   struct iscsi_in_pdu *in, int *is_finished)
{
	uint32_t flags, status;
	struct iscsi_scsi_cbdata *scsi_cbdata = &pdu->scsi_cbdata;
	struct scsi_task *task = scsi_cbdata->task;
	int dsl;

	flags = in->hdr[1];
	if ((flags&ISCSI_PDU_DATA_ACK_REQUESTED) != 0) {
		iscsi_set_error(iscsi, "scsi response asked for ACK "
				"0x%02x.", flags);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			              pdu->private_data);
		}
		return -1;
	}
	dsl = scsi_get_uint32(&in->hdr[4]) & 0x00ffffff;

	/* Don't add to reassembly buffer if we already have a user buffer */
	if (task->iovector_in.iov == NULL) {
		if (iscsi_add_data(iscsi, &pdu->indata, in->data, dsl, 0) != 0) {
		    iscsi_set_error(iscsi, "Out-of-memory: failed to add data "
				"to pdu in buffer.");
			return -1;
		}
	}

	if ((flags&ISCSI_PDU_DATA_FINAL) == 0) {
		*is_finished = 0;
	}
	if ((flags&ISCSI_PDU_DATA_CONTAINS_STATUS) == 0) {
		*is_finished = 0;
	}

	if (*is_finished == 0) {
		return 0;
	}

	task->residual_status = SCSI_RESIDUAL_NO_RESIDUAL;
	task->residual = 0;

	/*
	 * These flags should only be set if the S flag is also set
	 */
	if (flags & (ISCSI_PDU_DATA_RESIDUAL_OVERFLOW|ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW)) {
		task->residual = scsi_get_uint32(&in->hdr[44]);
		if (flags & ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW) {
			task->residual_status = SCSI_RESIDUAL_UNDERFLOW;
		} else {
			task->residual_status = SCSI_RESIDUAL_OVERFLOW;
		}
	}


	/* this was the final data-in packet in the sequence and it has
	 * the s-bit set, so invoke the callback.
	 */
	status = in->hdr[3];
	task->datain.data = pdu->indata.data;
	task->datain.size = pdu->indata.size;

	/* the pdu->indata.data was malloc'ed by iscsi_malloc,
	   as long as we have no struct iscsi_task we cannot track
	   the free'ing of this buffer which is currently
	   done in scsi_free_scsi_task() */
	if (pdu->indata.data != NULL) iscsi->frees++;
	
	pdu->indata.data = NULL;
	pdu->indata.size = 0;

	if (pdu->callback) {
		pdu->callback(iscsi, status, task, pdu->private_data);
	}

	return 0;
}

int
iscsi_process_r2t(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			 struct iscsi_in_pdu *in)
{
	uint32_t ttt, offset, len;

	ttt    = scsi_get_uint32(&in->hdr[20]);
	offset = scsi_get_uint32(&in->hdr[40]);
	len    = scsi_get_uint32(&in->hdr[44]);

	pdu->datasn = 0;
	iscsi_send_data_out(iscsi, pdu, ttt, offset, len);
	return 0;
}

/*
 * SCSI commands
 */

struct scsi_task *
iscsi_testunitready_task(struct iscsi_context *iscsi, int lun,
			  iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_testunitready();
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"testunitready cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_reportluns_task(struct iscsi_context *iscsi, int report_type,
		       int alloc_len, iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	if (alloc_len < 16) {
		iscsi_set_error(iscsi, "Minimum allowed alloc len for "
				"reportluns is 16. You specified %d.",
				alloc_len);
		return NULL;
	}

	task = scsi_reportluns_cdb(report_type, alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"reportluns cdb.");
		return NULL;
	}
	/* report luns are always sent to lun 0 */
	if (iscsi_scsi_command_async(iscsi, 0, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_inquiry_task(struct iscsi_context *iscsi, int lun, int evpd,
		    int page_code, int maxsize,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_inquiry(evpd, page_code, maxsize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"inquiry cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_readcapacity10_task(struct iscsi_context *iscsi, int lun, int lba,
			   int pmi, iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_readcapacity10(lba, pmi);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"readcapacity10 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_readcapacity16_task(struct iscsi_context *iscsi, int lun,
			   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_readcapacity16();
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"readcapacity16 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_readdefectdata10_task(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format, uint16_t alloc_len,
                            iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_readdefectdata10(req_plist, req_glist,
                                         defect_list_format, alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"readdefectdata10 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_readdefectdata12_task(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format,
                            uint32_t address_descriptor_index,
                            uint32_t alloc_len,
                            iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_readdefectdata12(req_plist, req_glist,
                                         defect_list_format,
                                         address_descriptor_index, alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"readdefectdata12 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_get_lba_status_task(struct iscsi_context *iscsi, int lun,
			  uint64_t starting_lba, uint32_t alloc_len,
			  iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_get_lba_status(starting_lba, alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"get-lba-status cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read6_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read6(lba, datalen, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read6 cdb.");
		return NULL;
	}

	if (iov != NULL)
		scsi_task_set_iov_in(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read6_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read6(lba, datalen, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read6 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		  iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read10(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read10 cdb.");
		return NULL;
	}

	if (iov != NULL)
		scsi_task_set_iov_in(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}
struct scsi_task *
iscsi_read10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		  iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read10(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read10 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read12_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read12(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read12 cdb.");
		return NULL;
	}

	if (iov != NULL)
		scsi_task_set_iov_in(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read12_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read12(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read12 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read16(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read16 cdb.");
		return NULL;
	}

	if (iov != NULL)
		scsi_task_set_iov_in(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_read16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_read16(lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read16 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_write10_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_write10(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"write10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}
		
	return task;
}

struct scsi_task *
iscsi_write10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_write10(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"write10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);


	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}
		
	return task;
}

struct scsi_task *
iscsi_write12_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_write12(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"write12 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_write12_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_write12(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"write12 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_write16_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_write16(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"write16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_write16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_write16(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"write16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeatomic16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			 unsigned char *data, uint32_t datalen, int blocksize,
			 int wrprotect, int dpo, int fua, int group_number,
			 iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeatomic16(lba, datalen, blocksize, wrprotect,
				      dpo, fua, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeAtomic16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeatomic16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int fua, int group_number,
			     iscsi_command_cb cb, void *private_data,
			     struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeatomic16(lba, datalen, blocksize, wrprotect,
				      dpo, fua, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeAtomic16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_orwrite_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_orwrite(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"orwrite cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_orwrite_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_orwrite(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"orwrite cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_compareandwrite_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % (blocksize * 2) != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize * 2:%d.", datalen, blocksize * 2);
		return NULL;
	}

	task = scsi_cdb_compareandwrite(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"compareandwrite cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_compareandwrite_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
			       unsigned char *data, uint32_t datalen, int blocksize,
			       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
			       iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % (blocksize * 2) != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize * 2:%d.", datalen, blocksize * 2);
		return NULL;
	}

	task = scsi_cdb_compareandwrite(lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"compareandwrite cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeverify10_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeverify10(lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeverify10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeverify10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     iscsi_command_cb cb, void *private_data,
			     struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeverify10(lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeverify10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeverify12_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeverify12(lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeverify12 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeverify12_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba, 
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     iscsi_command_cb cb, void *private_data,
			     struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeverify12(lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeverify12 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeverify16_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeverify16(lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeverify16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writeverify16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba, 
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     iscsi_command_cb cb, void *private_data,
			     struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_writeverify16(lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writeverify16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_verify10_task(struct iscsi_context *iscsi, int lun, unsigned char *data,
		    uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_verify10(lba, datalen, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"verify10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_verify10_iov_task(struct iscsi_context *iscsi, int lun, unsigned char *data,
			uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize,
			iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_verify10(lba, datalen, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"verify10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_verify12_task(struct iscsi_context *iscsi, int lun, unsigned char *data,
		    uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_verify12(lba, datalen, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"verify12 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_verify12_iov_task(struct iscsi_context *iscsi, int lun, unsigned char *data,
			uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize,
			iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_verify12(lba, datalen, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"verify12 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_verify16_task(struct iscsi_context *iscsi, int lun, unsigned char *data,
		    uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_verify16(lba, datalen, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"verify16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_verify16_iov_task(struct iscsi_context *iscsi, int lun, unsigned char *data,
			uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize,
			iscsi_command_cb cb, void *private_data, struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	if (datalen % blocksize != 0) {
		iscsi_set_error(iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return NULL;
	}

	task = scsi_cdb_verify16(lba, datalen, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"verify16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_modeselect6_task(struct iscsi_context *iscsi, int lun,
		       int pf, int sp, struct scsi_mode_page *mp,
		       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct scsi_data *data;
	struct iscsi_data d;

	task = scsi_cdb_modeselect6(pf, sp, 255); 
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"modeselect6 cdb.");
		return NULL;
	}
	data = scsi_modesense_dataout_marshall(task, mp, 1);
	if (data == NULL) {
		iscsi_set_error(iscsi, "Error: Failed to marshall "
				"modesense dataout buffer.");
		scsi_free_scsi_task(task);
		return NULL;
	}

	d.data = data->data;
	d.size = data->size;
	task->cdb[4] = data->size;
	task->expxferlen = data->size;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_modeselect10_task(struct iscsi_context *iscsi, int lun,
			int pf, int sp, struct scsi_mode_page *mp,
			iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct scsi_data *data;
	struct iscsi_data d;

	task = scsi_cdb_modeselect10(pf, sp, 255); 
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"modeselect10 cdb.");
		return NULL;
	}
	data = scsi_modesense_dataout_marshall(task, mp, 0);
	if (data == NULL) {
		iscsi_set_error(iscsi, "Error: Failed to marshall "
				"modesense dataout buffer.");
		scsi_free_scsi_task(task);
		return NULL;
	}

	d.data = data->data;
	d.size = data->size;
	task->cdb[7] = data->size >> 8;
	task->cdb[8] = data->size & 0xff;

	task->expxferlen = data->size;

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_modesense6_task(struct iscsi_context *iscsi, int lun, int dbd, int pc,
		       int page_code, int sub_page_code,
		       unsigned char alloc_len,
		       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_modesense6(dbd, pc, page_code, sub_page_code,
				   alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"modesense6 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_modesense10_task(struct iscsi_context *iscsi, int lun,
		       int llbaa, int dbd, int pc,
		       int page_code, int sub_page_code,
		       unsigned char alloc_len,
		       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_modesense10(llbaa, dbd, pc, page_code, sub_page_code,
				   alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"modesense10 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_startstopunit_task(struct iscsi_context *iscsi, int lun,
			 int immed, int pcm, int pc,
			 int no_flush, int loej, int start,
			 iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_startstopunit(immed, pcm, pc, no_flush,
				      loej, start);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"startstopunit cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_preventallow_task(struct iscsi_context *iscsi, int lun,
			int prevent,
			iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_preventallow(prevent);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"PreventAllowMediumRemoval cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_synchronizecache10_task(struct iscsi_context *iscsi, int lun, int lba,
			       int num_blocks, int syncnv, int immed,
			       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_synchronizecache10(lba, num_blocks, syncnv,
					   immed);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"synchronizecache10 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_synchronizecache16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			       uint32_t num_blocks, int syncnv, int immed,
			       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_synchronizecache16(lba, num_blocks, syncnv,
					   immed);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"synchronizecache16 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_persistent_reserve_in_task(struct iscsi_context *iscsi, int lun,
				 int sa, uint16_t xferlen,
				 iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_persistent_reserve_in(sa, xferlen);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"persistent-reserver-in cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_persistent_reserve_out_task(struct iscsi_context *iscsi, int lun,
				  int sa, int scope, int type, void *param,
				  iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_persistent_reserve_out(sa, scope, type, param);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"persistent-reserver-out cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_prefetch10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      int num_blocks, int immed, int group,
		      iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_prefetch10(lba, num_blocks, immed, group);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"prefetch10 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_prefetch16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		      int num_blocks, int immed, int group,
		      iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_prefetch16(lba, num_blocks, immed, group);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"prefetch16 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writesame10_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint16_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	task = scsi_cdb_writesame10(wrprotect, anchor, unmap, lba, group,
				    num_blocks, datalen);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writesame10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (data != NULL) {
		task->expxferlen = datalen;
	} else {
		task->expxferlen = 0;
		task->xfer_dir = SCSI_XFER_NONE;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}
	return task;
}

struct scsi_task *
iscsi_writesame10_iov_task(struct iscsi_context *iscsi, int lun, uint32_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint16_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   iscsi_command_cb cb, void *private_data,
			   struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	task = scsi_cdb_writesame10(wrprotect, anchor, unmap, lba, group,
				    num_blocks, datalen);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writesame10 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (data != NULL) {
		task->expxferlen = datalen;
	} else {
		task->expxferlen = 0;
		task->xfer_dir = SCSI_XFER_NONE;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}
	return task;
}

struct scsi_task *
iscsi_writesame16_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint32_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_data d;

	task = scsi_cdb_writesame16(wrprotect, anchor, unmap, lba, group,
				    num_blocks, datalen);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writesame16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (data != NULL) {
		task->expxferlen = datalen;
	} else {
		task->expxferlen = 0;
		task->xfer_dir = SCSI_XFER_NONE;
	}

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_writesame16_iov_task(struct iscsi_context *iscsi, int lun, uint64_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint32_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   iscsi_command_cb cb, void *private_data,
			   struct scsi_iovec *iov, int niov)
{
	struct scsi_task *task;
	struct iscsi_data d;

	task = scsi_cdb_writesame16(wrprotect, anchor, unmap, lba, group,
				    num_blocks, datalen);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"writesame16 cdb.");
		return NULL;
	}
	d.data = data;
	d.size = datalen;

	if (iov != NULL)
		scsi_task_set_iov_out(task, iov, niov);

	if (data != NULL) {
		task->expxferlen = datalen;
	} else {
		task->expxferlen = 0;
		task->xfer_dir = SCSI_XFER_NONE;
	}

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     &d, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_unmap_task(struct iscsi_context *iscsi, int lun, int anchor, int group,
		 struct unmap_list *list, int list_len,
		 iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct scsi_iovec *iov;
	unsigned char *data;
	int xferlen;
	int i;

	xferlen = 8 + list_len * 16;

	task = scsi_cdb_unmap(anchor, group, xferlen);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"unmap cdb.");
		return NULL;
	}

	data = scsi_malloc(task, xferlen);
	if (data == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"unmap parameters.");
		scsi_free_scsi_task(task);
		return NULL;
	}
	scsi_set_uint16(&data[0], xferlen - 2);
	scsi_set_uint16(&data[2], xferlen - 8);
	for (i = 0; i < list_len; i++) {
		scsi_set_uint32(&data[8 + 16 * i], list[i].lba >> 32);
		scsi_set_uint32(&data[8 + 16 * i + 4], list[i].lba & 0xffffffff);
		scsi_set_uint32(&data[8 + 16 * i + 8], list[i].num);
	}

	iov = scsi_malloc(task, sizeof(struct scsi_iovec));
	if (iov == NULL) {
		scsi_free_scsi_task(task);
		return NULL;
	}
	iov->iov_base = data;
	iov->iov_len  = xferlen;
	scsi_task_set_iov_out(task, iov, 1);

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_iovector *
iscsi_get_scsi_task_iovector_in(struct iscsi_context *iscsi, struct iscsi_in_pdu *in)
{
	struct iscsi_pdu *pdu;
	uint32_t itt;

	if ((in->hdr[0] & 0x3f) != ISCSI_PDU_DATA_IN) {
		return NULL;
	}

	itt = scsi_get_uint32(&in->hdr[16]);
	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		if (pdu->itt == itt) {
			break;
		}
	}

	if (pdu == NULL) {
		return NULL;
	}

	if (pdu->scsi_cbdata.task->iovector_in.iov == NULL) {
		return NULL;
	}

	return &pdu->scsi_cbdata.task->iovector_in;
}

struct scsi_iovector *
iscsi_get_scsi_task_iovector_out(struct iscsi_context *iscsi _U_, struct iscsi_pdu *pdu)
{
	if (pdu->scsi_cbdata.task->iovector_out.iov == NULL) {
		return NULL;
	}

	return &pdu->scsi_cbdata.task->iovector_out;
}

struct scsi_task *
iscsi_readtoc_task(struct iscsi_context *iscsi, int lun, int msf,
		   int format, int track_session, int maxsize,
		   iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_readtoc(msf, format, track_session, maxsize);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"read TOC cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_reserve6_task(struct iscsi_context *iscsi, int lun,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_reserve6();
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"reserve6 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_release6_task(struct iscsi_context *iscsi, int lun,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_release6();
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"release6 cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}


struct scsi_task *
iscsi_sanitize_task(struct iscsi_context *iscsi, int lun,
		    int immed, int ause, int sa, int param_len,
		    struct iscsi_data *data,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_sanitize(immed, ause, sa, param_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"sanitize cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     data, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_sanitize_block_erase_task(struct iscsi_context *iscsi, int lun,
		    int immed, int ause,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_sanitize(immed, ause, SCSI_SANITIZE_BLOCK_ERASE, 0);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"sanitize cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_sanitize_crypto_erase_task(struct iscsi_context *iscsi, int lun,
		    int immed, int ause,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_sanitize(immed, ause, SCSI_SANITIZE_CRYPTO_ERASE, 0);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"sanitize cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_sanitize_exit_failure_mode_task(struct iscsi_context *iscsi, int lun,
		    int immed, int ause,
		    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_sanitize(immed, ause,
				 SCSI_SANITIZE_EXIT_FAILURE_MODE, 0);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"sanitize cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_report_supported_opcodes_task(struct iscsi_context *iscsi, int lun,
				    int rctd, int options,
				    int opcode, int sa,
				    uint32_t alloc_len,
				    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_report_supported_opcodes(rctd, options, opcode, sa,
						 alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"Maintenance In/Read Supported Op Codes cdb.");
		return NULL;
	}
	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_receive_copy_results_task(struct iscsi_context *iscsi, int lun,
				int sa, int list_id, int alloc_len,
				iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_receive_copy_results(sa, list_id, alloc_len);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"RECEIVE COPY RESULTS cdb.");
		return NULL;
	}

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     NULL, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_extended_copy_task(struct iscsi_context *iscsi, int lun,
			 struct iscsi_data *param_data,
			 iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;

	task = scsi_cdb_extended_copy(param_data->size);
	if (task == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to create "
				"EXTENDED COPY cdb.");
		return NULL;
	}

	if (iscsi_scsi_command_async(iscsi, lun, task, cb,
				     param_data, private_data) != 0) {
		scsi_free_scsi_task(task);
		return NULL;
	}

	return task;
}

struct scsi_task *
iscsi_scsi_get_task_from_pdu(struct iscsi_pdu *pdu)
{
	return pdu->scsi_cbdata.task;
}

int
iscsi_scsi_cancel_task(struct iscsi_context *iscsi,
		       struct scsi_task *task)
{
	struct iscsi_pdu *pdu;

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		if (pdu->itt == task->itt) {
			ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
			}
			iscsi->drv->free_pdu(iscsi, pdu);
			return 0;
		}
	}
	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		if (pdu->itt == task->itt) {
			ISCSI_LIST_REMOVE(&iscsi->outqueue, pdu);
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
			}
			iscsi->drv->free_pdu(iscsi, pdu);
			return 0;
		}
	}
	return -1;
}

void
iscsi_scsi_cancel_all_tasks(struct iscsi_context *iscsi)
{
	iscsi_cancel_pdus(iscsi);
}
