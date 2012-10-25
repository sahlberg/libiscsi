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
#include <stdlib.h>
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

uint32_t block_size;

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

int T1030_unsolicited_data_overflow(const char *initiator, const char *url, int data_loss, int show_info)
{ 
  struct iscsi_context *iscsi, *iscsi2;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, lun;
	unsigned char *buf = NULL;
	struct iscsi_async_state test_state;
	uint32_t old_first_burst_len;

	printf("1030_unsolicited_data_overflow:\n");
	printf("===============================\n");
	if (show_info) {
		printf("Test sending command with way more unsolicited data than the target supports\n");
		printf("1, Send HUGE unsolicited data to the target.\n");
		printf("2, Verify the target is still alive\n");
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
	scsi_free_scsi_task(task);


	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}


	ret = 0;

	iscsi->use_immediate_data = ISCSI_IMMEDIATE_DATA_YES;
	old_first_burst_len = iscsi->first_burst_length;
	/* make first burst REAL big */
	iscsi->first_burst_length *= 16;
	buf = malloc(iscsi->first_burst_length);

	printf("Write too much unsolicited data ... ");
	/* we dont want autoreconnect since some targets will drop the session
	 * on this condition.
	 */
	iscsi_set_noautoreconnect(iscsi, 1);

	// 102400 -- 1024000
	task = iscsi_write16_task(iscsi, lun, 0, buf,
				iscsi->first_burst_length, block_size,
				0, 0, 0, 0, 0,
				test_cb, &test_state);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test2;
	}

	test_state.task     = task;
	test_state.finished = 0;
	test_state.status   = 0;
	wait_until_test_finished(iscsi, &test_state);
	printf("[OK]\n");


test2:
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
	if (buf) {
		free(buf);
	}
	iscsi_destroy_context(iscsi);
	iscsi_destroy_context(iscsi2);
	return ret;
}
