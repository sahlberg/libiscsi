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

int T0214_read12_beyondeol(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;

	printf("0214_read12_beyond_eol:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test that READ12 fails if reading beyond end-of-lun.\n");
		printf("This test is skipped for LUNs with more than 2^31 blocks\n");
		printf("1, Read 1-256 blocks one block beyond end-of-lun.\n");
		printf("2, Read 1-256 blocks at LBA 2^31\n");
		printf("2, Read 1-256 blocks at LBA -1\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;

	if (num_blocks >= 0x80000000) {
		printf("[SKIPPED]\n");
		printf("LUN is too big for read-beyond-eol tests with READ12. Skipping test.\n");
		ret = -2;
		goto finished;
	}

	/* read 1 - 256 blocks beyond the end of the device */
	printf("Reading 1-256 blocks beyond end-of-device ... ");
	for (i = 2; i <= 257; i++) {
		task = iscsi_read12_sync(iscsi, lun, num_blocks, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ12 command should fail when reading beyond end of device\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("READ12 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* read 1 - 256 blocks at LBA 2^31 */
	printf("Reading 1-256 blocks at LBA 2^31 ... ");
	if (num_blocks >= 0xffffffff) {
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	for (i = 2; i <= 257; i++) {
		task = iscsi_read12_sync(iscsi, lun, 0x80000000U, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ12 command should fail when reading from LBA 2^31\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("READ12 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* read 1 - 256 blocks at LBA -1 */
	printf("Reading 1-256 blocks at LBA -1 ... ");
	if (num_blocks > 0x80000000) {
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	for (i = 2; i <= 257; i++) {
		task = iscsi_read12_sync(iscsi, lun, -1, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ12 command should fail when reading from LBA -1\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("READ12 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
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
