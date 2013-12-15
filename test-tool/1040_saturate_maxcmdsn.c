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

static int num_cmds_in_flight;

static void test_cb(struct iscsi_context *iscsi _U_, int status,
			void *command_data _U_, void *private_data)
{
	struct iscsi_async_state *state = private_data;

	if (status != SCSI_STATUS_GOOD) {
		state->status = status;
	}

	if (--num_cmds_in_flight == 0) {
		state->finished = 1;
	}
}

#define T1040_NO_OF_WRITES (1024)

int T1040_saturate_maxcmdsn(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int i, ret, lun;
	unsigned char *data = NULL;
	struct iscsi_async_state test_state;

	printf("1040_saturate_maxcmdsn:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test sending so many commands we saturate maxcmdsn we do recover eventually\n");
		printf("1, Send 1024 commands in one go and make sure we eventually finish the queue of commands in flight\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (T1040_NO_OF_WRITES*2*iscsi->first_burst_length > block_size * num_blocks) {
		printf("target is too small for this test. at least %u bytes are required\n",T1040_NO_OF_WRITES*2*iscsi->first_burst_length);
		ret = -1;
		goto finished;
	}


	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}


	ret = 0;

	/* we don't want autoreconnect since some targets will drop the
	 * on this condition.
	 */
	iscsi_set_noautoreconnect(iscsi, 1);

	data = malloc(2*iscsi->first_burst_length);
	if (data == NULL) {
		printf("failed to malloc data buffer\n");
		ret = -1;
		goto finished;
	}

	int run=0;

	do {
		if (run || iscsi->use_immediate_data == ISCSI_IMMEDIATE_DATA_NO) {
			iscsi->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
			printf("Send %d Writes w/ ISCSI_IMMEDIATE_DATA_NO each needing a R2T so that we saturate the maxcmdsn queue ... ",T1040_NO_OF_WRITES);
		} else {
			printf("Send %d Writes w/ ISCSI_IMMEDIATE_DATA_YES each needing a R2T so that we saturate the maxcmdsn queue ... ",T1040_NO_OF_WRITES);
		}

		for (i = 0; i < T1040_NO_OF_WRITES; i++) {
			num_cmds_in_flight++;
			task = iscsi_write10_task(iscsi, lun, 2 * iscsi->first_burst_length * i / block_size, data, 2 * iscsi->first_burst_length, block_size,
					0, 0, 0, 0, 0,
					test_cb, &test_state);
			if (task == NULL) {
					printf("[FAILED]\n");
				printf("Failed to send WRITE10 command: %s\n", iscsi_get_error(iscsi));
				ret = -1;
				goto finished;
			}
		}
	
		test_state.task     = task;
		test_state.finished = 0;
		test_state.status   = 0;
		wait_until_test_finished(iscsi, &test_state);
		if (num_cmds_in_flight != 0) {
	        printf("[FAILED]\n");
			printf("Did not complete all I/O before deadline.\n");
			ret = -1;
			goto finished;
		} else if (test_state.status != 0) {
	        printf("[FAILED]\n");
			printf("Not all I/O commands succeeded.\n");
			ret = -1;
			goto finished;
		}
		printf("[OK]\n");
		run++;
	} while (iscsi->use_immediate_data == ISCSI_IMMEDIATE_DATA_YES);
	

finished:
	free(data);
	iscsi_destroy_context(iscsi);
	return ret;
}
