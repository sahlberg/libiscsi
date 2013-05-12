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

int T0181_writesame10_unmap_unaligned(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;

	printf("0181_writesame10_unmap_unaligned:\n");
	printf("=================================\n");
	if (show_info) {
		printf("Test unaligned WRITESAME10-UNMAP functionality.\n");
		printf("1, UNMAP the first 1-lbppb blocks at the start of the LUN\n");
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


	if (lbppb < 2) {
		printf("LBPPB==%d  Can not unmap fractional physical block\n", lbppb);
		ret = -1;
		goto finished;
	}

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	
	ret = 0;

	/* unmap the first 1 - lbppb blocks at the start of the LUN */
	printf("Unmapping first 1 - (LBPPB-1) blocks ... ");
	for (i=1; i < lbppb; i++) {
		task = iscsi_writesame10_sync(iscsi, lun, 0,
					      NULL, 0,
					      i,
					      0, 1, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
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
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITESAME10 command to unmap a fractional physical block should fail\n");
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
