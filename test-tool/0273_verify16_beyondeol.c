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

int T0273_verify16_beyondeol(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, i, lun;
	uint32_t block_size;
	uint64_t num_blocks;
	unsigned char buf[512*256];

	printf("0273_verify16_beyond_eol:\n");
	printf("========================\n");
	if (show_info) {
		printf("Test that VERIFY16 fails if reading beyond end-of-lun.\n");
		printf("1, Verify 2-256 blocks one block beyond end-of-lun.\n");
		printf("2, Verify 1-256 blocks at LBA 2^63\n");
		printf("3, Verify 1-256 blocks at LBA -1\n");
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
		printf("failed to unmarshall READCAPACITY10 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	block_size = rc16->block_length;
	num_blocks = rc16->returned_lba;
	scsi_free_scsi_task(task);



	ret = 0;

	/* verify 2 - 256 blocks beyond the end of the device */
	printf("Verifying 2-256 blocks beyond end-of-device ... ");
	for (i = 2; i <= 256; i++) {
		task = iscsi_verify16_sync(iscsi, lun, buf, i * block_size, num_blocks, 0, 1, 1, block_size);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send verify16 command: %s\n", iscsi_get_error(iscsi));
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
			printf("Verify16 command should fail when reading beyond end of device\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("VERIFY16 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

	/* verify 1 - 256 blocks at LBA 2^63 */
	printf("Verify 1-256 blocks at LBA 2^63 ... ");
	for (i = 2; i <= 257; i++) {
		task = iscsi_verify16_sync(iscsi, lun, buf, i * block_size, 0x8000000000000000ULL, 0, 1, 1, block_size);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send verify16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("Verify16 command should fail when reading at LBA 2^63\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("VERIFY16 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

	/* verify 1 - 256 blocks at LBA -1 */
	printf("Verifying 1-256 blocks at LBA -1 ... ");
	for (i = 2; i <= 257; i++) {
		task = iscsi_verify16_sync(iscsi, lun, buf, i * block_size, -1LL, 0, 1, 1, block_size);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send verify16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test2;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("verify16 command should fail when reading at LBA -1\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
			|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
			printf("[FAILED]\n");
			printf("VERIFY16 failed but with the wrong sense code. It should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test2:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
