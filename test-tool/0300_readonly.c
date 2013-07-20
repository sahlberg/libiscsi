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

int T0300_readonly(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_mode_sense *ms;
	int ret, lun;
	unsigned char data[4096];
	struct unmap_list list[1];
	int full_size;

	ret = -1;

	printf("0300_readonly:\n");
	printf("==============\n");
	if (show_info) {
		printf("Test that all commands that modify the medium fail for readonly devices\n");
		printf("1, WRITE10 at LUN 0 should fail.\n");
		printf("2, WRITE12 at LUN 0 should fail.\n");
		printf("3, WRITE16 at LUN 0 should fail.\n");
		printf("4, WRITESAME10 at LUN 0 should fail.\n");
		printf("5, WRITESAME16 at LUN 0 should fail.\n");
		printf("6, WRITESAME10 with UNMAP at LUN 0 should fail (skipped if not thin-provisioned).\n");
		printf("7, WRITESAME16 with UNMAP at LUN 0 should fail (skipped if not thin-provisioned).\n");
		printf("8, UNMAP at LUN 0 should fail (skipped if not thin-provisioned).\n");
		printf("9, WRITEVERIFY10 at LUN 0 should fail.\n");
		printf("10, WRITEVERIFY12 at LUN 0 should fail.\n");
		printf("11, WRITEVERIFY16 at LUN 0 should fail.\n");
		printf("12, COMPAREANDWRITE at LUN 0 should fail.\n");
		printf("13, ORWRITE at LUN 0 should fail.\n");
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
	if (!data_loss) {
		logging(LOG_VERBOSE, "[SKIPPED] --dataloss flag not set."
			" Skipping test");
		return -2;
	}

	/* verify the device is readonly */
	task = iscsi_modesense6_sync(iscsi, lun, 0, SCSI_MODESENSE_PC_CURRENT,
				     SCSI_MODEPAGE_RETURN_ALL_PAGES, 0,
				     4);
	if (task == NULL) {
		printf("Failed to send modesense6 command: %s\n", iscsi_get_error(iscsi));
		goto finished;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);
		task = iscsi_modesense6_sync(iscsi, lun, 0, SCSI_MODESENSE_PC_CURRENT,
					     SCSI_MODEPAGE_RETURN_ALL_PAGES, 0,
					     full_size);
		if (task == NULL) {
			printf("Failed to send modesense6 command: %s\n", iscsi_get_error(iscsi));
			goto finished;
		}
	}
	ms = scsi_datain_unmarshall(task);
	if (ms == NULL) {
		printf("failed to unmarshall mode sense datain blob\n");
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (!(ms->device_specific_parameter & 0x80)) {
		printf("Device is not read-only. Skipping test\n");
		ret = -2;
		goto finished;
	}
	scsi_free_scsi_task(task);


	ret = 0;


	/* Write one block at lba 0 */
	printf("WRITE10 to LUN 0 ... ");
	task = iscsi_write10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE10 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITE10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITE12 to LUN 0 ... ");
	task = iscsi_write12_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITE12 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITE16 to LUN 0 ... ");
	task = iscsi_write16_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE16 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITE16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITESAME10 to LUN 0 ... ");
	task = iscsi_writesame10_sync(iscsi, lun, 0,
				      data, block_size,
				      1,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITESAME16 to LUN 0 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, 0,
				      data, block_size,
				      1,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME16 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* UNMAP one block at lba 0 */
	printf("WRITESAME10 to UNMAP LUN 0 ... ");
	if (rc16 == NULL || rc16->lbpme == 0){
		printf("LUN is not thin-provisioned. [SKIPPED]\n");
		goto finished;
	}
	task = iscsi_writesame10_sync(iscsi, lun, 0,
				      data, block_size,
				      1,
				      0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* UNMAP one block at lba 0 */
	printf("WRITESAME16 to UNMAP LUN 0 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, 0,
				      data, block_size,
				      1,
				      0, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME16 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* UNMAP one block at lba 0 */
	printf("UNMAP LUN 0 ... ");
	list[0].lba = 0;
	list[0].num = 1;
	task = iscsi_unmap_sync(iscsi, lun, 0, 0, &list[0], 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("UNMAP command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("UNMAP failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITEVERIFY10 to LUN 0 ... ");
	task = iscsi_writeverify10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITEVERIFY10 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITEVERIFY12 to LUN 0 ... ");
	task = iscsi_writeverify12_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITEVERIFY12 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY12 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("WRITEVERIFY16 to LUN 0 ... ");
	task = iscsi_writeverify16_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITEVERIFY16 command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("COMPAREWRITE to LUN 0 ... ");
	task = iscsi_compareandwrite_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send COMPAREANDWRITE command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("COMPAREANDWRITE command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("COMPAREANDWRITE failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Write one block at lba 0 */
	printf("ORWRITE to LUN 0 ... ");
	task = iscsi_orwrite_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send ORWRITE command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("ORWRITE command should fail when writing to readonly devices\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("ORWRITE failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
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
