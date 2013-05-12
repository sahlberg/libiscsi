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

int T0171_unmap_zero(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;

	printf("0171_unmap_zero:\n");
	printf("================\n");
	if (show_info) {
		printf("Test UNMAP of 0 blocks.\n");
		printf("1, Try to UNMAP 0 blocks at LBA 0 to LBA 255\n");
		printf("2, Try to UNMAP 0 blocks at 0 to 255 blocks from end-of-lun\n");
		printf("3, Try to UNMAP 0 blocks at 1 to 256 blocks beyond end-of-lun\n");
		printf("4, Send UNMAP without any block descriptors\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (rc16 == NULL || rc16->lbpme == 0){
		printf("Logical unit is fully provisioned. Skipping test\n");
		ret = -2;
		goto finished;
	}

	if (!data_loss) {
		printf("data_loss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	
	ret = 0;

	/* unmap no blocks at LBA 0 - 255 */
	printf("Unmapping of no block at lbas 0-255 blocks ... ");
	for (i=0; i<=255; i++) {
		struct unmap_list list[1];

		list[0].lba = i;
		list[0].num = 0;
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


	/* unmap no blocks at the last 1 - 256 blocks at the end of the LUN */
	printf("Unmapping last 1-256 blocks ... ");
	for (i=0; i<=255; i++) {
		struct unmap_list list[1];

		list[0].lba = num_blocks - i;
		list[0].num = 0;
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

	/* unmap no blocks 0-255 blocks beyond the end of the LUN */
	printf("Unmapping no blocks but 0-255 blocks beyong end of LUN... ");
	for (i=0; i<=255; i++) {
		struct unmap_list list[1];

		list[0].lba = num_blocks + 1 + i;
		list[0].num = 0;
		task = iscsi_unmap_sync(iscsi, lun, 0, 0, &list[0], 1);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("STATUS==GOOD. UNMAP command should fail with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		        printf("[FAILED]\n");
			printf("UNMAP fail but ascq was wrong. Should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

	/* unmap with no block descriptors at all */
	printf("Unmap without any block descriptors ... ");
	task = iscsi_unmap_sync(iscsi, lun, 0, 0, NULL, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));			ret = -1;
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
	printf("[OK]\n");

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
