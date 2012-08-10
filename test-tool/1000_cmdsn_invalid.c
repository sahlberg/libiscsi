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

static int change_cmdsn;

static int my_iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	switch (change_cmdsn) {
	case 1:
		/* change the cmdsn so it becomes too big */
		*(uint32_t *)&pdu->outdata.data[24] = htonl(iscsi->maxcmdsn + 1);
		break;
	case 2:
		/* change the cmdsn so it becomes too small */
		*(uint32_t *)&pdu->outdata.data[24] = 0;
		break;
	}

	change_cmdsn = 0;	
}

static void test_cb(struct iscsi_context *iscsi _U_, int status,
			void *command_data _U_, void *private_data)
{
	struct scsi_task *task = command_data;
	struct iscsi_async_state *state = private_data;

	state->finished = 1;
	state->status = status;

	if (status) {
		task->status = status;
	}
}


int T1000_cmdsn_invalid(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, i, lun;
	uint32_t block_size;
	uint32_t num_blocks;
	unsigned char data[512 * 256];
	struct iscsi_async_state test_state;

	printf("1000_cmdsn_invalid:\n");
	printf("==================\n");
	if (show_info) {
		printf("Test sending commands with invalid cmdsn values.\n");
		printf("CMDSN MUST be in the range EXPCMDSN and MAXCMDSN\n");
		printf("1, Test that a CMDSN > MAXCMDSN is an error\n");
		printf("2, Test that a CMDSN == 0 is an error\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/* find the size of the LUN */
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		printf("failed to unmarshall READCAPACITY16 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	block_size = rc16->block_length;
	num_blocks = rc16->returned_lba;
	scsi_free_scsi_task(task);


	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -1;
		goto finished;
	}


	ret = 0;

	iscsi->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
	iscsi->target_max_recv_data_segment_length = 512;
	local_iscsi_queue_pdu = my_iscsi_queue_pdu;

	printf("Write 2 blocks with CMDSN > MAXCMDSN ... ");
	change_cmdsn = 1;
	/* we dont want autoreconnect since some targets will drop the
	 * on this condition.
	 */
	iscsi_set_noautoreconnect(iscsi, 1);

	task = iscsi_write10_task(iscsi, lun, 0, data, 2 * block_size, block_size,
				0, 0, 0, 0, 0,
				test_cb, &test_state);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test2;
	}
	test_state.task     = task;
	test_state.finished = 0;
	test_state.status   = 0;
	wait_until_test_finished(iscsi, &test_state);
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE10 command successful. Should have failed with error\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test2;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test2:
	/* in case the previous test failed the session */
	iscsi_set_noautoreconnect(iscsi, 0);
	iscsi->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
	iscsi->target_max_recv_data_segment_length = 512;

	printf("Write 2 blocks with CMDSN == 0 ... ");fflush(stdout);
	change_cmdsn = 2;
	/* we dont want autoreconnect since some targets will drop the
	 * on this condition.
	 */

	iscsi_set_noautoreconnect(iscsi, 1);
	task = iscsi_write10_task(iscsi, lun, 0, data, 2 * block_size, block_size,
				0, 0, 0, 0, 0,
				test_cb, &test_state);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test3;
	}
	test_state.task     = task;
	test_state.finished = 0;
	test_state.status   = 0;
	wait_until_test_finished(iscsi, &test_state);
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE10 command successful. Should have failed with error\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test3:
	/* in case the previous test failed the session */
	iscsi_set_noautoreconnect(iscsi, 0);

finished:
	local_iscsi_queue_pdu = NULL;
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
