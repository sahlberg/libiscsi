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

int T0260_get_lba_status_simple(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;

	printf("0260_get_lba_status_simple:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test basic GET_LBA_STATUS functionality.\n");
		printf("1, Verify we can read a descriptor at the start of the lun.\n");
		printf("2, Verify we can read a descriptor at the end of the lun.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (rc16 == NULL || rc16->lbpme == 0){
		printf("Logical unit is fully provisioned. Skipping test\n");
		ret = -2;
		goto finished;
	}

	ret = 0;

	/* try reading one descriptor at offset 0 */
	printf("Read one descriptor at LBA 0 ... ");
	task = iscsi_get_lba_status_sync(iscsi, lun, 0, 8 + 16);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send GET_LBA_STATUS command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("GET_LBA_STATUS command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* try reading one descriptor at end-of-device */
	printf("Read one descriptor at end-of-device ... ");
	task = iscsi_get_lba_status_sync(iscsi, lun, num_blocks, 8 + 16);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send GET_LBA_STATUS command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("GET_LBA_STATUS command: failed with sense. %s\n", iscsi_get_error(iscsi));
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
