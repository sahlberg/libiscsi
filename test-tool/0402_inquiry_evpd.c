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
#include <ctype.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0402_inquiry_evpd(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun, i;

	printf("0402_inquiry_evpd:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test the EVPD flag.\n");
		printf("1, Test that EVPD==0 and PC!=0 is an error\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;



	printf("Test INQUIRY with EVPD==0 and PC!=0 ... ");
	for (i = 1; i < 256; i++) {
		task = iscsi_inquiry_sync(iscsi, lun, 0, i, 255);
		if (task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send INQUIRY command : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("INQUIRY should have failed with CHECK_CONDITION/ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			ret = -1;
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			printf("[FAILED]\n");
			printf("INQUIRY should have failed with wrong sense code. It failed with %s but should have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			ret = -1;
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
