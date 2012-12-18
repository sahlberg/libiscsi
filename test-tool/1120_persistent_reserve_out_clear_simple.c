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

int T1120_persistent_reserve_out_clear_simple(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_persistent_reserve_out_basic poc;
	int ret, lun;

	printf("1120_persistent_reserve_out_clear_simple:\n");
	printf("============================================\n");
	if (show_info) {
		printf("Test basic PERSISTENT_RESERVE_OUT/CLEAR functionality.\n");
		printf("1, Verify that CLEAR works.\n"),
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
	
	ret = 0;

	/* Verify that PERSISTENT_RESERVE_OUT/CLEAR works */
	printf("Send PERSISTENT_RESERVE_OUT/CLEAR ... ");
	poc.reservation_key			= 1L;
	poc.service_action_reservation_key	= 2L;
	poc.spec_i_pt				= 0;
	poc.all_tg_pt				= 1;
	poc.aptpl				= 1;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
			SCSI_PERSISTENT_RESERVE_CLEAR,
			SCSI_PERSISTENT_RESERVE_SCOPE_LU,
			SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
			&poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_OUT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PERSISTENT_RESERVE_OUT Not Supported\n");
		ret = -2;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_OUT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
