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

int T0382_preventallow_itnexus_loss(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	struct scsi_inquiry_standard *inq;
	int ret, lun, removable;
	uint32_t block_size;
	uint64_t num_blocks;
	int full_size;

	printf("0382_preventallow_itnexus_loss:\n");
	printf("===============================\n");
	if (show_info) {
		printf("Test that an I_T_Nexus loss clears PREVENTALLOW.\n");
		printf("1, Verify we can set PREVENTALLOW (if the medium is removable)\n");
		printf("2, Verify we can no longer eject the media\n");
		printf("3, Tear down the I_T_Nexus and re-login on a new nexus\n");
		printf("4, Verify we can eject the media\n");
		printf("5, Load the media again in case it was ejected\n");
		printf("6, Clear PREVENTALLOW again\n");
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


	if (!removable) {
		printf("Media is not removable. Skipping tests\n");
		ret = -2;
		goto finished;
	}

	printf("Try to set PREVENTALLOW ... ");
	task = iscsi_preventallow_sync(iscsi, lun, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREVENTALLOW command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test2;
	}

	/* SPC doesnt really say anything about what should happen if using PREVENTALLOW 
	 * on a device that does not support medium removals.
	 */
	if (removable) {
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("PREVENTALLOW command: failed with sense %s\n", iscsi_get_error(iscsi));
			ret++;
			scsi_free_scsi_task(task);
			goto test2;
		}
	}
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test2:
	printf("Try to eject the media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test3;
	}
	if (task->status     != SCSI_STATUS_CHECK_CONDITION
	||  task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	||  task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command should have failed with ILLEGAL_REQUEST/MEDIUM_REMOVAL_PREVENTED with : failed with sense. %s\n", iscsi_get_error(iscsi));
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);
	printf("Eject failed. [OK]\n");

test3:
	printf("Tear down the IT_Nexus and create a new one ... ");
	iscsi_destroy_context(iscsi);
	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		goto finished;
	}
	printf("[OK]\n");


//test4:

	printf("Try to eject the media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test5;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command should have worked but it failed with sense. %s\n", iscsi_get_error(iscsi));
		ret++;
		scsi_free_scsi_task(task);
		goto test5;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test5:


	printf("Load the media again in case it was ejected ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test6;
	}
	/* SBC doesnt really say anything about whether we can LOAD media when the prevent
	 * flag is set
	 */
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test6:


	printf("Clear the PREVENTALLOW again ... ");
	task = iscsi_preventallow_sync(iscsi, lun, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREVENTALLOW command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test7;
	}
	/* SPC doesnt really say anything about what should happen if using PREVENTALLOW 
	 * on a device that does not support medium removals.
	 */
	if (removable) {
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("PREVENTALLOW command: failed with sense %s\n", iscsi_get_error(iscsi));
			ret++;
			scsi_free_scsi_task(task);
			goto test7;
		}
	}
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test7:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
