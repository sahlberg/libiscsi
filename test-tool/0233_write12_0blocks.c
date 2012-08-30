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

int T0233_write12_0blocks(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret = 0, lun;
	uint32_t block_size;
	uint64_t num_blocks;

	printf("0233_write12_0blocks:\n");
	printf("====================\n");
	if (show_info) {
		printf("Test that WRITE12 works correctly when writing 0 number of blocks.\n");
		printf("1, Read at LBA:0 should work.\n");
		printf("2, Read at LBA:end-of-lun should work.\n");
		printf("3, Read at LBA:end-of-lun+1 should fail.\n");
		printf("4, Read at LBA:2^63 should fail (only on LUNs < 2TB).\n");
		printf("5, Read at LBA:-1 should fail (only on LUNs < 2TB).\n");
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

	block_size = rc16->block_length;
	num_blocks = rc16->returned_lba;
	scsi_free_scsi_task(task);

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}


	printf("WRITE12 0blocks at LBA:0 ");
	task = iscsi_write12_sync(iscsi, lun, 0, NULL, 0, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send write12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test2;
	}
	printf("[OK]\n");


test2:
	printf("WRITE12 0blocks at LBA:<end-of-disk> ");
	task = iscsi_write12_sync(iscsi, lun, num_blocks, NULL, 0, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send write12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test3;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test3;
	}
	printf("[OK]\n");


test3:
	printf("WRITE12 0blocks at LBA:<beyond end-of-disk> ");
	task = iscsi_write12_sync(iscsi, lun, num_blocks + 1, NULL, 0, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send write12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test4;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command: Should fail when writing 0blocks beyond end\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto test4;
	}
	printf("[OK]\n");

test4:
	printf("WRITE12 0blocks at LBA 2^31 ... ");
	if (num_blocks > 0x80000000) {
		printf("LUN is too big, skipping test\n");
		goto test5;
	}
	task = iscsi_write12_sync(iscsi, lun, 0x80000000, NULL, 0, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test5;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command: Should fail when writing 0blocks at 2^31\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto test5;
	}
	printf("[OK]\n");


test5:
	printf("WRITE12 0blocks at LBA -1 ... ");
	if (num_blocks > 0x80000000) {
		printf("LUN is too big, skipping test\n");
		goto test5;
	}
	task = iscsi_write12_sync(iscsi, lun, -1, NULL, 0, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test6;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command: Should fail when writing 0blocks at -1\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto test6;
	}
	printf("[OK]\n");


test6:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
