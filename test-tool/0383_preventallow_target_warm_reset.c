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


int T0383_preventallow_target_warm_reset(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;
	struct mgmt_task mgmt_task = {0, 0};
	struct pollfd pfd;

	printf("0383_preventallow_target_warm_reset:\n");
	printf("====================================\n");
	if (show_info) {
		printf("Test that a target reset clears PREVENTALLOW.\n");
		printf("1, Verify we can set PREVENTALLOW (if the medium is removable)\n");
		printf("2, Verify we can no longer eject the media\n");
		printf("3, Send a Warm Reset to the target\n");
		printf("4, Verify we can eject the media\n");
		printf("5, Load the media again in case it was ejected\n");
		printf("6, Clear PREVENTALLOW again\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	
	if (!inq->rmb) {
		printf("Media is not removable. Skipping tests\n");
		ret = -2;
		goto finished;
	}

	ret = 0;


	printf("Try to set PREVENTALLOW ... ");
	task = iscsi_preventallow_sync(iscsi, lun, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREVENTALLOW command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}

	/* SPC doesnt really say anything about what should happen if using PREVENTALLOW 
	 * on a device that does not support medium removals.
	 */
	if (inq->rmb) {
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("PREVENTALLOW command: failed with sense %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Try to eject the media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status     != SCSI_STATUS_CHECK_CONDITION
	||  task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	||  task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command should have failed with ILLEGAL_REQUEST/MEDIUM_REMOVAL_PREVENTED with : failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("Eject failed. [OK]\n");


	printf("Send a Warm Reset to the target ... ");
	iscsi_task_mgmt_target_warm_reset_async(iscsi, mgmt_cb, &mgmt_task);
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
		printf("Failed to reset the target\n");
		goto finished;
	}
	printf("[OK]\n");

again:
	printf("Use TESTUNITREADY and clear any unit attentions.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto again;
	}

	printf("Try to eject the media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command should have worked but it failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Load the media again in case it was ejected ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	/* SBC doesnt really say anything about whether we can LOAD media when the prevent
	 * flag is set
	 */
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Clear the PREVENTALLOW again ... ");
	task = iscsi_preventallow_sync(iscsi, lun, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREVENTALLOW command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	/* SPC doesnt really say anything about what should happen if using PREVENTALLOW 
	 * on a device that does not support medium removals.
	 */
	if (inq->rmb) {
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("PREVENTALLOW command: failed with sense %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
