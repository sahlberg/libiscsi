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

int T0243_prefetch10_0blocks(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, lun;
	uint64_t num_blocks;

	printf("0243_prefetch10_0blocks:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test that PREFETCH10 works correctly when transfer length is 0 blocks.\n");
		printf("Transfer Length == 0 means to PREFETCH until the end of the LUN.\n");
		printf("1, Prefetch at LBA:0 should work.\n");
		printf("2, Prefetch at one block beyond end-of-lun should fail. (only on LUNs with less than 2^31 blocks)\n");
		printf("3, Prefetch at LBA:2^31 should fail (only on LUNs with less than 2^31 blocks).\n");
		printf("4, Prefetch at LBA:-1 should fail (only on LUNs with less than 2^31 blocks).\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/* find the size of the LUN */
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		printf("failed to unmarshall READCAPACITY16 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	num_blocks = rc16->returned_lba;
	scsi_free_scsi_task(task);

	ret = 0;


	/* prefetch 0blocks at the start of the LUN */
	printf("PREFETCH10 0blocks at LBA==0 ... ");
	task = iscsi_prefetch10_sync(iscsi, lun, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test2;
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
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PREFETCH10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test2;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test2:
	/* Prefetch 0 blocks beyond end of the LUN */
	printf("PREFETCH10 0blocks at one block beyond <end-of-LUN> ... ");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto test3;
	}
	task = iscsi_prefetch10_sync(iscsi, lun, num_blocks + 1, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test3;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("PREFETCH10 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test3:
	/* Prefetch 0blocks at LBA:2^31 */
	printf("PREFETCH10 0blocks at LBA:2^31 ... ");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto test4;
	}
	task = iscsi_prefetch10_sync(iscsi, lun, 0x80000000, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test4;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("PREFETCH10 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test4;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test4:
	/* Prefetch 0blocks at LBA:-1 */
	printf("PREFETCH10 0blocks at LBA:-1 ... ");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto test5;
	}
	task = iscsi_prefetch10_sync(iscsi, lun, 0xffffffff, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test5;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("PREFETCH10 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test5;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test5:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
