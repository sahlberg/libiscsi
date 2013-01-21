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

int T0131_verify10_mismatch(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;
	unsigned char *buf = NULL;


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


	buf = malloc(256 * block_size);
	if (buf == NULL) {
		printf("Failed to allocate buffer.\n");
		ret = -1;
		goto finished;
	}

	printf("Read first 256 blocks.\n");
	task = iscsi_read10_sync(iscsi, lun, 0, 256 * block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("READ10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	memcpy(buf, task->datain.data, task->datain.size);
	scsi_free_scsi_task(task);


	ret = 0;

	/* read and verify the first 1 - 256 blocks at the start of the LUN */
	printf("Verify first 1-256 blocks with a miscompare.\n");
	for (i = 1; i <= 256; i++) {
		int offset = random() % (i * block_size);

		/* flip a random byte in the data */
		buf[offset] ^= 'X';

		ret = verify10_miscompare(iscsi, lun, 0, i * block_size, block_size, 0, 1, 1, buf);
		if (ret != 0) {
			goto finished;
		}

		/* flip the byte back */
		buf[offset] ^= 'X';
	}


finished:
	free(buf);
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
