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

int T0170_unmap_simple(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret, i, lun;
	uint32_t num_blocks;

	printf("0170_unmap_simple:\n");
	printf("==================\n");
	if (show_info) {
		printf("Test basic UNMAP functionality.\n");
		printf("1, Test UNMAP the first 1-256 blocks of the LUN.\n");
		printf("2, Test UNMAP the last 1-256 blocks of the LUN.\n");
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
		printf("Failed to send readcapacity16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("Readcapacity command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		printf("failed to unmarshall readcapacity16 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}

	if (rc16->lbpme == 0){
		printf("Logical unit is fully provisioned. Skipping test\n");
		ret = -2;
		scsi_free_scsi_task(task);
		goto finished;
	}

	num_blocks = rc16->returned_lba;

	scsi_free_scsi_task(task);

	if (!data_loss) {
		printf("data_loss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	
	ret = 0;

	/* unmap the first 1 - 256 blocks at the start of the LUN */
	printf("Unmapping first 1-256 blocks ... ");
	for (i=1; i<=256; i++) {
		struct unmap_list list[1];

		list[0].lba = 0;
		list[0].num = i;
		task = iscsi_unmap_sync(iscsi, lun, 0, 0, &list[0], 1);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("UNMAP command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* unmap the last 1 - 256 blocks at the end of the LUN */
	printf("Unmapping last 1-256 blocks ... ");
	for (i=1; i<=256; i++) {
		struct unmap_list list[1];

		list[0].lba = num_blocks + 1 - i;
		list[0].num = i;
		task = iscsi_unmap_sync(iscsi, lun, 0, 0, &list[0], 1);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("UNMAP command: failed with sense. %s\n", iscsi_get_error(iscsi));
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
