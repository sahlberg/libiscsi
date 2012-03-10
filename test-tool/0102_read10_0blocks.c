/* 
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

int T0102_read10_0blocks(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity10 *rc10;
	int ret, lun;
	uint32_t block_size, num_blocks;

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/* find the size of the LUN */
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("Readcapacity command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall readcapacity10 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	block_size = rc10->block_size;
	num_blocks = rc10->lba;
	scsi_free_scsi_task(task);



	ret = 0;

	/* read10 0 blocks one block at lba 0 */
	printf("Reading 0 blocks at lba:0 ... ");
	task = iscsi_read10_sync(iscsi, lun, 0, 0, block_size);
	if (task == NULL) {
 	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
 	        printf("[FAILED]\n");
		printf("Read10 of 0 at lba:0 failed with sense.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	/* read10 0 blocks one block beyond the eol */
	printf("Reading 0 blocks at one block beyond end ... ");
	task = iscsi_read10_sync(iscsi, lun, num_blocks + 1, 0, block_size);
	if (task == NULL) {
 	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
 	        printf("[FAILED]\n");
		printf("Read10 of 0 one block beyond end-of-lun failed with sense.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* read10 0 blocks two blocks beyond the eol */
	printf("Reading 0 blocks at two blocks beyond end ... ");
	task = iscsi_read10_sync(iscsi, lun, num_blocks + 1, 0, block_size);
	if (task == NULL) {
 	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
 	        printf("[FAILED]\n");
		printf("Read10 of 0 two blocks beyond end-of-lun failed with sense.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* read10 0 at lba -1 */
	printf("Reading 0 blocks at lba:-1 ... ");
	task = iscsi_read10_sync(iscsi, lun, 0xffffff, 0, block_size);
	if (task == NULL) {
 	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
 	        printf("[FAILED]\n");
		printf("Read10 of 0 at lba:-1 failed with sense.\n");
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
