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

int T0264_get_lba_status_beyondeol(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;

	printf("0264_get_lba_status_beyondeol:\n");
	printf("==============================\n");
	if (show_info) {
		printf("Test GET_LBA_STATUS functionality for beyond end-of-lun requests\n");
		printf("1, Reading a descriptor beyond the end of the lun should fail.\n");
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

	/* try reading one descriptor beyond end-of-device */
	printf("Read one descriptor beyond end-of-device ... ");
	task = iscsi_get_lba_status_sync(iscsi, lun, num_blocks + 1, 8 + 16);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send GET_LBA_STATUS command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("GET_LBA_STATUS beyond eol should fail with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
	        printf("[FAILED]\n");
		printf("GET_LBA_STATUS failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
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
