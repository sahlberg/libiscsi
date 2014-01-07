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

int T0184_writesame10_0blocks(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;
	unsigned char buf[4096];

	printf("0184_writesame10_0blocks:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test that WRITESAME10 works correctly when transfer length is 0 blocks.\n");
		printf("1, Writesame at LBA:0 should work.\n");
		printf("2, Writesame at one block beyond end-of-lun should fail. (only on LUNs with less than 2^31 blocks)\n");
		printf("3, Writesame at LBA:2^31 should fail (only on LUNs with less than 2^31 blocks).\n");
		printf("4, Writesame at LBA:-1 should fail (only on LUNs with less than 2^31 blocks).\n");
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

	printf("Writesame10 0blocks at LBA:0 ... ");
	task = iscsi_writesame10_sync(iscsi, lun, 0,
				      buf, block_size,
				      0,
				      0, 0, 0, 0);
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
	if (inq_bl == NULL) {
		printf("[SKIPPED]\n");
		printf("Target does not support block limits VPD\n");
		ret = -2;
		goto finished;
	}
	if ((!inq_bl->wsnz && task->status != SCSI_STATUS_GOOD) ||
	    (inq_bl->wsnz && task->status != SCSI_STATUS_CHECK_CONDITION)) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command: failed with sense and WSNZ = %d. "
		       "%s\n", inq_bl->wsnz, iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Writesame10 0blocks at one block beyond <end-of-LUN> ... ");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	task = iscsi_writesame10_sync(iscsi, lun, num_blocks + 2,
				      buf, block_size,
				      0,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command: Should fail when writing 0blocks beyond end\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Writesame10 0blocks at LBA 2^31 ... ");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	task = iscsi_writesame10_sync(iscsi, lun, 0x80000000,
				      buf, block_size,
				      0,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command: Should fail when writing 0blocks at 2^31\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Writesame10 0blocks at LBA -1 ... ");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	task = iscsi_writesame10_sync(iscsi, lun, -1,
				      buf, block_size,
				      0,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command: Should fail when writing 0blocks at -1\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
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
