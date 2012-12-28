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

int T0401_inquiry_alloclen(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun, i;

	printf("0401_inquiry_alloclen:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test INQUIRY with alloclen 0-255.\n");
		printf("1, Test standard inquiry with alloclen 0-255 is successful\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;



	printf("Test INQUIRY with alloclen 0-255 ... ");
	for (i = 0; i < 256; i++) {
		task = iscsi_inquiry_sync(iscsi, lun, 0, 0, i);
		if (task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send INQUIRY command : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("INQUIRY command with alloclen:%d failed : %s\n", i, iscsi_get_error(iscsi));
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
