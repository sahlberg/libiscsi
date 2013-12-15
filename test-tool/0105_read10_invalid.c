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

int T0105_read10_invalid(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct iscsi_data data;
	char buf[4096];
	int ret, lun;

	printf("0105_read10_invalid:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test various protocol violations.\n");
		printf("1, Read 1 block but set xferlength to 0. Should result in residual overflow of 'block_size' bytes.\n");
		printf("2, Read 1 block but set xferlength to 2*'block_size'. Should result in residual underflow of 'block_size' bytes.\n");
		printf("3, Read 1 block but set xferlength to 200. Should result in residual overflow of 'block_size'-200 bytes.\n");
		printf("4, Read 2 blocks but set xferlength to 'block_size'. Should result in residual overflow of 'block_size' bytes.\n");
		printf("5, Read 1 block but send one block as data-out write on the iSCSI level. Should result in both residual overflow and underflow of 'block_size' bytes.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;


	/* Try a read of 1 block but xferlength == 0 */
	printf("Read10 1 block but with iscsi ExpectedDataTransferLength==0 ... ");

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate task structure\n");
		ret = -1;
		goto finished;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 0;

	/* we don't want autoreconnect since some targets will drop the session
	 * on this condition.
	 */
	iscsi_set_noautoreconnect(iscsi, 1);

	if (iscsi_scsi_command_sync(iscsi, lun, task, NULL) == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;

		goto finished;
	}
	if (task->status == SCSI_STATUS_CANCELLED) {
		scsi_free_scsi_task(task);
		printf("Target dropped the session [OK]\n");
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read10 of 1 block with iscsi ExpectedDataTransferLength==0 should not fail.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->residual_status != SCSI_RESIDUAL_OVERFLOW ||
	    task->residual != block_size) {
	        printf("[FAILED]\n");
		printf("Read10 returned incorrect residual overflow.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* in case the previous test failed the session */
	iscsi_set_noautoreconnect(iscsi, 0);

	/* Try a read of 1 block but xferlength == 1024 */
	printf("Read10 1 block but with iscsi ExpectedDataTransferLength==1024 ... ");

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate task structure\n");
		ret = -1;
		goto finished;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 1024;

	if (iscsi_scsi_command_sync(iscsi, lun, task, NULL) == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;

		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read10 of 1 block with iscsi ExpectedDataTransferLength==1024 should not fail.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->residual_status != SCSI_RESIDUAL_UNDERFLOW ||
	    task->residual != block_size) {
	        printf("[FAILED]\n");
		printf("Read10 returned incorrect residual underflow.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Try a read of 1 block but xferlength == 200 */
	printf("Read10 1 block but with iscsi ExpectedDataTransferLength==200 ... ");

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate task structure\n");
		ret = -1;
		goto finished;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 200;

	if (iscsi_scsi_command_sync(iscsi, lun, task, NULL) == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;

		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read10 of 1 block with iscsi ExpectedDataTransferLength==200 should not fail.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->residual_status != SCSI_RESIDUAL_OVERFLOW ||
	    task->residual != block_size - 200) {
	        printf("[FAILED]\n");
		printf("Read10 returned incorrect residual overflow.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Try a read of 2 blocks but xferlength == 'block_size' */
	printf("Read10 2 blocks but with iscsi ExpectedDataTransferLength==%zu ... ", block_size);

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate task structure\n");
		ret = -1;
		goto finished;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 2;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	if (iscsi_scsi_command_sync(iscsi, lun, task, NULL) == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;

		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read10 of 2 blocks with iscsi ExpectedDataTransferLength==%zu should succeed.\n", block_size);
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->residual_status != SCSI_RESIDUAL_OVERFLOW ||
	    task->residual != block_size) {
	        printf("[FAILED]\n");
		printf("Read10 returned incorrect residual overflow.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Try a read of 1 block but make it a data-out write on the iscsi layer */
	printf("Read10 of 1 block but sent as data-out write in iscsi layer ... ");

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		printf("Failed to allocate task structure\n");
		ret = -1;
		goto finished;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = sizeof(buf);

	data.size = sizeof(buf);
	data.data = (unsigned char *)&buf[0];

	if (iscsi_scsi_command_sync(iscsi, lun, task, &data) == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send read10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;

		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("Read10 of 1 block but iscsi data-out write should fail.\n");
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
