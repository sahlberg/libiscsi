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
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T1110_persistent_reserve_in_serviceaction_range(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun, i;

	printf("1110_persistent_reserve_in_serviceaction_range:\n");
	printf("============================================\n");
	if (show_info) {
		printf("Test the range for PERSISTENT_RESERVE_IN service actions.\n");
		printf("1, Verify that PERSISTENT_RESERVE_IN works.\n"),
		printf("2, Verify that SERVICE ACTIONS 0 - 3 works.\n");
		printf("3, Verify that SERVICE ACTIONS > 3 fail.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;

	/* Verify that PERSISTENT_RESERVE_IN works */
	printf("Verify PERSISTENT_RESERVE_IN works ... ");
	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
			SCSI_PERSISTENT_RESERVE_READ_KEYS,
			16384);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_IN command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PERSISTENT_RESERVE_IN Not Supported\n");
		ret = -2;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_IN command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



	printf("Verify that SERVICE ACTIONS 0 - 3 works ... ");
	for (i = 0; i < 4; i ++) {
		task = iscsi_persistent_reserve_in_sync(iscsi, lun,
				i,
				16384);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send PERSISTENT_RESERVE_IN command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("PERSISTENT_RESERVE_IN with serviceaction:%d failed with sense. %s\n", i, iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	printf("Verify that SERVICE ACTIONS > 3 fails ... ");
	for (i = 4; i < 0x20; i ++) {
		task = iscsi_persistent_reserve_in_sync(iscsi, lun,
				i,
				16384);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send PERSISTENT_RESERVE_IN command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("PERSISTENT_RESERVE_IN command successful for invalid ServiceAction: %d\n", i);
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");



finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
