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
#include <poll.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

struct mgmt_task {
       uint32_t	 status;
       uint32_t	 finished;
};

static void mgmt_cb(struct iscsi_context *iscsi _U_, int status _U_,
				 void *command_data, void *private_data)
{
	struct mgmt_task *mgmt_task = (struct mgmt_task *)private_data;

	mgmt_task->status   = *(uint32_t *)command_data;
	mgmt_task->finished = 1;
}

int T0421_reserve6_lun_reset(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi = NULL, *iscsi2 = NULL;
	struct scsi_task *task;
	int ret, lun;
	struct mgmt_task mgmt_task = {0, 0};
	struct pollfd pfd;

	printf("0421_reserve6_lun_reset:\n");
	printf("========================\n");
	if (show_info) {
		printf("Test that a RESERVE6 is dropped by a LUN-reset\n");
		printf("  If device does not support RESERVE6, just skip the test.\n");
		printf("1, Reserve the device from the first initiator.\n");
		printf("2, Verify we can access the LUN from the first initiator\n");
		printf("3, Verify we can NOT access the LUN from the second initiator\n");
		printf("4, Send a LUN-reset to the target\n");
		printf("5, Verify we can access the LUN from the second initiator\n");
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

	printf("Send a LUN Reset to the target ... ");
	iscsi_task_mgmt_lun_reset_async(iscsi, lun, mgmt_cb, &mgmt_task);
	while (mgmt_task.finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if (poll(&pfd, 1, -1) < 0) {
			printf("Poll failed");
			goto finished;
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			printf("iscsi_service failed with : %s\n", iscsi_get_error(iscsi));
			break;
		}
	}
	if (mgmt_task.status != 0) {
		printf("[FAILED]\n");
		printf("Failed to reset the LUN\n");
		goto finished;
	}
	printf("[OK]\n");


	/* We might be getting UNIT_ATTENTION/BUS_RESET after the lun-reset above.
	   If so just loop and try the TESTUNITREADY again until it clears
	*/
	printf("Use TESTUNITREADY and clear any unit attentions on the second initiator.\n");
again:
	ret = testunitready(iscsi2, lun);
	if (ret != 0) {
		goto again;
	}


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
