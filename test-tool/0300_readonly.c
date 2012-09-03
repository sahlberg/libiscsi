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

int T0300_readonly(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	struct scsi_inquiry_standard *inq;
	struct scsi_mode_sense *ms;
	int ret, lun;
	uint32_t block_size;
	unsigned char data[4096];
	int full_size;
	int lbpme;
	struct unmap_list list[1];

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

	/* find the size of the LUN */
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		printf("failed to unmarshall READCAPACITY10 data. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		goto finished;
	}
	block_size = rc16->block_length;
	lbpme = rc16->lbpme;

	scsi_free_scsi_task(task);

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}

	/* This test is only valid for SBC devices */
	task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (inq->periperal_device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		printf("LUN is not SBC device. Skipping test\n");
		scsi_free_scsi_task(task);
		return -1;
	}

	/* verify the device is readonly */
	task = iscsi_modesense6_sync(iscsi, lun, 0, SCSI_MODESENSE_PC_CURRENT,
				     SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES, 0,
				     4);
	if (task == NULL) {
		printf("Failed to send modesense6 command: %s\n", iscsi_get_error(iscsi));
		goto finished;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);
		task = iscsi_modesense6_sync(iscsi, lun, 0, SCSI_MODESENSE_PC_CURRENT,
					     SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES, 0,
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
		ret++;
		goto test2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE10 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITE10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test2;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test2:

	/* Write one block at lba 0 */
	printf("WRITE12 to LUN 0 ... ");
	task = iscsi_write12_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE12 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test3;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE12 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITE12 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test3:

	/* Write one block at lba 0 */
	printf("WRITE16 to LUN 0 ... ");
	task = iscsi_write16_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test4;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE16 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test4;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITE16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test4;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test4:

	/* Write one block at lba 0 */
	printf("WRITESAME10 to LUN 0 ... ");
	task = iscsi_writesame10_sync(iscsi, lun, data, block_size, 0, 1, 0, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test5;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test5;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test5;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test5:

	/* Write one block at lba 0 */
	printf("WRITESAME16 to LUN 0 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, data, block_size, 0, 1, 0, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test6;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME16 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test6;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test6;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test6:


	/* UNMAP one block at lba 0 */
	printf("WRITESAME10 to UNMAP LUN 0 ... ");
	if (lbpme == 0) {
		printf("LUN is not thin-provisioned. [SKIPPED]\n");
		goto test7;
	}
	task = iscsi_writesame10_sync(iscsi, lun, data, block_size, 0, 1, 0, 1, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test7;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME10 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test7;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test7;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test7:

	/* UNMAP one block at lba 0 */
	printf("WRITESAME16 to UNMAP LUN 0 ... ");
	if (lbpme == 0) {
		printf("LUN is not thin-provisioned. [SKIPPED]\n");
		goto test8;
	}
	task = iscsi_writesame16_sync(iscsi, lun, data, block_size, 0, 1, 0, 1, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test8;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITESAME16 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test8;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITESAME16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test8;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test8:

	/* UNMAP one block at lba 0 */
	printf("UNMAP LUN 0 ... ");
	if (lbpme == 0) {
		printf("LUN is not thin-provisioned. [SKIPPED]\n");
		goto test9;
	}
	list[0].lba = 0;
	list[0].num = 1;
	task = iscsi_unmap_sync(iscsi, lun, 0, 0, &list[0], 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test9;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("UNMAP command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test9;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("UNMAP failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test9;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


test9:

	/* Write one block at lba 0 */
	printf("WRITEVERIFY10 to LUN 0 ... ");
	task = iscsi_writeverify10_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test10;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITEVERIFY10 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test10;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY10 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test10;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test10:

	/* Write one block at lba 0 */
	printf("WRITEVERIFY12 to LUN 0 ... ");
	task = iscsi_writeverify12_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test11;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITEVERIFY12 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test11;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY12 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test11;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test11:

	/* Write one block at lba 0 */
	printf("WRITEVERIFY16 to LUN 0 ... ");
	task = iscsi_writeverify16_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test12;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITEVERIFY16 command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test12;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY16 failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test12;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test12:
	/* Write one block at lba 0 */
	printf("COMPAREWRITE to LUN 0 ... ");
	task = iscsi_compareandwrite_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send COMPAREANDWRITE command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test13;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("COMPAREANDWRITE command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test13;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("COMPAREANDWRITE failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test13;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test13:

	/* Write one block at lba 0 */
	printf("ORWRITE to LUN 0 ... ");
	task = iscsi_orwrite_sync(iscsi, lun, 0, data, block_size, block_size, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send ORWRITE command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test14;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("ORWRITE command should fail when writing to readonly devices\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test14;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_DATA_PROTECTION
	    || task->sense.ascq != SCSI_SENSE_ASCQ_WRITE_PROTECTED) {
		printf("[FAILED]\n");
		printf("ORWRITE failed with the wrong sense code. Should fail with DATA_PROTECTION/WRITE_PROTECTED\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test14;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test14:


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
