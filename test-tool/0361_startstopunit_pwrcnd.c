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
#include <string.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0361_startstopunit_pwrcnd(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	struct scsi_inquiry_standard *inq;
	int ret, i, lun, removable;
	uint32_t block_size;
	uint64_t num_blocks;
	int full_size;

	printf("0361_startstopunit_pwrcnd:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test STARTSTOPUNIT POWERCONDITION functionality.\n");
		printf("1, If PC != 0 we can not eject the media\n");
		printf("2, Try to remount the media\n");
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

	/* See how big this inquiry data is */
	task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, 0, 0, full_size)) == NULL) {
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			return -1;
		}
	}
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	removable = inq->rmb;

	scsi_free_scsi_task(task);

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	

	ret = 0;


	if (removable) {
		printf("Media is removable. STARTSTOPUNIT should work\n");
	} else {
		printf("Media is not removable. STARTSTOPUNIT should fail\n");
	}
	printf("Try to eject media with PC != 0 ... ");
	for (i = 1; i < 16; i++) {
		task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, i, 0, 1, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
			ret++;
			goto test2;
		}
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret++;
			scsi_free_scsi_task(task);
			goto test2;
		}
		scsi_free_scsi_task(task);

		task = iscsi_testunitready_sync(iscsi, lun);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
			ret++;
			goto test2;
		}

		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
			ret++;
			scsi_free_scsi_task(task);
			goto test2;
		}
	}
	scsi_free_scsi_task(task);

	printf("[OK]\n");


test2:
	printf("Try to mount the media again ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test3;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test3;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);

	printf("[OK]\n");


test3:


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
