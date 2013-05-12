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
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0103_read10_rdprotect(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;

	printf("0103_read10_rdprotect:\n");
	printf("======================\n");
	if (show_info) {
		printf("Test how READ10 handles the rdprotect bits\n");
		printf("1, Any non-zero valued for rdprotect should fail.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (inq->device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		logging(LOG_VERBOSE, "[SKIPPED] Not SBC device."
			" Skipping test");
		return -2;
	}

	ret = 0;

	/* Try out Different non-zero values for RDPROTECT. They should all fail */
	printf("Read10 with non-zero RDPROTECT ... ");
	for (i = 1; i < 8; i++) {

		task = malloc(sizeof(struct scsi_task));

		if (task == NULL) {
			printf("Failed to allocate task structure\n");
			ret = -1;
			goto finished;
		}

		memset(task, 0, sizeof(struct scsi_task));
		task->cdb[0] = SCSI_OPCODE_READ10;
		task->cdb[1] = (i<<5)&0xe0;
		task->cdb[8] = 1;
		task->cdb_size = 10;
		task->xfer_dir = SCSI_XFER_READ;
		task->expxferlen = block_size;

		if (iscsi_scsi_command_sync(iscsi, lun, task, NULL) == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		        printf("[FAILED]\n");
			printf("READ10 with rdprotect should fail with ILLEGAL REQUEST/INVALID_FIELD_IN_CDB\n");
			ret = -1;
			scsi_free_scsi_task(task);
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
