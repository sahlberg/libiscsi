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

int T0242_prefetch10_beyondeol(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, i, lun;
	uint64_t num_blocks;

	printf("0242_prefetch10_beyondeol:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test PREFETCH10 for blocks beyond the EOL.\n");
		printf("1, Prefetch 1-256 blocks one block beyond end-of-lun.\n");
		printf("2, Prefetch 1-256 blocks at LBA 2^31 (only on LUNs < 1TB)\n");
		printf("3, Prefetch 1-256 blocks at LBA -1 (only on LUN < 2TB)\n");
		printf("4, Prefetch 2-256 blocks all but one beyond end-of-lun.\n");
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



	/* prefetch 1-256 blocks, one block beyond the end-of-lun */
	printf("Prefetch last 1-256 blocks one block beyond eol ... ");
	for (i = 1; i <= 256; i++) {
		task = iscsi_prefetch10_sync(iscsi, lun, num_blocks + 2 - i, i, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send prefetch10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test2;
		}
		if (task->status    == SCSI_STATUS_CHECK_CONDITION
		&& task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
		&& task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
			printf("[SKIPPED]\n");
			printf("Opcode is not implemented on target\n");
			scsi_free_scsi_task(task);
			ret = -2;
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("PREFETCH10 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test2:
	/* Prefetch 1 - 256 blocks at LBA 2^31 */
	printf("Prefetch 1-256 blocks at LBA 2^31 ... ");
	if (num_blocks > 0x80000000) {
		printf("LUN is too big, skipping test\n");
		goto test3;
	}
	for (i = 1; i <= 256; i++) {
		task = iscsi_prefetch10_sync(iscsi, lun, 0x80000000, i, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test3;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("PREFETCH10 command should fail for LBA 2^31\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test3;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("PREFETCH10 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test3;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


test3:
	/* prefetch 1 - 256 blocks at LBA -1 */
	printf("Prefetch 1-256 blocks at LBA -1 ... ");
	if (num_blocks > 0x80000000) {
		printf("LUN is too big, skipping test\n");
		goto test4;
	}
	for (i = 1; i <= 256; i++) {
		task = iscsi_prefetch10_sync(iscsi, lun, -1, i, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test4;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("PREFETCH10 command should fail for LBA -1\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test4;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("PREFETCH10 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test4;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


 test4:
	/* prefetch 2-256 blocks, all but one block beyond the eol */
	printf("Prefetch 1-255 blocks beyond eol starting at last block ... ");
	for (i=2; i<=256; i++) {
		task = iscsi_prefetch10_sync(iscsi, lun, num_blocks, i, 0, 0);
		if (task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test5;
		}
		if (task->status == SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("PREFETCH10 beyond end-of-lun did not return sense.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test5;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("PREFETCH10 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test5;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


test5:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
