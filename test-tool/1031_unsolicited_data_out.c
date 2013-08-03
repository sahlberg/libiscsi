/*
   Copyright (C) 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

static int
my_iscsi_add_data(struct iscsi_context *iscsi _U_, struct iscsi_data *data,
	       unsigned char *dptr, int dsize, int pdualignment)
{
	int len, aligned;
	unsigned char *buf;

	if (dsize == 0) {
		printf("Trying to append zero size data to iscsi_data");
		return -1;
	}

	len = data->size + dsize;
	aligned = len;
	if (pdualignment) {
		aligned = (aligned+3)&0xfffffffc;
	}
	buf = malloc(aligned);
	if (buf == NULL) {
		printf("failed to allocate buffer for %d bytes", len);
		return -1;
	}

	if (data->size > 0) {
		memcpy(buf, data->data, data->size);
	}
	memcpy(buf + data->size, dptr, dsize);
	if (len != aligned) {
		/* zero out any padding at the end */
	  memset(buf+len, 0, aligned-len);
	}

	free(data->data);

	data->data  = buf;
	data->size = len;

	return 0;
}

static int
my_iscsi_pdu_add_data(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
		   unsigned char *dptr, int dsize)
{
	if (my_iscsi_add_data(iscsi, &pdu->outdata, dptr, dsize, 1) != 0) {
		printf("failed to add data to pdu buffer");
		return -1;
	}

	/* update data segment length */
	scsi_set_uint32(&pdu->outdata.data[4], pdu->outdata.size - ISCSI_HEADER_SIZE);

	return 0;
}

static void
my_iscsi_pdu_set_itt(struct iscsi_pdu *pdu, uint32_t itt)
{
	scsi_set_uint32(&pdu->outdata.data[16], itt);
}

static void
my_iscsi_pdu_set_expstatsn(struct iscsi_pdu *pdu, uint32_t expstatsnsn)
{
	scsi_set_uint32(&pdu->outdata.data[28], expstatsnsn);
}

static void
my_iscsi_pdu_set_pduflags(struct iscsi_pdu *pdu, unsigned char flags)
{
	pdu->outdata.data[1] = flags;
}

static void
my_iscsi_pdu_set_lun(struct iscsi_pdu *pdu, uint32_t lun)
{
	pdu->outdata.data[8] = lun >> 8;
	pdu->outdata.data[9] = lun & 0xff;
}

static struct iscsi_pdu *
my_iscsi_allocate_pdu_with_itt_flags(struct iscsi_context *iscsi, enum iscsi_opcode opcode,
				  enum iscsi_opcode response_opcode, uint32_t itt, uint32_t flags)
{
	struct iscsi_pdu *pdu;

	pdu = malloc(sizeof(struct iscsi_pdu));
	if (pdu == NULL) {
		printf("failed to allocate pdu");
		return NULL;
	}
	memset(pdu, 0, sizeof(struct iscsi_pdu));

	pdu->outdata.size = ISCSI_HEADER_SIZE;
	pdu->outdata.data = malloc(pdu->outdata.size);

	if (pdu->outdata.data == NULL) {
		printf("failed to allocate pdu header");
		free(pdu);
		return NULL;
	}
	memset(pdu->outdata.data, 0, pdu->outdata.size);

	/* opcode */
	pdu->outdata.data[0] = opcode;
	pdu->response_opcode = response_opcode;

	/* isid */
	if (opcode == ISCSI_PDU_LOGIN_REQUEST) {
		memcpy(&pdu->outdata.data[8], &iscsi->isid[0], 6);
	}

	/* itt */
	my_iscsi_pdu_set_itt(pdu, itt);
	pdu->itt = itt;

	/* flags */
	pdu->flags = flags;

	return pdu;
}

int T1031_unsolicited_data_out(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi = NULL;
	struct iscsi_context *iscsi2 = NULL;
	int i, ret, lun;
	unsigned char buf[1024];

	printf("1031_unsolicited_data_out:\n");
	printf("==========================\n");
	if (show_info) {
		printf("Test sending unsolicited DATA-OUT that are not associated with any SCSI-command.\n");
		printf("1, Send 100 DATA-OUT PDUs\n");
		printf("2, Verify the target is still alive\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;

	printf("Send unsolicited DATA-OUT PDUs ... ");
	for (i = 0; i < 100; i++) {
		struct iscsi_pdu *pdu;

		pdu = my_iscsi_allocate_pdu_with_itt_flags(iscsi, ISCSI_PDU_DATA_OUT,
				 ISCSI_PDU_NO_PDU,
				 i + 0x1000,
				 ISCSI_PDU_DELETE_WHEN_SENT|ISCSI_PDU_NO_CALLBACK);
		if (pdu == NULL) {
			printf("Failed to allocated PDU. Aborting\n");
			ret = -2;
			goto finished;
		}
		my_iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_SCSI_FINAL);
		my_iscsi_pdu_set_lun(pdu, lun);
		my_iscsi_pdu_set_expstatsn(pdu, iscsi->statsn+1);
		if (my_iscsi_pdu_add_data(iscsi, pdu, buf, sizeof(buf)) != 0) {
			printf("Failed to add data to PDU. Aborting\n");
			ret = -2;
			goto finished;
		}
		pdu->callback     = NULL;
		pdu->private_data = NULL;
		if (iscsi_queue_pdu(iscsi, pdu) != 0) {
			printf("Failed to queue PDU. Aborting\n");
			ret = -2;
			goto finished;
		}
	}
	printf("[OK]\n");

	/* Send a TUR to drive the eventsystem and make sure the
	 * DATA-OUT PDUs are flushed
	 */
	printf("Send a TESTUNITREADY and flush tx queue.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	printf("Verify the target is still alive ... ");
	iscsi2 = iscsi_context_login(initiator, url, &lun);
	if (iscsi2 == NULL) {
	        printf("[FAILED]\n");
		printf("Target is dead?\n");
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");


finished:
	iscsi_destroy_context(iscsi);
	iscsi_destroy_context(iscsi2);
	return ret;
}
