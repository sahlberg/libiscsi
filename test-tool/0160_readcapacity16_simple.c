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

int T0160_readcapacity16_simple(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_readcapacity16 *rc;
	struct scsi_task *task;
	int ret, lun;

	printf("0160_readcapacity16_simple:\n");
	printf("===========================\n");
	if (show_info) {
		printf("Test that basic READCAPACITY16 works\n");
		printf("1, READCAPACITY16 should work.\n");
		printf("\n");
		return 0;
	}

	ret = 0;

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	printf("Test that READCAPACITY16 is supported ... ");
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
 	        printf("[FAILED]\n");
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
 	        printf("[FAILED]\n");
		printf("READCAPACITY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc = scsi_datain_unmarshall(task);
	if (rc == NULL) {
 	        printf("[FAILED]\n");
		printf("failed to unmarshall READCAPACITY16 data. %s\n", iscsi_get_error(iscsi));
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
