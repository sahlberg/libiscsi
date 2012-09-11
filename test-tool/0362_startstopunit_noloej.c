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

int T0362_startstopunit_noloej(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_inquiry_standard *inq;
	int ret, lun, removable;
	int full_size;

	printf("0362_startstopunit_noloej:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test STARTSTOPUNIT and LOEJ==0 will never eject/load media.\n");
		printf("1, LOEJ==0  IMMED==0 NO_FLUSH==0 START==0 will not eject the media\n");
		printf("2, LOEJ==0  IMMED==0 NO_FLUSH==0 START==1 will not eject the media\n");
		printf("3, LOEJ==0  IMMED==1 NO_FLUSH==0 START==0 will not eject the media\n");
		printf("4, LOEJ==0  IMMED==1 NO_FLUSH==0 START==1 will not eject the media\n");
		printf("5, LOEJ==0  IMMED==0 NO_FLUSH==1 START==0 will not eject the media\n");
		printf("6, LOEJ==0  IMMED==0 NO_FLUSH==1 START==1 will not eject the media\n");
		printf("7, LOEJ==0  IMMED==1 NO_FLUSH==1 START==0 will not eject the media\n");
		printf("8, LOEJ==0  IMMED==1 NO_FLUSH==1 START==1 will not eject the media\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

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
		printf("Media is not removable. SKIPPING tests\n");
		ret = -2;
		goto finished;
	}



	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==0 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test2;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test2;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test2:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==0 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 0, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test3;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test3;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test3;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test3:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==0 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test4;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test4;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test4;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test4;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test4:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==0 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test5;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test5;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test5;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test5;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test5:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==1 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test6;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test6;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test6;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test6;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test6:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==1 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 1, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test7;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test7;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test7;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test7;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test7:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==1 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test8;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test8;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test8;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test8;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test8:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==1 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 1, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test9;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test9;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test9;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto test9;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test9:
	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
