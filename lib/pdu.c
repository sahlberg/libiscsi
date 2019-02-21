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

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
#define PRIu64 "llu"
#define PRIx32 "x"
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include "win32/win32_compat.h"
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "slist.h"

/* This adds 32-bit serial comparision as defined in RFC1982.
 * It returns 0 for equality, 1 if s1 is greater than s2 and
 * -1 if s1 is less than s2. According to RFC1982 section 3.2
 * there are rare cases where the result of the comparision is
 * undefined e.g. when s1 = 0 and s2=2^31. This cases should
 * not happen in iSCSI protocol.
 */
int
iscsi_serial32_compare(uint32_t s1, uint32_t s2) {
	if (s1 == s2) return 0;
	if (s1 < s2 && s2-s1 < (uint32_t)1<<31) return -1;
	if (s1 > s2 && s1-s2 < (uint32_t)1<<31) return 1;
	if (s1 > s2 && s1-s2 > (uint32_t)1<<31) return -1;
	if (s1 < s2 && s2-s1 > (uint32_t)1<<31) return 1;
	/* undefined result */
	return -1;
}

uint32_t
iscsi_itt_post_increment(struct iscsi_context *iscsi) {
	uint32_t old_itt = iscsi->itt;
	iscsi->itt++;
	/* 0xffffffff is a reserved value */
	if (iscsi->itt == 0xffffffff) {
		iscsi->itt = 0;
	}
	return old_itt;
}

void iscsi_dump_pdu_header(struct iscsi_context *iscsi, unsigned char *data) {
	char dump[ISCSI_RAW_HEADER_SIZE*3+1]={0};
	int i;
	for (i=0;i<ISCSI_RAW_HEADER_SIZE;i++) {
		snprintf(&dump[i * 3], 4, " %02x", data[i]);
	}
	ISCSI_LOG(iscsi, 2, "PDU header:%s", dump);
}

struct iscsi_pdu*
iscsi_tcp_new_pdu(struct iscsi_context *iscsi, size_t size)
{
	struct iscsi_pdu *pdu;

	pdu = iscsi_szmalloc(iscsi, size);

	return pdu;
}

struct iscsi_pdu *
iscsi_allocate_pdu(struct iscsi_context *iscsi, enum iscsi_opcode opcode,
		   enum iscsi_opcode response_opcode, uint32_t itt,
		   uint32_t flags)
{
	struct iscsi_pdu *pdu;

	pdu = iscsi->drv->new_pdu(iscsi, sizeof(struct iscsi_pdu));
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "failed to allocate pdu");
		return NULL;
	}

	pdu->outdata.size = ISCSI_HEADER_SIZE(iscsi->header_digest);
	pdu->outdata.data = iscsi_szmalloc(iscsi, pdu->outdata.size);

	if (pdu->outdata.data == NULL) {
		iscsi_set_error(iscsi, "failed to allocate pdu header");
		iscsi_free(iscsi, pdu);
		return NULL;
	}

	/* opcode */
	pdu->outdata.data[0] = opcode;
	pdu->response_opcode = response_opcode;

	/* isid */
	if (opcode == ISCSI_PDU_LOGIN_REQUEST) {
		memcpy(&pdu->outdata.data[8], &iscsi->isid[0], 6);
	}

	/* itt */
	iscsi_pdu_set_itt(pdu, itt);
	pdu->itt = itt;

	/* flags */
	pdu->flags = flags;

	return pdu;
}

void
iscsi_tcp_free_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "trying to free NULL pdu");
		return;
	}

	if (pdu->outdata.size <= iscsi->smalloc_size) {
		iscsi_sfree(iscsi, pdu->outdata.data);
	} else {
		iscsi_free(iscsi, pdu->outdata.data);
	}
	pdu->outdata.data = NULL;

	if (pdu->indata.size <= iscsi->smalloc_size) {
		iscsi_sfree(iscsi, pdu->indata.data);
	} else {
		iscsi_free(iscsi, pdu->indata.data);
	}
	pdu->indata.data = NULL;

	if (iscsi->outqueue_current == pdu) {
		iscsi->outqueue_current = NULL;
	}

	iscsi_sfree(iscsi, pdu);
}

int
iscsi_add_data(struct iscsi_context *iscsi, struct iscsi_data *data,
	       unsigned char *dptr, int dsize, int pdualignment)
{
	size_t len, aligned;

	if (dsize == 0) {
		iscsi_set_error(iscsi, "Trying to append zero size data to "
				"iscsi_data");
		return -1;
	}

	len = data->size + dsize;

	aligned = len;
	if (pdualignment) {
		aligned = (aligned+3)&0xfffffffc;
	}

	if (data->size == 0) {
		if (aligned <= iscsi->smalloc_size) {
			data->data = iscsi_szmalloc(iscsi, aligned);
		} else {
			data->data = iscsi_malloc(iscsi, aligned);
		}
	} else {
		if (aligned > iscsi->smalloc_size) {
			data->data = iscsi_realloc(iscsi, data->data, aligned);
		}
	}
	if (data->data == NULL) {
		iscsi_set_error(iscsi, "failed to allocate buffer for %d "
				"bytes", (int) len);
		return -1;
	}

	memcpy(data->data + data->size, dptr, dsize);
	data->size += dsize;

	if (len != aligned) {
		/* zero out any padding at the end */
		memset(data->data + len, 0, aligned - len);
	}

	return 0;
}

int
iscsi_pdu_add_data(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
		   unsigned char *dptr, int dsize)
{
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "trying to add data to NULL pdu");
		return -1;
	}
	if (dsize == 0) {
		iscsi_set_error(iscsi, "Trying to append zero size data to "
				"pdu");
		return -1;
	}

	if (iscsi_add_data(iscsi, &pdu->outdata, dptr, dsize, 1) != 0) {
		iscsi_set_error(iscsi, "failed to add data to pdu buffer");
		return -1;
	}

	/* update data segment length */
	scsi_set_uint32(&pdu->outdata.data[4], pdu->outdata.size
				- ISCSI_HEADER_SIZE(iscsi->header_digest));

	return 0;
}

int
iscsi_get_pdu_data_size(const unsigned char *hdr)
{
	int size;

	size = scsi_get_uint32(&hdr[4]) & 0x00ffffff;

	return size;
}

int
iscsi_get_pdu_padding_size(const unsigned char *hdr)
{
	int data_size, padded_size;

	data_size = scsi_get_uint32(&hdr[4]) & 0x00ffffff;
	padded_size = (data_size+3) & 0xfffffffc;

	return padded_size - data_size;
}

enum iscsi_reject_reason {
	ISCSI_REJECT_RESERVED                 = 0x01,
	ISCSI_REJECT_DATA_DIGEST_ERROR        = 0x02,
	ISCSI_REJECT_SNACK_REJECT             = 0x03,
	ISCSI_REJECT_PROTOCOL_ERROR           = 0x04,
	ISCSI_REJECT_COMMAND_NOT_SUPPORTED    = 0x05,
	ISCSI_REJECT_IMMEDIATE_COMMAND_REJECT = 0x06,
	ISCSI_REJECT_TASK_IN_PROCESS          = 0x07,
	ISCSI_REJECT_INVALID_DATA_ACK         = 0x08,
	ISCSI_REJECT_INVALID_PDU_FIELD        = 0x09,
	ISCSI_REJECT_LONG_OPERATION_REJECT    = 0x0a,
	ISCSI_REJECT_NEGOTIATION_RESET        = 0x0b,
	ISCSI_REJECT_WAITING_FOR_LOGOUT       = 0x0c
};

static const char *iscsi_reject_reason_str(enum iscsi_reject_reason reason)
{
	switch (reason) {
	case ISCSI_REJECT_RESERVED:
	     return "Reserved";
	case ISCSI_REJECT_DATA_DIGEST_ERROR:
	     return "Data Digest Error";
	case ISCSI_REJECT_SNACK_REJECT:
	     return "SNACK Reject";
	case ISCSI_REJECT_PROTOCOL_ERROR:
	     return "Protocol Error";
	case ISCSI_REJECT_COMMAND_NOT_SUPPORTED:
	     return "Command Not Supported";
	case ISCSI_REJECT_IMMEDIATE_COMMAND_REJECT:
	     return "Immediate Command Reject";
	case ISCSI_REJECT_TASK_IN_PROCESS:
	     return "Task In Process";
	case ISCSI_REJECT_INVALID_DATA_ACK:
	     return "Invalid Data ACK";
	case ISCSI_REJECT_INVALID_PDU_FIELD:
	     return "Invalid PDU Field";
	case ISCSI_REJECT_LONG_OPERATION_REJECT:
	     return "Long Operation Reject";
	case ISCSI_REJECT_NEGOTIATION_RESET:
	     return "Negotiation Reset";
	case ISCSI_REJECT_WAITING_FOR_LOGOUT:
	     return "Waiting For Logout";
	}

	return "Unknown";
}
	
int iscsi_process_target_nop_in(struct iscsi_context *iscsi,
				struct iscsi_in_pdu *in)
{
	uint32_t ttt = scsi_get_uint32(&in->hdr[20]);
	uint32_t itt = scsi_get_uint32(&in->hdr[16]);
	uint32_t lun = scsi_get_uint16(&in->hdr[8]);

	ISCSI_LOG(iscsi, (iscsi->nops_in_flight > 1) ? 1 : 6,
	          "NOP-In received (pdu->itt %08x, pdu->ttt %08x, pdu->lun %8x, iscsi->maxcmdsn %08x, iscsi->expcmdsn %08x, iscsi->statsn %08x)",
	          itt, ttt, lun, iscsi->maxcmdsn, iscsi->expcmdsn, iscsi->statsn);

	/* if the server does not want a response */
	if (ttt == 0xffffffff) {
		return 0;
	}

	iscsi_send_target_nop_out(iscsi, ttt, lun);

	return 0;
}

static void iscsi_reconnect_after_logout(struct iscsi_context *iscsi, int status,
                        void *command_data _U_, void *opaque _U_)
{
	if (status) {
		ISCSI_LOG(iscsi, 1, "logout failed: %s", iscsi_get_error(iscsi));
	}
	iscsi->pending_reconnect = 1;
}

int iscsi_process_reject(struct iscsi_context *iscsi,
				struct iscsi_in_pdu *in)
{
	int size = in->data_pos;
	uint32_t itt;
	struct iscsi_pdu *pdu;
	uint8_t reason = in->hdr[2];

	if (size < ISCSI_RAW_HEADER_SIZE) {
		iscsi_set_error(iscsi, "size of REJECT payload is too small."
				       "Need >= %d bytes but got %d.",
				       ISCSI_RAW_HEADER_SIZE, (int)size);
		return -1;
	}

	if (reason == ISCSI_REJECT_WAITING_FOR_LOGOUT) {
		ISCSI_LOG(iscsi, 1, "target rejects request with reason: %s",  iscsi_reject_reason_str(reason));
		iscsi_logout_async(iscsi, iscsi_reconnect_after_logout, NULL);
		return 0;
	}

	iscsi_set_error(iscsi, "Request was rejected with reason: 0x%02x (%s)", reason, iscsi_reject_reason_str(reason));

	itt = scsi_get_uint32(&in->data[16]);

	iscsi_dump_pdu_header(iscsi, in->data);

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		if (pdu->itt == itt) {
			break;
		}
	}

	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Can not match REJECT with"
				       "any outstanding pdu with itt:0x%08x",
				       itt);
		return -1;
	}

	if (pdu->callback) {
		pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
		              pdu->private_data);
	}

	ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
	iscsi->drv->free_pdu(iscsi, pdu);
	return 0;
}

static void iscsi_process_pdu_serials(struct iscsi_context *iscsi, struct iscsi_in_pdu *in)
{
	uint32_t itt = scsi_get_uint32(&in->hdr[16]);
	uint32_t statsn = scsi_get_uint32(&in->hdr[24]);
	uint32_t maxcmdsn = scsi_get_uint32(&in->hdr[32]);
	uint32_t expcmdsn = scsi_get_uint32(&in->hdr[28]);
	uint16_t status = scsi_get_uint16(&in->hdr[36]);
	uint8_t flags = in->hdr[1];
	enum iscsi_opcode opcode = in->hdr[0] & 0x3f;

	/* RFC3720 10.13.5 (serials are invalid if status class != 0) */
	if (opcode == ISCSI_PDU_LOGIN_RESPONSE && (status >> 8)) {
		return;
	}

	if (iscsi_serial32_compare(maxcmdsn, iscsi->maxcmdsn) > 0) {
		iscsi->maxcmdsn = maxcmdsn;
	}
	if (iscsi_serial32_compare(expcmdsn, iscsi->expcmdsn) > 0) {
		iscsi->expcmdsn = expcmdsn;
	}

	/* RFC3720 10.7.3 (StatSN is invalid if S bit unset in flags) */
	if (opcode == ISCSI_PDU_DATA_IN &&
	    !(flags & ISCSI_PDU_DATA_CONTAINS_STATUS)) {
		return;
	}

	if (itt == 0xffffffff) {
		/* target will not increase statsn if itt == 0xffffffff */
		statsn--;
	}
	if (iscsi_serial32_compare(statsn, iscsi->statsn) > 0) {
		iscsi->statsn = statsn;
	}
}

int
iscsi_process_pdu(struct iscsi_context *iscsi, struct iscsi_in_pdu *in)
{
	uint32_t itt = scsi_get_uint32(&in->hdr[16]);
	enum iscsi_opcode opcode = in->hdr[0] & 0x3f;
	uint8_t ahslen = in->hdr[4];
	struct iscsi_pdu *pdu;

	/* verify header checksum */
	if (iscsi->header_digest != ISCSI_HEADER_DIGEST_NONE) {
		uint32_t crc, crc_rcvd = 0;
		crc = crc32c(in->hdr, ISCSI_RAW_HEADER_SIZE);
		crc_rcvd |= in->hdr[ISCSI_RAW_HEADER_SIZE+0];
		crc_rcvd |= in->hdr[ISCSI_RAW_HEADER_SIZE+1] << 8;
		crc_rcvd |= in->hdr[ISCSI_RAW_HEADER_SIZE+2] << 16;
		crc_rcvd |= in->hdr[ISCSI_RAW_HEADER_SIZE+3] << 24;
		if (crc != crc_rcvd) {
			iscsi_set_error(iscsi, "header checksum verification failed: calculated 0x%" PRIx32 " received 0x%" PRIx32, crc, crc_rcvd);
			return -1;
		}
	}

	if (ahslen != 0) {
		iscsi_set_error(iscsi, "cant handle expanded headers yet");
		return -1;
	}

	/* All target PDUs update the serials */
	iscsi_process_pdu_serials(iscsi, in);

	if (opcode == ISCSI_PDU_ASYNC_MSG) {
		uint8_t event = in->hdr[36];
		uint16_t param1 = scsi_get_uint16(&in->hdr[38]); 
		uint16_t param2 = scsi_get_uint16(&in->hdr[40]); 
		uint16_t param3 = scsi_get_uint16(&in->hdr[42]); 
		switch (event) {
		case 0x0:
			/* Just ignore these ones for now. It could be
			 * a UNIT_ATTENTION for some changes on the
			 * target but we don't have an API to pass this on
			 * to the application yet.
			 */
			ISCSI_LOG(iscsi, 2, "Ignoring received iSCSI AsyncMsg/"
				  "SCSI Async Event");
			return 0;
		case 0x1:
			ISCSI_LOG(iscsi, 2, "target requests logout within %u seconds", param3);
			/* this is an ugly workaround for DELL Equallogic FW 7.x bugs:
			 *  Bug_71409 - I/O errors during volume move (present before 7.0.7)
			 *  Bug_73732 - I/O errors during volume move operation (still present in 7.0.9)
			 */
			if (getenv("LIBISCSI_DROP_CONN_ON_ASYNC_EVENT1") != NULL) {
				ISCSI_LOG(iscsi, 2, "dropping connection to fix errors with broken DELL Equallogic firmware 7.x");
				return -1;
			}
			iscsi_logout_async(iscsi, iscsi_reconnect_after_logout, NULL);
			return 0;
		case 0x2:
			ISCSI_LOG(iscsi, 2, "target will drop this connection. Time2Wait is %u seconds", param2);
			iscsi->next_reconnect = time(NULL) + param2;
			return 0;
		case 0x3:
			ISCSI_LOG(iscsi, 2, "target will drop all connections of this session. Time2Wait is %u seconds", param2);
			iscsi->next_reconnect = time(NULL) + param2;
			return 0;
		case 0x4:
			ISCSI_LOG(iscsi, 2, "target requests parameter renogitiation.");
			iscsi_logout_async(iscsi, iscsi_reconnect_after_logout, NULL);
			return 0;
		default:
			ISCSI_LOG(iscsi, 1, "unhandled async event %u: param1 %u param2 %u param3 %u", event, param1, param2, param3);
			return -1;
		}
	}

	if (opcode == ISCSI_PDU_REJECT) {
		return iscsi_process_reject(iscsi, in);
	}

	if (opcode == ISCSI_PDU_NOP_IN && itt == 0xffffffff) {
		if (iscsi_process_target_nop_in(iscsi, in) != 0) {
			return -1;
		}
		return 0;
	}

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		enum iscsi_opcode expected_response = pdu->response_opcode;
		int is_finished = 1;

		if (pdu->itt != itt) {
			continue;
		}

		/* we have a special case with scsi-command opcodes,
		 * they are replied to by either a scsi-response
		 * or a data-in, or a combination of both.
		 */
		if (opcode == ISCSI_PDU_DATA_IN
		    && expected_response == ISCSI_PDU_SCSI_RESPONSE) {
			expected_response = ISCSI_PDU_DATA_IN;
		}

		/* Another special case is if we get a R2T.
		 * In this case we should find the original request and just send an additional
		 * DATAOUT segment for this task.
		 */
		if (opcode == ISCSI_PDU_R2T) {
			expected_response = ISCSI_PDU_R2T;
		}

		if (opcode != expected_response) {
			iscsi_set_error(iscsi, "Got wrong opcode back for "
					"itt:%d  got:%d expected %d",
					itt, opcode, pdu->response_opcode);
			return -1;
		}
		switch (opcode) {
		case ISCSI_PDU_LOGIN_RESPONSE:
			if (iscsi_process_login_reply(iscsi, pdu, in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi login reply "
						"failed");
				return -1;
			}
			break;
		case ISCSI_PDU_TEXT_RESPONSE:
			if (iscsi_process_text_reply(iscsi, pdu, in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi text reply "
						"failed");
				return -1;
			}
			break;
		case ISCSI_PDU_LOGOUT_RESPONSE:
			if (iscsi_process_logout_reply(iscsi, pdu, in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi logout reply "
						"failed");
				return -1;
			}
			break;
		case ISCSI_PDU_SCSI_RESPONSE:
			if (iscsi_process_scsi_reply(iscsi, pdu, in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi response reply "
						"failed");
				return -1;
			}
			break;
		case ISCSI_PDU_DATA_IN:
			if (iscsi_process_scsi_data_in(iscsi, pdu, in,
						       &is_finished) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi data in "
						"failed");
				return -1;
			}
			break;
		case ISCSI_PDU_NOP_IN:
			if (iscsi_process_nop_out_reply(iscsi, pdu, in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi nop-in failed");
				return -1;
			}
			break;
		case ISCSI_PDU_SCSI_TASK_MANAGEMENT_RESPONSE:
			if (iscsi_process_task_mgmt_reply(iscsi, pdu,
							  in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi task-mgmt failed");
				return -1;
			}
			break;
		case ISCSI_PDU_R2T:
			if (iscsi_process_r2t(iscsi, pdu, in) != 0) {
				ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
				iscsi->drv->free_pdu(iscsi, pdu);
				iscsi_set_error(iscsi, "iscsi r2t "
						"failed");
				return -1;
			}
			is_finished = 0;
			break;
		default:
			iscsi_set_error(iscsi, "Don't know how to handle "
					"opcode 0x%02x", opcode);
			return -1;
		}

		if (is_finished) {
			ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
			iscsi->drv->free_pdu(iscsi, pdu);
		}
		return 0;
	}

	return 0;
}

void
iscsi_pdu_set_itt(struct iscsi_pdu *pdu, uint32_t itt)
{
	scsi_set_uint32(&pdu->outdata.data[16], itt);
}

void
iscsi_pdu_set_ritt(struct iscsi_pdu *pdu, uint32_t ritt)
{
	scsi_set_uint32(&pdu->outdata.data[20], ritt);
}

void
iscsi_pdu_set_pduflags(struct iscsi_pdu *pdu, unsigned char flags)
{
	pdu->outdata.data[1] = flags;
}

void
iscsi_pdu_set_immediate(struct iscsi_pdu *pdu)
{
	pdu->outdata.data[0] |= ISCSI_PDU_IMMEDIATE;
}

void
iscsi_pdu_set_ttt(struct iscsi_pdu *pdu, uint32_t ttt)
{
	scsi_set_uint32(&pdu->outdata.data[20], ttt);
}

void
iscsi_pdu_set_cmdsn(struct iscsi_pdu *pdu, uint32_t cmdsn)
{
	scsi_set_uint32(&pdu->outdata.data[24], cmdsn);
	pdu->cmdsn = cmdsn;
}

void
iscsi_pdu_set_rcmdsn(struct iscsi_pdu *pdu, uint32_t rcmdsn)
{
	scsi_set_uint32(&pdu->outdata.data[32], rcmdsn);
}

void
iscsi_pdu_set_datasn(struct iscsi_pdu *pdu, uint32_t datasn)
{
	scsi_set_uint32(&pdu->outdata.data[36], datasn);
}

void
iscsi_pdu_set_expstatsn(struct iscsi_pdu *pdu, uint32_t expstatsnsn)
{
	scsi_set_uint32(&pdu->outdata.data[28], expstatsnsn);
}

void
iscsi_pdu_set_bufferoffset(struct iscsi_pdu *pdu, uint32_t bufferoffset)
{
	scsi_set_uint32(&pdu->outdata.data[40], bufferoffset);
}

void
iscsi_pdu_set_cdb(struct iscsi_pdu *pdu, struct scsi_task *task)
{
	memset(&pdu->outdata.data[32], 0, 16);
	memcpy(&pdu->outdata.data[32], task->cdb, task->cdb_size);
}

void
iscsi_pdu_set_lun(struct iscsi_pdu *pdu, uint32_t lun)
{
	scsi_set_uint16(&pdu->outdata.data[8], lun);
}

void
iscsi_pdu_set_expxferlen(struct iscsi_pdu *pdu, uint32_t expxferlen)
{
	pdu->expxferlen = expxferlen;
	scsi_set_uint32(&pdu->outdata.data[20], expxferlen);
}

void
iscsi_timeout_scan(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;
	struct iscsi_pdu *next_pdu;
	time_t t = time(NULL);

	for (pdu = iscsi->outqueue; pdu; pdu = next_pdu) {
		next_pdu = pdu->next;

		if (pdu->scsi_timeout == 0) {
			/* no timeout for this pdu */
			continue;
		}
		if (t < pdu->scsi_timeout) {
			/* not expired yet */
			continue;
		}
		ISCSI_LIST_REMOVE(&iscsi->outqueue, pdu);
		iscsi_set_error(iscsi, "command timed out");
		iscsi_dump_pdu_header(iscsi, pdu->outdata.data);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_TIMEOUT,
			              NULL, pdu->private_data);
		}
		iscsi->drv->free_pdu(iscsi, pdu);
	}
	for (pdu = iscsi->waitpdu; pdu; pdu = next_pdu) {
		next_pdu = pdu->next;

		if (pdu->scsi_timeout == 0) {
			/* no timeout for this pdu */
			continue;
		}
		if (t < pdu->scsi_timeout) {
			/* not expired yet */
			continue;
		}
		ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
		iscsi_set_error(iscsi, "command timed out");
		iscsi_dump_pdu_header(iscsi, pdu->outdata.data);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_TIMEOUT,
			              NULL, pdu->private_data);
		}
		iscsi->drv->free_pdu(iscsi, pdu);
	}
}

int
iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	return iscsi->drv->queue_pdu(iscsi, pdu);
}

void
iscsi_cancel_pdus(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;

	while ((pdu = iscsi->outqueue)) {
		ISCSI_LIST_REMOVE(&iscsi->outqueue, pdu);
		if (iscsi->is_loggedin && pdu->callback) {
			/* If an error happened during connect/login,
			   we don't want to call any of the callbacks.
			*/
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED,
			              NULL, pdu->private_data);
		}
		iscsi->drv->free_pdu(iscsi, pdu);
	}
	while ((pdu = iscsi->waitpdu)) {
		ISCSI_LIST_REMOVE(&iscsi->waitpdu, pdu);
		if (iscsi->is_loggedin && pdu->callback) {
			/* If an error happened during connect/login,
			   we don't want to call any of the callbacks.
			*/
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED,
			              NULL, pdu->private_data);
		}
		iscsi->drv->free_pdu(iscsi, pdu);
	}
}
