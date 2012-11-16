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

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "slist.h"

struct iscsi_cbdata {
	struct iscsi_cbdata *prev, *next;
	iscsi_command_cb          callback;
	void                     *private_data;
	struct iscsi_task        *task;
};

struct iscsi_task* iscsi_create_task(struct iscsi_context *iscsi)
{
	struct iscsi_task* task;
	task = iscsi_zmalloc(iscsi, sizeof(struct iscsi_task));
	
	if (task == NULL) return NULL;
	
	SLIST_ADD_END(&iscsi->tasks, task);
	
	task->iscsi = iscsi;
	task->iscsi->tasks_created++;
	
	return task;
}

void iscsi_clear_task_not_zeroed(struct iscsi_task *task)
{
	if (task->scsi.datain.data != NULL) {
		iscsi_free(task->iscsi, task->scsi.datain.data);
		task->scsi.datain.data = NULL;
	}
		
	scsi_clear_task(&task->scsi);
}

void iscsi_clear_task(struct iscsi_task *task)
{
	iscsi_clear_task_not_zeroed(task);
	memset(&task->scsi, 0, sizeof(struct scsi_task));
}

void iscsi_free_task(struct iscsi_task *task)
{
	iscsi_clear_task_not_zeroed(task);

	SLIST_REMOVE(&task->iscsi->tasks, task);
	task->iscsi->tasks_freed++;
	iscsi_free(task->iscsi, task);
}

void
iscsi_free_cbdata(struct iscsi_context *iscsi, struct iscsi_cbdata *iscsi_cbdata)
{
	if (iscsi_cbdata == NULL) {
		return;
	}
	if (iscsi_cbdata->task != NULL) {
		iscsi_cbdata->task = NULL;
	}
	iscsi_free(iscsi, iscsi_cbdata);
}

static void
iscsi_scsi_response_cb(struct iscsi_context *iscsi, int status,
		       void *command_data _U_, void *private_data)
{
	struct iscsi_cbdata *iscsi_cbdata =
	  (struct iscsi_cbdata *)private_data;

	switch (status) {
	case SCSI_STATUS_RESERVATION_CONFLICT:
	case SCSI_STATUS_CHECK_CONDITION:
	case SCSI_STATUS_GOOD:
	case SCSI_STATUS_ERROR:
	case SCSI_STATUS_CANCELLED:
		iscsi_cbdata->callback(iscsi, status, iscsi_cbdata->task,
				      iscsi_cbdata->private_data);
		return;
	default:
		iscsi_set_error(iscsi, "Cant handle  scsi status %d yet.",
				status);
		iscsi_cbdata->callback(iscsi, SCSI_STATUS_ERROR, iscsi_cbdata->task,
				      iscsi_cbdata->private_data);
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

		if (len > iscsi->target_max_recv_data_segment_length) {
			len = iscsi->target_max_recv_data_segment_length;
		}

		pdu = iscsi_allocate_pdu_with_itt_flags(iscsi, ISCSI_PDU_DATA_OUT,
				 ISCSI_PDU_NO_PDU,
				 cmd_pdu->itt,
				 ISCSI_PDU_DELETE_WHEN_SENT|ISCSI_PDU_NO_CALLBACK);
		if (pdu == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory, Failed to allocate "
				"scsi data out pdu.");
			SLIST_REMOVE(&iscsi->outqueue, cmd_pdu);
			SLIST_REMOVE(&iscsi->waitpdu, cmd_pdu);
			cmd_pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				     cmd_pdu->private_data);
			iscsi_free_pdu(iscsi, cmd_pdu);
			return -1;

		}

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

		/* exp statsn */
		iscsi_pdu_set_expstatsn(pdu, iscsi->statsn+1);

		/* data sn */
		iscsi_pdu_set_datasn(pdu, cmd_pdu->datasn++);

		/* buffer offset */
		iscsi_pdu_set_bufferoffset(pdu, offset);

		if (iscsi_pdu_add_data(iscsi, pdu, cmd_pdu->nidata.data + offset, len)
		    != 0) {
		    	iscsi_set_error(iscsi, "Out-of-memory: Failed to "
				"add outdata to the pdu.");
			SLIST_REMOVE(&iscsi->outqueue, cmd_pdu);
			SLIST_REMOVE(&iscsi->waitpdu, cmd_pdu);
			cmd_pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				     cmd_pdu->private_data);
			iscsi_free_pdu(iscsi, cmd_pdu);
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}

		pdu->callback     = cmd_pdu->callback;
		pdu->private_data = cmd_pdu->private_data;

		if (iscsi_queue_pdu(iscsi, pdu) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"scsi pdu.");
			SLIST_REMOVE(&iscsi->outqueue, cmd_pdu);
			SLIST_REMOVE(&iscsi->waitpdu, cmd_pdu);
			cmd_pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				     cmd_pdu->private_data);
			iscsi_free_pdu(iscsi, cmd_pdu);
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}

		tot_len -= len;
		offset  += len;
	}
	return 0;
}

int
iscsi_scsi_command_async(struct iscsi_task *task, int lun,
			    iscsi_command_cb cb, struct iscsi_data *d, void *private_data)
{
	struct iscsi_pdu *pdu;
	struct iscsi_cbdata *iscsi_cbdata;
	struct iscsi_data data;
	uint32_t offset = 0;

	int flags;

	data.data = (d != NULL) ? d->data : NULL;
	data.size = (d != NULL) ? d->size : 0;

	if (task->iscsi->session_type != ISCSI_SESSION_NORMAL) {
		iscsi_set_error(task->iscsi, "Trying to send command on "
				"discovery session.");
		return -1;
	}

	if (task->iscsi->is_loggedin == 0) {
		iscsi_set_error(task->iscsi, "Trying to send command while "
				"not logged in.");
		return -1;
	}

	iscsi_cbdata = iscsi_zmalloc(task->iscsi, sizeof(struct iscsi_cbdata));
	if (iscsi_cbdata == NULL) {
		iscsi_set_error(task->iscsi, "Out-of-memory: failed to allocate "
				"scsi cbdata.");
		return -1;
	}

	iscsi_cbdata->task         = task;
	iscsi_cbdata->callback     = cb;
	iscsi_cbdata->private_data = private_data;

	scsi_set_task_private_ptr(&task->scsi, iscsi_cbdata);

	pdu = iscsi_allocate_pdu(task->iscsi, ISCSI_PDU_SCSI_REQUEST,
				 ISCSI_PDU_SCSI_RESPONSE);
	if (pdu == NULL) {
		iscsi_set_error(task->iscsi, "Out-of-memory, Failed to allocate "
				"scsi pdu.");
		iscsi_free_cbdata(task->iscsi, iscsi_cbdata);
		return -1;
	}
	pdu->iscsi_cbdata = iscsi_cbdata;

	/* flags */
	flags = ISCSI_PDU_SCSI_FINAL|ISCSI_PDU_SCSI_ATTR_SIMPLE;
	switch (task->scsi.xfer_dir) {
	case SCSI_XFER_NONE:
		break;
	case SCSI_XFER_READ:
		flags |= ISCSI_PDU_SCSI_READ;
		break;
	case SCSI_XFER_WRITE:
		flags |= ISCSI_PDU_SCSI_WRITE;
		if (data.size == 0) {
			iscsi_set_error(task->iscsi, "DATA-OUT command but data "
					"== NULL.");
			iscsi_free_pdu(task->iscsi, pdu);
			return -1;
		}
		if (data.size != task->scsi.expxferlen) {
			iscsi_set_error(task->iscsi, "Data size:%d is not same as "
					"expected data transfer "
					"length:%d.", data.size,
					task->scsi.expxferlen);
			iscsi_free_pdu(task->iscsi, pdu);
			return -1;
		}

		/* Assume all data is non-immediate data */
		pdu->nidata.data = data.data;
		pdu->nidata.size = data.size;

		/* Are we allowed to send immediate data ? */
		if (task->iscsi->use_immediate_data == ISCSI_IMMEDIATE_DATA_YES) {
			uint32_t len = data.size;

			if (len > task->iscsi->first_burst_length) {
				len = task->iscsi->first_burst_length;
			}

			if (iscsi_pdu_add_data(task->iscsi, pdu, data.data, len)
			    != 0) {
			    	iscsi_set_error(task->iscsi, "Out-of-memory: Failed to "
						"add outdata to the pdu.");
				iscsi_free_pdu(task->iscsi, pdu);
				return -1;
			}
			offset = len;

			if (len == (uint32_t)data.size) {
				/* We managed to send it all as immediate data, so there is no non-immediate data left */
				pdu->nidata.data = NULL;
				pdu->nidata.size = 0;

			}
		}

		if (pdu->nidata.size > 0 && task->iscsi->use_initial_r2t == ISCSI_INITIAL_R2T_NO) {
			/* We have more data to send, and we are allowed to send
			 * unsolicited data, so dont flag this PDU as final.
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
	iscsi_pdu_set_expxferlen(pdu, task->scsi.expxferlen);

	/* cmdsn */
	iscsi_pdu_set_cmdsn(pdu, task->iscsi->cmdsn);
	pdu->cmdsn = task->iscsi->cmdsn;
	task->iscsi->cmdsn++;

	/* exp statsn */
	iscsi_pdu_set_expstatsn(pdu, task->iscsi->statsn+1);
	
	/* cdb */
	iscsi_pdu_set_cdb(pdu, task);

	pdu->callback     = iscsi_scsi_response_cb;
	pdu->private_data = iscsi_cbdata;

	if (iscsi_queue_pdu(task->iscsi, pdu) != 0) {
		iscsi_set_error(task->iscsi, "Out-of-memory: failed to queue iscsi "
				"scsi pdu.");
		iscsi_free_pdu(task->iscsi, pdu);
		return -1;
	}

	/* Can we send some unsolicited data ? */
	if (pdu->nidata.size != 0 && task->iscsi->use_initial_r2t == ISCSI_INITIAL_R2T_NO && task->iscsi->use_immediate_data == ISCSI_IMMEDIATE_DATA_NO) {
		uint32_t len = pdu->nidata.size - offset;

		if (len > task->iscsi->first_burst_length) {
			len = task->iscsi->first_burst_length;
		}
		iscsi_send_data_out(task->iscsi, pdu, 0xffffffff, offset, len);
	}

	/* remember cmdsn and itt so we can use task management */
	task->scsi.cmdsn = pdu->cmdsn;
	task->scsi.itt   = pdu->itt;
	task->scsi.lun   = lun;

	return 0;
}


int
iscsi_process_scsi_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			 struct iscsi_in_pdu *in)
{
	uint32_t statsn, maxcmdsn, flags, status;
	struct iscsi_cbdata *iscsi_cbdata = pdu->iscsi_cbdata;
	struct iscsi_task *task = iscsi_cbdata->task;

	statsn = ntohl(*(uint32_t *)&in->hdr[24]);
	if (statsn > iscsi->statsn) {
		iscsi->statsn = statsn;
	}

	maxcmdsn = ntohl(*(uint32_t *)&in->hdr[32]);
	if (maxcmdsn > iscsi->maxcmdsn) {
		iscsi->maxcmdsn = maxcmdsn;
	}

	flags = in->hdr[1];
	if ((flags&ISCSI_PDU_DATA_FINAL) == 0) {
		iscsi_set_error(task->iscsi, "scsi response pdu but Final bit is "
				"not set: 0x%02x.", flags);
		pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			      pdu->private_data);
		return -1;
	}
	if ((flags&ISCSI_PDU_DATA_ACK_REQUESTED) != 0) {
		iscsi_set_error(task->iscsi, "scsi response asked for ACK "
				"0x%02x.", flags);
		pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			      pdu->private_data);
		return -1;
	}

	status = in->hdr[3];

	switch (status) {
	case SCSI_STATUS_GOOD:
		task->scsi.datain.data = pdu->indata.data;
		task->scsi.datain.size = pdu->indata.size;

		task->scsi.residual_status = SCSI_RESIDUAL_NO_RESIDUAL;
		task->scsi.residual = 0;

		/*
		 * These flags should only be set if the S flag is also set
		 */
		if (flags & (ISCSI_PDU_DATA_RESIDUAL_OVERFLOW|ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW)) {
			task->scsi.residual = ntohl(*((uint32_t *)&in->hdr[44]));
			if (flags & ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW) {
				task->scsi.residual_status = SCSI_RESIDUAL_UNDERFLOW;
			} else {
				task->scsi.residual_status = SCSI_RESIDUAL_OVERFLOW;
			}
		}

		pdu->indata.data = NULL;
		pdu->indata.size = 0;

		pdu->callback(iscsi, SCSI_STATUS_GOOD, task,
			      pdu->private_data);
		break;
	case SCSI_STATUS_CHECK_CONDITION:
		task->scsi.datain.size = in->data_pos;
		task->scsi.datain.data = malloc(task->scsi.datain.size);
		if (task->scsi.datain.data == NULL) {
			iscsi_set_error(task->iscsi, "failed to allocate blob for "
					"sense data");
		}
		memcpy(task->scsi.datain.data, in->data, task->scsi.datain.size);

		task->scsi.sense.error_type = task->scsi.datain.data[2] & 0x7f;
		task->scsi.sense.key        = task->scsi.datain.data[4] & 0x0f;
		task->scsi.sense.ascq       = ntohs(*(uint16_t *)
					       &(task->scsi.datain.data[14]));

		iscsi_set_error(task->iscsi, "SENSE KEY:%s(%d) ASCQ:%s(0x%04x)",
				scsi_sense_key_str(task->scsi.sense.key),
				task->scsi.sense.key,
				scsi_sense_ascq_str(task->scsi.sense.ascq),
				task->scsi.sense.ascq);
		pdu->callback(iscsi, SCSI_STATUS_CHECK_CONDITION, task,
			      pdu->private_data);
		break;
	case SCSI_STATUS_RESERVATION_CONFLICT:
		iscsi_set_error(task->iscsi, "RESERVATION CONFLICT");
		pdu->callback(iscsi, SCSI_STATUS_RESERVATION_CONFLICT,
			task, pdu->private_data);
		break;
	default:
		iscsi_set_error(task->iscsi, "Unknown SCSI status :%d.", status);

		pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			      pdu->private_data);
		return -1;
	}

	return 0;
}

int
iscsi_process_scsi_data_in(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			   struct iscsi_in_pdu *in, int *is_finished)
{
	uint32_t statsn, maxcmdsn, flags, status;
	struct iscsi_cbdata *iscsi_cbdata = pdu->iscsi_cbdata;
	struct iscsi_task *task = iscsi_cbdata->task;
	int dsl;

	statsn = ntohl(*(uint32_t *)&in->hdr[24]);
	if (statsn > iscsi->statsn) {
		iscsi->statsn = statsn;
	}

	maxcmdsn = ntohl(*(uint32_t *)&in->hdr[32]);
	if (maxcmdsn > iscsi->maxcmdsn) {
		iscsi->maxcmdsn = maxcmdsn;
	}

	flags = in->hdr[1];
	if ((flags&ISCSI_PDU_DATA_ACK_REQUESTED) != 0) {
		iscsi_set_error(task->iscsi, "scsi response asked for ACK "
				"0x%02x.", flags);
		pdu->callback(iscsi, SCSI_STATUS_ERROR, task,
			      pdu->private_data);
		return -1;
	}
	dsl = ntohl(*(uint32_t *)&in->hdr[4])&0x00ffffff;

	/* Dont add to reassembly buffer if we already have a user buffer */
	if (scsi_task_get_data_in_buffer(&task->scsi, 0, NULL) == NULL) {
		if (iscsi_add_data(iscsi, &pdu->indata,
				   in->data, dsl, 0)
		    != 0) {
		    	iscsi_set_error(task->iscsi, "Out-of-memory: failed to add data "
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

	task->scsi.residual_status = SCSI_RESIDUAL_NO_RESIDUAL;
	task->scsi.residual = 0;

	/*
	 * These flags should only be set if the S flag is also set
	 */
	if (flags & (ISCSI_PDU_DATA_RESIDUAL_OVERFLOW|ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW)) {
		task->scsi.residual = ntohl(*((uint32_t *)&in->hdr[44]));
		if (flags & ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW) {
			task->scsi.residual_status = SCSI_RESIDUAL_UNDERFLOW;
		} else {
			task->scsi.residual_status = SCSI_RESIDUAL_OVERFLOW;
		}
	}


	/* this was the final data-in packet in the sequence and it has
	 * the s-bit set, so invoke the callback.
	 */
	status = in->hdr[3];
	task->scsi.datain.data = pdu->indata.data;
	task->scsi.datain.size = pdu->indata.size;

	pdu->indata.data = NULL;
	pdu->indata.size = 0;

	pdu->callback(iscsi, status, task, pdu->private_data);

	return 0;
}

int
iscsi_process_r2t(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			 struct iscsi_in_pdu *in)
{
	uint32_t ttt, offset, len, maxcmdsn;

	ttt    = ntohl(*(uint32_t *)&in->hdr[20]);
	offset = ntohl(*(uint32_t *)&in->hdr[40]);
	len    = ntohl(*(uint32_t *)&in->hdr[44]);

	maxcmdsn = ntohl(*(uint32_t *)&in->hdr[32]);
	if (maxcmdsn > iscsi->maxcmdsn) {
		iscsi->maxcmdsn = maxcmdsn;
	}

	pdu->datasn = 0;
	iscsi_send_data_out(iscsi, pdu, ttt, offset, len);
	return 0;
}

/*
 * SCSI commands
 */

int
iscsi_testunitready_task(struct iscsi_task *task, int lun,
			  iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_testunitready(&task->scsi);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_reportluns_task(struct iscsi_task *task, int report_type,
		       int alloc_len, iscsi_command_cb cb, void *private_data)
{
	if (alloc_len < 16) {
		iscsi_set_error(task->iscsi, "Minimum allowed alloc len for "
				"reportluns is 16. You specified %d.",
				alloc_len);
		return -1;
	}

	scsi_reportluns_cdb(&task->scsi, report_type, alloc_len);

	/* report luns are always sent to lun 0 */
	return iscsi_scsi_command_async(task, 0, cb, NULL,
				     private_data);
}

int
iscsi_inquiry_task(struct iscsi_task *task, int lun, int evpd,
		    int page_code, int maxsize,
		    iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_inquiry(&task->scsi, evpd, page_code, maxsize);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_readcapacity10_task(struct iscsi_task *task, int lun, int lba,
			   int pmi, iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_readcapacity10(&task->scsi, lba, pmi);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_readcapacity16_task(struct iscsi_task *task, int lun,
			   iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_readcapacity16(&task->scsi);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_get_lba_status_task(struct iscsi_task *task, int lun,
			  uint64_t starting_lba, uint32_t alloc_len,
			  iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_get_lba_status(&task->scsi, starting_lba, alloc_len);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_read6_task(struct iscsi_task *task, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   iscsi_command_cb cb, void *private_data)
{
	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_read6(&task->scsi, lba, datalen, blocksize);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_read10_task(struct iscsi_task *task, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		  iscsi_command_cb cb, void *private_data)
{
	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_read10(&task->scsi, lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_read12_task(struct iscsi_task *task, int lun, uint32_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_read12(&task->scsi, lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_read16_task(struct iscsi_task *task, int lun, uint64_t lba,
		   uint32_t datalen, int blocksize,
		   int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of "
				"the blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_read16(&task->scsi, lba, datalen, blocksize, rdprotect,
				dpo, fua, fua_nv, group_number);
	
	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_write10_task(struct iscsi_task *task, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_write10(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_write12_task(struct iscsi_task *task, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_write12(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_write16_task(struct iscsi_task *task, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_write16(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_orwrite_task(struct iscsi_task *task, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_orwrite(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_compareandwrite_task(struct iscsi_task *task, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_compareandwrite(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, fua, fua_nv, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_writeverify10_task(struct iscsi_task *task, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_writeverify10(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_writeverify12_task(struct iscsi_task *task, int lun, uint32_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_writeverify12(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_writeverify16_task(struct iscsi_task *task, int lun, uint64_t lba, 
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number,
		   iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_writeverify16(&task->scsi, lba, datalen, blocksize, wrprotect,
				dpo, bytchk, group_number);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_verify10_task(struct iscsi_task *task, int lun, unsigned char *data,
		    uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize,
		    iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_verify10(&task->scsi, lba, datalen, vprotect, dpo, bytchk, blocksize);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_verify12_task(struct iscsi_task *task, int lun, unsigned char *data,
		    uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize,
		    iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_verify12(&task->scsi, lba, datalen, vprotect, dpo, bytchk, blocksize);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_verify16_task(struct iscsi_task *task, int lun, unsigned char *data,
		    uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize,
		    iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	if (datalen % blocksize != 0) {
		iscsi_set_error(task->iscsi, "Datalen:%d is not a multiple of the "
				"blocksize:%d.", datalen, blocksize);
		return -1;
	}

	scsi_cdb_verify16(&task->scsi, lba, datalen, vprotect, dpo, bytchk, blocksize);

	outdata.data = data;
	outdata.size = datalen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

int
iscsi_modesense6_task(struct iscsi_task *task, int lun, int dbd, int pc,
		       int page_code, int sub_page_code,
		       unsigned char alloc_len,
		       iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_modesense6(&task->scsi, dbd, pc, page_code, sub_page_code,
				   alloc_len);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_startstopunit_task(struct iscsi_task *task, int lun,
			 int immed, int pcm, int pc,
			 int no_flush, int loej, int start,
			 iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_startstopunit(&task->scsi, immed, pcm, pc, no_flush,
				      loej, start);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_preventallow_task(struct iscsi_task *task, int lun,
			int prevent,
			iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_preventallow(&task->scsi, prevent);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_synchronizecache10_task(struct iscsi_task *task, int lun, int lba,
			       int num_blocks, int syncnv, int immed,
			       iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_synchronizecache10(&task->scsi, lba, num_blocks, syncnv,
					   immed);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_synchronizecache16_task(struct iscsi_task *task, int lun, uint64_t lba,
			       uint32_t num_blocks, int syncnv, int immed,
			       iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_synchronizecache16(&task->scsi, lba, num_blocks, syncnv,
					   immed);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_prefetch10_task(struct iscsi_task *task, int lun, uint32_t lba,
		      int num_blocks, int immed, int group,
		      iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_prefetch10(&task->scsi, lba, num_blocks, immed, group);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_prefetch16_task(struct iscsi_task *task, int lun, uint64_t lba,
		      int num_blocks, int immed, int group,
		      iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_prefetch16(&task->scsi, lba, num_blocks, immed, group);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_writesame10_task(struct iscsi_task *task, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint32_t lba, uint16_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	scsi_cdb_writesame10(&task->scsi, wrprotect, anchor, unmap, pbdata, lbdata, lba, group, num_blocks);

	if (datalen) {
		outdata.data = data;
		outdata.size = datalen;
		task->scsi.expxferlen = datalen;

		return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
	} 

	task->scsi.expxferlen = 0;
	task->scsi.xfer_dir = SCSI_XFER_NONE;

	return iscsi_scsi_command_async(task, lun, cb, NULL,
			       private_data);
}

int
iscsi_writesame16_task(struct iscsi_task *task, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint64_t lba, uint32_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group,
		       iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;

	scsi_cdb_writesame16(&task->scsi, wrprotect, anchor, unmap, pbdata, lbdata, lba, group, num_blocks);

	if (datalen) {
		outdata.data = data;
		outdata.size = datalen;
		task->scsi.expxferlen = datalen;

		return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
	}
	
	task->scsi.expxferlen = 0;
	task->scsi.xfer_dir = SCSI_XFER_NONE;

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				       private_data);
}

int
iscsi_unmap_task(struct iscsi_task *task, int lun, int anchor, int group,
		 struct unmap_list *list, int list_len,
		 iscsi_command_cb cb, void *private_data)
{
	struct iscsi_data outdata;
	unsigned char *data;
	int xferlen;
	int i;

	xferlen = 8 + list_len * 16;

	scsi_cdb_unmap(&task->scsi, anchor, group, xferlen);

	data = scsi_malloc(&task->scsi, xferlen);
	if (data == NULL) {
		iscsi_set_error(task->iscsi, "Out-of-memory: Failed to create "
				"unmap parameters.");
		return -1;
	}
	*((uint16_t *)&data[0]) = htons(xferlen - 2);
	*((uint16_t *)&data[2]) = htons(xferlen - 8);
	for (i = 0; i < list_len; i++) {
		*((uint32_t *)&data[8 + 16 * i])     = htonl(list[0].lba >> 32);
		*((uint32_t *)&data[8 + 16 * i + 4]) = htonl(list[0].lba & 0xffffffff);
		*((uint32_t *)&data[8 + 16 * i + 8]) = htonl(list[0].num);
	}

	outdata.data = data;
	outdata.size = xferlen;

	return iscsi_scsi_command_async(task, lun, cb, &outdata,
				       private_data);
}

unsigned char *
iscsi_get_user_in_buffer(struct iscsi_context *iscsi, struct iscsi_in_pdu *in, uint32_t pos, ssize_t *count)
{
	struct iscsi_pdu *pdu;
	uint32_t offset;
	uint32_t itt;

	if ((in->hdr[0] & 0x3f) != ISCSI_PDU_DATA_IN) {
		return NULL;
	}

	offset = ntohl(*(uint32_t *)&in->hdr[40]);

	itt = ntohl(*(uint32_t *)&in->hdr[16]);
	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		if (pdu->itt == itt) {
			break;
		}
	}
	if (pdu == NULL) {
		return NULL;
	}

	return scsi_task_get_data_in_buffer(&pdu->iscsi_cbdata->task->scsi, offset + pos, count);
}

int
iscsi_readtoc_task(struct iscsi_task *task, int lun, int msf,
		   int format, int track_session, int maxsize,
		   iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_readtoc(&task->scsi, msf, format, track_session, maxsize);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				     private_data);
}

int
iscsi_reserve6_task(struct iscsi_task *task, int lun,
		    iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_reserve6(&task->scsi);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				     private_data);
}

int
iscsi_release6_task(struct iscsi_task *task, int lun,
		    iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_release6(&task->scsi);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				     private_data);
}

int
iscsi_report_supported_opcodes_task(struct iscsi_task *task, 
				    int lun, int return_timeouts, int maxsize,
				    iscsi_command_cb cb, void *private_data)
{
	scsi_cdb_report_supported_opcodes(&task->scsi, return_timeouts, maxsize);

	return iscsi_scsi_command_async(task, lun, cb, NULL,
				     private_data);
}

struct iscsi_task *
iscsi_get_task_from_pdu(struct iscsi_pdu *pdu)
{
	return pdu->iscsi_cbdata->task;
}

int
iscsi_cancel_task(struct iscsi_task *task)
{
	struct iscsi_pdu *pdu;

	for (pdu = task->iscsi->waitpdu; pdu; pdu = pdu->next) {
		if (pdu->itt == task->scsi.itt) {
			SLIST_REMOVE(&task->iscsi->waitpdu, pdu);
			if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
				pdu->callback(task->iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
			}
			iscsi_free_pdu(task->iscsi, pdu);
			return 0;
		}
	}
	for (pdu = task->iscsi->outqueue; pdu; pdu = pdu->next) {
		if (pdu->itt == task->scsi.itt) {
			SLIST_REMOVE(&task->iscsi->outqueue, pdu);
			if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
				pdu->callback(task->iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
			}
			iscsi_free_pdu(task->iscsi, pdu);
			return 0;
		}
	}
	return -1;
}

void
iscsi_cancel_all_tasks(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		SLIST_REMOVE(&iscsi->waitpdu, pdu);
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}
	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		SLIST_REMOVE(&iscsi->outqueue, pdu);
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}
}
