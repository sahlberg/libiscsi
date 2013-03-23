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

int T0350_orwrite_simple(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;
	unsigned int j;
	unsigned char r1data[4096 * 256];
	unsigned char r2data[4096 * 256];
	unsigned char ordata[4096 * 256];

	printf("0350_orwrite_simple:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test basic ORWRITE functionality.\n");
		printf("1, Verify we can write the first 1-255 blocks of the LUN.\n");
		printf("2, Verify we can write the last 1-255 blocks of the LUN.\n");
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


	/* write the first 1 - 255 blocks at the start of the LUN */
	printf("Orwrite first 1-255 blocks ... ");
	for (i = 1; i < 256; i++) {
		task = iscsi_read16_sync(iscsi, lun, 0, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		if (task->datain.data == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to access DATA-IN buffer %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		memcpy(r1data, task->datain.data, i * block_size);
		memset(ordata, 0x5a, i * block_size);
		for (j = 0; j < i * block_size; j++) {
			r2data[j] = r1data[j] | ordata[j];
		}
		scsi_free_scsi_task(task);

		task = iscsi_orwrite_sync(iscsi, lun, 0, ordata, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send ORWRITE command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("ORWRITE command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);

		task = iscsi_read16_sync(iscsi, lun, 0, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		if (task->datain.data == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to access DATA-IN buffer %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		if (memcmp(r2data, task->datain.data, i * block_size)) {
		        printf("[FAILED]\n");
			printf("Blocks were not updated as expected.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* write the last 1 - 255 blocks at the end of the LUN */
	printf("Orwrite last 1-255 blocks ... ");
	for (i = 1; i < 256; i++) {
		task = iscsi_read16_sync(iscsi, lun, num_blocks + 1 - i, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		if (task->datain.data == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to access DATA-IN buffer %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		memcpy(r1data, task->datain.data, i * block_size);
		memcpy(r1data, task->datain.data, i * block_size);
		memset(ordata, 0xa5, i * block_size);
		for (j = 0; j < i * block_size; j++) {
			r2data[j] = r1data[j] | ordata[j];
		}
		scsi_free_scsi_task(task);

		task = iscsi_orwrite_sync(iscsi, lun, num_blocks + 1 - i, ordata, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send ORWRITE command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("ORWRITE command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
		task = iscsi_read16_sync(iscsi, lun, num_blocks + 1 - i, i * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send READ16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("READ16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		if (task->datain.data == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to access DATA-IN buffer %s\n", iscsi_get_error(iscsi));
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}

		if (memcmp(r2data, task->datain.data, i * block_size)) {
		        printf("[FAILED]\n");
			printf("Blocks were not updated as expected.\n");
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
