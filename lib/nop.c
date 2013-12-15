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
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include "iscsi.h"
#include "iscsi-private.h"

int
iscsi_nop_out_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		    unsigned char *data, int len, void *private_data)
{
	struct iscsi_pdu *pdu;

	if (iscsi->is_loggedin == 0) {
		iscsi_set_error(iscsi, "trying to send nop-out while not "
				"logged in");
		return -1;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_NOP_OUT, ISCSI_PDU_NOP_IN);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Failed to allocate nop-out pdu");
		return -1;
	}

	/* We don't want to requeue these on reconnect */
	pdu->flags |= ISCSI_PDU_DROP_ON_RECONNECT;

	/* immediate flag */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, 0x80);

	/* ttt */
	iscsi_pdu_set_ttt(pdu, 0xffffffff);

	/* lun */
	iscsi_pdu_set_lun(pdu, 0);

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);
	pdu->cmdsn = iscsi->cmdsn;

	/* exp statsn */
	iscsi_pdu_set_expstatsn(pdu, iscsi->statsn+1);

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (data != NULL && len > 0) {
		if (iscsi_pdu_add_data(iscsi, pdu, data, len) != 0) {
			iscsi_set_error(iscsi, "Failed to add outdata to nop-out");
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
	}

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "failed to queue iscsi nop-out pdu");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	iscsi->nops_in_flight++;

	return 0;
}

int
iscsi_send_target_nop_out(struct iscsi_context *iscsi, uint32_t ttt)
{
	struct iscsi_pdu *pdu;

	pdu = iscsi_allocate_pdu_with_itt_flags(iscsi, ISCSI_PDU_NOP_OUT, ISCSI_PDU_NO_PDU,
				0xffffffff,ISCSI_PDU_DROP_ON_RECONNECT|ISCSI_PDU_DELETE_WHEN_SENT|ISCSI_PDU_NO_CALLBACK);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Failed to allocate nop-out pdu");
		return -1;
	}

	/* immediate flag */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, 0x80);

	/* ttt */
	iscsi_pdu_set_ttt(pdu, ttt);

	/* lun */
	iscsi_pdu_set_lun(pdu, 0);

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);
	pdu->cmdsn = iscsi->cmdsn;

	/* exp statsn */
	iscsi_pdu_set_expstatsn(pdu, iscsi->statsn+1);

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "failed to queue iscsi nop-out pdu");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

int
iscsi_process_nop_out_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			    struct iscsi_in_pdu *in)
{
	struct iscsi_data data;

	iscsi->nops_in_flight = 0;

	if (pdu->callback == NULL) {
		return 0;
	}

	data.data = NULL;
	data.size = 0;

	if (in->data_pos > ISCSI_HEADER_SIZE) {
		data.data = in->data;
		data.size = in->data_pos;
	}
	
	pdu->callback(iscsi, SCSI_STATUS_GOOD, &data, pdu->private_data);

	return 0;
}

int iscsi_get_nops_in_flight(struct iscsi_context *iscsi)
{
	return iscsi->nops_in_flight;
}
