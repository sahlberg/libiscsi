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

int T0292_write10_flags(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret = 0, lun;
	unsigned char data[4096];

	printf("0292_write10_flags:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test how WRITE10 handles the flags\n");
		printf("1, Write with DPU should work.\n");
		printf("2, Write with FUA should work.\n");
		printf("3, Write with FUA_NV should work.\n");
		printf("4, Write with FUA+FUA_NV should work.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (inq->device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		logging(LOG_VERBOSE, "[SKIPPED] Not SBC device."
			" Skipping test");
		return -2;
	}

	printf("Write10 with DPO ");
	task = iscsi_write10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 1, 0, 0, 0);
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
	printf("[OK]\n");

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}


	printf("Write10 with FUA ");
	task = iscsi_write10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 1, 0, 0);
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
	printf("[OK]\n");


	printf("Write10 with FUA_NV ");
	task = iscsi_write10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 1, 0);
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
	printf("[OK]\n");

	printf("Write10 with FUA+FUA_NV ");
	task = iscsi_write10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 1, 1, 0);
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
	printf("[OK]\n");

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
