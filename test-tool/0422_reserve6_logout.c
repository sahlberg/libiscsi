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
#include <ctype.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0422_reserve6_logout(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi = NULL, *iscsi2 = NULL;
	struct scsi_task *task;
	int ret, lun;

	printf("0422_reserve6_logout:\n");
	printf("=====================\n");
	if (show_info) {
		printf("Test that a RESERVE6 is dropped when the session is logged out\n");
		printf("  If device does not support RESERVE6, just skip the test.\n");
		printf("1, Reserve the device from the first initiator.\n");
		printf("2, Verify we can access the LUN from the first initiator.\n");
		printf("3, Verify we can NOT access the LUN from the second initiator.\n");
		printf("4, Logout the first initiator.\n");
		printf("5, Verify we can access the LUN from the second initiator.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	iscsi2 = iscsi_context_login(initiatorname2, url, &lun);
	if (iscsi2 == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;




	printf("Send RESERVE6 from the first initiator ... ");
	task = iscsi_reserve6_sync(iscsi, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RESERVE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("RESERVE6 Not Supported\n");
		ret = -2;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RESERVE6 failed with sense:%s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Verify we can access the LUN from the first initiator.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	printf("Verify we can NOT access the LUN from the second initiator.\n");
	ret = testunitready_conflict(iscsi2, lun);
	if (ret != 0) {
		goto finished;
	}

	printf("Logout the first initiator ... ");
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	iscsi = NULL;
	printf("[OK]\n");

	printf("Verify we can access the LUN from the second initiator.\n");
	ret = testunitready(iscsi2, lun);
	if (ret != 0) {
		goto finished;
	}


finished:
	if (iscsi2 != NULL) {
		iscsi_logout_sync(iscsi2);
		iscsi_destroy_context(iscsi2);
	}
	if (iscsi != NULL) {
		iscsi_logout_sync(iscsi);
		iscsi_destroy_context(iscsi);
	}
	return ret;
}
