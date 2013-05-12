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

int T0362_startstopunit_noloej(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;

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

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	

	ret = 0;


	if (!inq->rmb) {
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
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==0 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 0, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==0 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==0 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==1 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==0 NO_FLUSH==1 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 0, 0, 0, 1, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==1 START==0 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

	printf("STARTSTOP LOEJ==0 IMMED==1 NO_FLUSH==1 START==1 does not eject media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 1, 0, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is still present.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	/* in case the previous command did eject the media */
	iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
