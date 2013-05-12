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

int T0212_read12_flags(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret = 0, lun;

	printf("0212_read12_flags:\n");
	printf("==================\n");
	if (show_info) {
		printf("Test how READ12 handles the flag bits\n");
		printf("1, Reading with DPO should work\n");
		printf("2, Reading with FUA should work\n");
		printf("3, Reading with FUA_NV should work\n");
		printf("4, Reading with FUA+FUA_NV should work\n");
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

	printf("Read12 with DPO ");
	task = iscsi_read12_sync(iscsi, lun, 0, block_size, block_size, 0, 1, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	printf("[OK]\n");

	printf("Read12 with FUA ");
	task = iscsi_read12_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	printf("[OK]\n");


	printf("Read12 with FUA_NV ");
	task = iscsi_read12_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	printf("[OK]\n");

	printf("Read12 with FUA+FUA_NV ");
	task = iscsi_read12_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
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
