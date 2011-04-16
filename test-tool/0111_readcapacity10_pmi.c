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

int T0111_readcapacity10_pmi(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;
	struct scsi_readcapacity10 *rc10;
	uint32_t block_size, num_blocks;

	ret = 0;

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



	/* lba != 0 and pmi == 0 is an error */
	printf("Readcapacity10 PMI==0 LBA!=0 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, 10, 0);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq !=SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB ) {
	        printf("[FAILED]\n");
		printf("Readcapacity10 PMI==0 and LBA!=0 should fail with sense code.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* PMI==1 LBA==0 */
	printf("Readcapacity10 PMI==1 LBA==0 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 1);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Readcapacity10 PMI==1 and LBA==0 should not fail with sense code.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	/* PMI==1 LBA==EOL-1 */
	printf("Readcapacity10 PMI==1 LBA==EOL-1 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, num_blocks-1, 1);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Readcapacity10 PMI==1 and LBA==EOL-1 should not fail with sense code.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	/* PMI==1 LBA==EOL */
	printf("Readcapacity10 PMI==1 LBA==EOL ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, num_blocks, 1);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Readcapacity10 PMI==1 and LBA==EOL should not fail with sense code.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* PMI==1 LBA==EOL+1 */
	printf("Readcapacity10 PMI==1 LBA==EOL+1 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, num_blocks+1, 1);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Readcapacity10 PMI==1 and LBA==EOL+1 should not fail with sense code.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	/* PMI==1 LBA==-1 */
	printf("Readcapacity10 PMI==1 LBA==-1 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, 0xffffffff, 1);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Readcapacity10 PMI==1 and LBA==-1 should not fail with sense code.\n");
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
