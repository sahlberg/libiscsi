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
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"
#include <stdlib.h>

int pdu_was_valid;

/* one block sent as immediate data. PDU should have F-bit set
 * and datasegmentlength should be a single block.
 */
static int my_queue_immediate_data(struct iscsi_context *iscsi _U_, struct iscsi_pdu *pdu)
{
	pdu_was_valid = 1;

	if (!(pdu->outdata.data[1] & 0x80)) {
		printf("SCSI-Command PDU with immediate data did not have the F-flag set.\n");
		pdu_was_valid = 0;
		return 0;
	}
	if ( (scsi_get_uint32(&pdu->outdata.data[4]) & 0x00ffffff) != block_size) {
		printf("SCSI-Command PDU did not have one block of immediate data.\n");
		pdu_was_valid = 0;
		return 0;
	}
	return 1;
}

int T1041_unsolicited_immediate_data(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;
	struct iscsi_url *iscsi_url;
	unsigned char data[4096];

	printf("1041_unsolicited_immediate_data:\n");
	printf("================================\n");
	if (show_info) {
		printf("Test we can send unsolicited data to the target\n");
		printf("1, Login to target with IMMEDIATE_DATA=YES and INITIAL_R2T=YES.\n");
		printf("2, Write one block to the target as immediate data.\n");
		printf("3, Verify that the PDU sent has the F-flag set.\n");
		printf("4, Verify that the PDU sent has <block-size> of immediate data.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	iscsi_url = iscsi_parse_full_url(iscsi, url);
	if (iscsi_url == NULL) {
		printf("Failed to parse iscsi url\n");
		return -1;
	}


	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}


	ret = 0;


	/* This setting will allow us to send unsolicited data as
	 * immediate data but not as a data-out.
	 */ 	
	printf("Login to target with IMMEDIATE_DATA=YES and INITIAL_R2T=YES ... ");
	iscsi_destroy_context(iscsi);
	iscsi_url->iscsi = NULL;
	iscsi = iscsi_create_context(initiator);
	iscsi_set_targetname(iscsi, iscsi_url->target);
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
	iscsi_set_immediate_data(iscsi, ISCSI_IMMEDIATE_DATA_YES);
	iscsi_set_initial_r2t(iscsi, ISCSI_INITIAL_R2T_YES);
	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, lun) != 0) {
		printf("[FAILED]\n");
		printf("Failed to log in to target with IMMEDIATE_DATA=YES and INITIAL_R2T=YES %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (iscsi->use_immediate_data != ISCSI_IMMEDIATE_DATA_YES) {
		printf("[FAILED]\n");
		printf("Failed to negotiate IMMEDIATE_DATA==YES with target\n");
		ret = -1;
		goto finished;
	}
	if (iscsi->use_initial_r2t != ISCSI_INITIAL_R2T_YES) {
		printf("[FAILED]\n");
		printf("Failed to negotiate INITIAL_R2T==YES with target\n");
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");



	printf("Write one block as immediate data ... ");
	local_iscsi_queue_pdu = my_queue_immediate_data;
	task = iscsi_write10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	local_iscsi_queue_pdu = NULL;
	/* Verify that the PDU we sent had the F-bit set and that 
	 * datasegmentlength was one block.
	 */
	if (pdu_was_valid == 0) {
	        printf("[FAILED]\n");
		printf("PDU to send was invalid.\n");
		ret = -1;
		goto finished;
	}
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send write10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Write10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");



finished:
	iscsi_destroy_url(iscsi_url);
	iscsi_destroy_context(iscsi);

	return ret;
}
