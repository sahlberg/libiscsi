/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0161_readcapacity16_alloclen(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;

	printf("0161_readcapacity16_alloclen:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test allocation-length for READCAPACITY16\n");
		printf("1, READCAPACITY16 with alloclen==0 is not an error\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;


	printf("READCAPACITY16 with AllocationLength==0 ... ");
	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate task structure\n");
		ret = -1;
		goto finished;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = 0x9e;
	task->cdb[1] = 0x10;
	task->cdb_size = 16;
	task->xfer_dir = SCSI_XFER_NONE;
	task->expxferlen = 0;

	if (iscsi_scsi_command_sync(iscsi, lun, task, NULL) == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;

		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("READCAPACITY16 with AllocationLength==0 should not fail. Sense:%s\n", iscsi_get_error(iscsi));
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
