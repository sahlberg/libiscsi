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

int T0324_writeverify12_beyondeol(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;
	unsigned char data[4096 * 258];

	printf("0324_writeverify12_beyond_eol:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test that WRITEVERIFY12 fails if writing beyond end-of-lun.\n");
		printf("This test is skipped for LUNs with more than 2^31 blocks\n");
		printf("1, Writing 1-256 blocks with one block beyond end-of-lun should fail.\n");
		printf("2, Writing 1-256 blocks at LBA 2^31 should fail.\n");
		printf("3, Writing 1-256 blocks at LBA -1 should fail.\n");
		printf("4, Writing 1-256 blocks all but one block beyond eol\n");
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

	if (num_blocks >= 0x80000000) {
		printf("[SKIPPED]\n");
		printf("LUN is too big for read-beyond-eol tests with WRITEVERIFY12. Skipping test.\n");
		ret = -2;
		goto finished;
	}

	/* read 1 - 256 blocks beyond the end of the device */
	printf("Writing 1-256 blocks with one block beyond end-of-device ... ");
	for (i = 1; i <= 256; i++) {
		task = iscsi_writeverify12_sync(iscsi, lun, num_blocks + 2 - i, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
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
			printf("WRITEVERIFY12 beyond end-of-lun did not return sense. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY12 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* read 1 - 256 blocks at lba 2^31 */
	printf("Writing 1-256 blocks at LBA 2^31 ... ");
	if (num_blocks > 0x80000000) {
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	for (i = 1; i <= 256; i++) {
		task = iscsi_writeverify12_sync(iscsi, lun, 0x80000000, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY12 beyond end-of-lun did not return sense. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY12 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* read 1 - 256 blocks at lba -1 */
	printf("Writing 1-256 blocks at LBA -1 ... ");
	if (num_blocks >= 0xffffffff) {
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	for (i = 1; i <= 256; i++) {
		task = iscsi_writeverify12_sync(iscsi, lun, 0xffffffff, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY12 beyond end-of-lun did not return sense. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY12 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* read 2-256 blocks, all but one block beyond the eol */
	printf("Writing 1-255 blocks beyond eol starting at last block ... ");
	for (i=2; i<=256; i++) {
		task = iscsi_writeverify12_sync(iscsi, lun, num_blocks, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("WRITEVERIFY12 beyond end-of-lun did not return sense. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY12 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
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
