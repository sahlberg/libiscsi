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

int T0231_write12_wrprotect(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct iscsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret = 0, i, lun;
	uint32_t block_size;
	unsigned char data[4096];

	printf("0231_write12_wrprotect:\n");
	printf("======================\n");
	if (show_info) {
		printf("Test how WRITE12 handles the wrprotect bits\n");
		printf("1, Any non-zero valued for wrprotect should fail.\n");
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
	if (task->scsi_task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		iscsi_free_task(iscsi, task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task->scsi_task);
	if (rc16 == NULL) {
		printf("failed to unmarshall READCAPACITY16 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		iscsi_free_task(iscsi, task);
		goto finished;
	}

	block_size = rc16->block_length;

	if(rc16->prot_en != 0) {
		printf("device is formatted with protection information, skipping test\n");
		iscsi_free_task(iscsi, task);
		ret = -2;
		goto finished;
	}
	iscsi_free_task(iscsi, task);

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}

	printf("Write12 with RDPROTECT ");
	for (i = 1; i <= 7; i++) {
		task = iscsi_write12_sync(iscsi, lun, 0, data, block_size, block_size, i, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send write12 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->scsi_task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->scsi_task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->scsi_task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		        printf("[FAILED]\n");
			printf("Write12 with RDPROTECT!=0 should have failed with CHECK_CONDITION/ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB\n");
			ret = -1;
			iscsi_free_task(iscsi, task);
			goto finished;
		}
		iscsi_free_task(iscsi, task);
	}
	printf("[OK]\n");

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
