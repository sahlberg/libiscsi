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

int T0334_writeverify16_beyondeol(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, i, lun;
	uint32_t block_size;
	uint64_t num_blocks;
	unsigned char data[258 * 512];

	printf("0334_writeverify16_beyond_eol:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test that WRITEVERIFY10 fails if writing beyond end-of-lun.\n");
		printf("1, Writing 1-256 blocks with one block beyond end-of-lun should fail.\n");
		printf("2, Writing 1-256 blocks at LBA 2^63 should fail.\n");
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

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}


	ret = 0;

	/* read 1 - 256 blocks beyond the end of the device */
	printf("Writing 1-256 blocks with one block beyond end-of-device ... ");
	for (i = 1; i <= 256; i++) {
		task = iscsi_writeverify16_sync(iscsi, lun, num_blocks + 2 - i, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
			ret++;
			goto test2;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY16 command should fail when writing beyond end of device\n");
			ret++;
			scsi_free_scsi_task(task);
			goto test2;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test2:

	/* read 1 - 256 blocks at lba 2^63 */
	printf("Writing 1-256 blocks at LBA 2^63 ... ");
	for (i = 1; i <= 256; i++) {
		task = iscsi_writeverify16_sync(iscsi, lun, 0x8000000000000000, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
			ret++;
			goto test3;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY16 command should fail when writing beyond end of device\n");
			ret++;
			scsi_free_scsi_task(task);
			goto test3;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test3:

	/* read 1 - 256 blocks at lba -1 */
	printf("Writing 1-256 blocks at LBA -1 ... ");
	for (i = 1; i <= 256; i++) {
		task = iscsi_writeverify16_sync(iscsi, lun, 0xffffffffffffffff, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
			ret++;
			goto test4;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY16 command should fail when writing beyond end of device\n");
			ret++;
			scsi_free_scsi_task(task);
			goto test4;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test4:
	/* read 2-256 blocks, all but one block beyond the eol */
	printf("Writing 1-255 blocks beyond eol starting at last block ... ");
	for (i=2; i<=256; i++) {
		task = iscsi_writeverify16_sync(iscsi, lun, num_blocks, data, i * block_size, block_size, 0, 0, 0, 0);
		if (task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test5;
		}
		if (task->status == SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("WRITEVERIFY16 beyond end-of-lun did not return sense.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto test5;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("WRITEVERIFY16 failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
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
