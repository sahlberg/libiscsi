/* 
   Copyright (C) 2012 by Jon Grimm <jon.grimm@gmail.com>
   
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
#include <ctype.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0420_reserve6_simple(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi, *iscsi2;
	struct scsi_task *task;
	int ret, lun;

	printf("0420_reserve6_simple:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test RESERVE6/RELEASE6 commands if supported.\n");
		printf("  If device does not support, just verify appropriate error returned\n");
		printf("1. Test simple RESERVE6 followed by RELEASE6\n");
		printf("2. Test Initator 1 can reserve if already reserved by Intiator 1.\n");
		printf("3. Test Initiator 2 can't reserve if already reserved by Initiator 1.\n");
		printf("3a. Test Initiator 2 release when reserved by Initiator 1 returns success, but without releasing.\n");
		printf("4. Test Initiator 1 can testunitready if reserved by Initiator 1.\n");
		printf("5. Test Initiator 2 can't testunitready if reserved by Initiator 1.\n");
		printf("6. Test Initiator 2 can get reservation once Intiator 1 releases reservation.\n");
			
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	iscsi2 = iscsi_context_login(initiator, url, &lun);
	if (iscsi2 == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;

	printf("Send RESERVE6 ... ");
	task = iscsi_reserve6_sync(iscsi, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RESERVE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		if (task->status == SCSI_STATUS_CHECK_CONDITION
		    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST 
		    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {		
			printf("[OK]\n");
			printf("RESERVE6 Not Supported\n");
			ret = 0;
		} else {
			printf("[FAILED]\n");
			printf("RESERVE6 failed but ascq was wrong. Should "
			       "have failed with ILLEGAL_REQUEST/"
			       "INVALID OPERATOR. Sense:%s\n", 
			       iscsi_get_error(iscsi));
			ret = -1;	
		}
		scsi_free_scsi_task(task);
		goto finished;
	}
	printf("[OK]\n");

	printf("Send RELEASE6 ... ");
	task = iscsi_release6_sync(iscsi, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RELEASE6 command : %s\n", 
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RELEASE6 command failed : %s\n", 
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("[OK]\n");


test2:
	printf("Test that reservation works.\n");
	printf("Send RESERVE6 from Initiator 1 ... ");
	task = iscsi_reserve6_sync(iscsi, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RESERVE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RESERVE6 command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("[OK]\n");

	printf("Send another RESERVE6 from Initiator 1 ... ");
	task = iscsi_reserve6_sync(iscsi, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RESERVE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RESERVE6 command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("[OK]\n");



test3:
	printf("Send RESERVE6 from Initiator 2. Expect conflict. ... ");
	task = iscsi_reserve6_sync(iscsi2, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RESERVE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	/* We expect this command to fail for the test to pass. */
	if (task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		printf("[FAILED]\n");
		printf("Expected RESERVATION CONFLICT\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("[OK]\n");


test3a:
	printf("Send RELEASE6 from Initiator 2. Expect NO-OP ...");
	task = iscsi_release6_sync(iscsi2, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RELEASE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	/* We expect this command to pass. */
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RELEASE6 command: failed with sense %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto test4;
	}
	printf("[OK]\n");


test4:
	printf("Send TESTUNITREADY from Initiator 1 ... ");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TEST UNIT READY command: %s\n", 
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TEST UNIT READY command: failed with sense %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");


test5:
	printf("Send TESTUNITREADY from Initiator 2. Expect conflict. ... ");
	task = iscsi_testunitready_sync(iscsi2, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TEST UNIT READY command: %s\n", 
		       iscsi_get_error(iscsi2));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		printf("[FAILED]\n");
		printf("Expected RESERVATION CONFLICT\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");



test6:
	printf("Test that release actually works\n");
	printf("Send RELEASE6 from Initiator 1 ... ");
	task = iscsi_release6_sync(iscsi, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RELEASE6 command : %s\n", 
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RELEASE6 command failed : %s\n", 
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("[OK]\n");

	printf("Send RESERVE6 Initiator 2 ... ");
	task = iscsi_reserve6_sync(iscsi2, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RESERVE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RESERVE6 command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

	printf("Send RELEASE6 Initiator 2 ... ");
	task = iscsi_reserve6_sync(iscsi2, lun);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send RELEASE6 command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("RELEASE6 command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("[OK]\n");

	scsi_free_scsi_task(task);

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
