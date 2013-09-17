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

int T0341_compareandwrite_mismatch(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, i, lun;
	unsigned char data[4096 * 256];

	printf("0341_compareandwrite_mismatch:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test COMPAREANDWRITE can detect a data mismatch.\n");
		printf("1, Verify we detect a mismatch in the first 1-255 blocks of the LUN.\n");
		printf("2, Verify we detect a mismatch in the last 1-255 blocks of the LUN.\n");
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
	printf("Compare and write first 1-255 blocks (data is not matching) ... ");
	for (i = 1; i < 256; i++) {
		task = iscsi_read16_sync(iscsi, lun, 0, i * 2 * block_size, block_size, 0, 0, 0, 0, 0);
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
		memcpy(data, task->datain.data, i * block_size);
		scsi_free_scsi_task(task);

		/* flip some bits */
		data[ (i - 1) * block_size] ^= 0xa5;
		/* set the write part of the data-out buffer to 1s */
		memset(data + (i * block_size), 0xff, (i * block_size));

		task = iscsi_compareandwrite_sync(iscsi, lun, 0, data, i * 2 * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send COMPAREANDWRITE command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status        == SCSI_STATUS_CHECK_CONDITION
		    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
		    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
			printf("[SKIPPED]\n");
			printf("Opcode is not implemented on target\n");
			scsi_free_scsi_task(task);
			ret = -2;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("COMPAREANDWRITE successful. It should have failed with MISCOMPARE/MISCOMPARE_DURING_VERIFY\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status    != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_MISCOMPARE
		|| task->sense.ascq != SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY) {
		        printf("[FAILED]\n");
			printf("COMPAREANDWRITE Failed with the wrong sense : %s(0x%02x)/%s(0x%04x). It should have failed with MISCOMPARE/MISCOMPARE_DURING_VERIFY\n", scsi_sense_key_str(task->sense.key), task->sense.key, scsi_sense_ascq_str(task->sense.ascq), task->sense.ascq);
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* write the last 1 - 255 blocks at the end of the LUN */
	printf("Compare and write last 1-255 blocks (data is not matching) ... ");
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
		memcpy(data, task->datain.data, i * block_size);
		scsi_free_scsi_task(task);

		/* flip some bits */
		data[ (i - 1) * block_size] ^= 0xa5;
		/* set the write part of the data-out buffer to 1s */
		memset(data + (i * block_size), 0xff, (i * block_size));

		task = iscsi_compareandwrite_sync(iscsi, lun, num_blocks + 1 - i, data, i * 2 * block_size, block_size, 0, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send COMPAREANDWRITE command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status == SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("COMPAREANDWRITE successful. It should have failed with MISCOMPARE/MISCOMPARE_DURING_VERIFY\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
		if (task->status    != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_MISCOMPARE
		|| task->sense.ascq != SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY) {
		        printf("[FAILED]\n");
			printf("COMPAREANDWRITE Failed with the wrong sense : %s(0x%02x)/%s(0x%04x). It should have failed with MISCOMPARE/MISCOMPARE_DURING_VERIFY\n", scsi_sense_key_str(task->sense.key), task->sense.key, scsi_sense_ascq_str(task->sense.ascq), task->sense.ascq);
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
