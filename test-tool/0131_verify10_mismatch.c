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
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0131_verify10_mismatch(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_task *vtask;
	struct scsi_readcapacity10 *rc10;
	int ret, i, lun;
	uint32_t block_size;

	printf("0131_verify10_mismatch:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test basic VERIFY10 functionality.\n");
		printf("1, Verify the first 1-256 blocks with a deliberate error detects the mismatch.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/* find the size of the LUN */
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("Readcapacity command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall readcapacity10 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	block_size = rc10->block_size;
	scsi_free_scsi_task(task);



	ret = 0;

	/* read and verify the first 1 - 256 blocks at the start of the LUN */
	printf("Read+verify first 1-256 blocks ... ");
	for (i = 1; i <= 256; i++) {
		unsigned char *buf;

		task = iscsi_read10_sync(iscsi, lun, 0, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test2;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("Read10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}

		buf = task->datain.data;
		if (buf == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to access DATA-IN buffer %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}
		/* flip a random byte in the data */
		buf[random() % task->datain.size] ^= 'X';

		vtask = iscsi_verify10_sync(iscsi, lun, buf, i * block_size, 0, 0, 1, 1, block_size);
		if (vtask == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send verify10 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto test2;
		}
		if (vtask->status        == SCSI_STATUS_CHECK_CONDITION
		    && vtask->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
		    && vtask->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
			printf("[SKIPPED]\n");
			printf("Opcode is not implemented on target\n");
			scsi_free_scsi_task(task);
			scsi_free_scsi_task(vtask);
			goto finished;
		}
		if (vtask->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("Verify10 command returned sense ok but the data is not matching.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			scsi_free_scsi_task(vtask);
			goto test2;
		}

		if (vtask->sense.key != SCSI_SENSE_MISCOMPARE) {
		        printf("[FAILED]\n");
			printf("Verify10 command returned wrong sense key. MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s\n", SCSI_SENSE_MISCOMPARE, vtask->sense.key, iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			scsi_free_scsi_task(vtask);
			goto test2;
		}

		scsi_free_scsi_task(task);
		scsi_free_scsi_task(vtask);
	}
	printf("[OK]\n");

test2:

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
