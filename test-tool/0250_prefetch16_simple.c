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
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0250_prefetch16_simple(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, i, lun;
	uint64_t num_blocks;

	printf("0250_prefetch16_simple:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test basic PREFETCH16 functionality.\n");
		printf("1, Verify we can prefetch the first 1-256 blocks of the LUN.\n");
		printf("2, Verify we can prefetch the last 1-256 blocks of the LUN.\n");
		printf("3, Verify we can prefetch the last 256 blocks of the LUN by setting LEN==0.\n");
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
	num_blocks = rc16->returned_lba;
	scsi_free_scsi_task(task);

	ret = 0;

	/* prefetch the first 1 - 256 blocks at the start of the LUN */
	printf("Prefetching first 1-256 blocks ... ");
	for (i = 1; i <= 256; i++) {
	  task = iscsi_prefetch16_sync(iscsi, lun, 0, i, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send prefetch16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test2;
		}
		if (task->status        == SCSI_STATUS_CHECK_CONDITION
		    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
		    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
			printf("[SKIPPED]\n");
			printf("Opcode is not implemented on target\n");
			scsi_free_scsi_task(task);
			ret = -2;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("Prefetch16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


test2:
	/* Prefetch the last 0 - 255 blocks at the end of the LUN */
	printf("Prefetching last 0-255 blocks ... ");
	for (i = 1; i < 256; i++) {
	  task = iscsi_prefetch16_sync(iscsi, lun, num_blocks - i, i, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send prefetch16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test3;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("Prefetch16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto test3;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test3:

	printf("Prefetching last 0-255 blocks ... ");
	task = iscsi_prefetch16_sync(iscsi, lun, num_blocks - 256, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send prefetch16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test4;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Prefetch16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test4;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test4:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
