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

int T0190_writesame16_unmap(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int full_size;
	struct scsi_inquiry_logical_block_provisioning *inq_lbp;
	int ret, i, lun;
	uint32_t num_blocks;
	int lbppb;
	int lbpme;
	int lbpws = 0;
	int anc_sup = 0;

	printf("0190_writesame16_unmap:\n");
	printf("=======================\n");
	if (show_info) {
		printf("Test basic WRITESAME16-UNMAP functionality.\n");
		printf("1, If LBPME==1 we should have VPD page 0xB2\n");
		printf("2, UNMAP the first 1-256 blocks at the start of the LUN\n");
		printf("3, UNMAP the last 1-256 blocks at the end of the LUN\n");
		printf("4, Verify that UNMAP == 0 and ANCHOR == 1 is invalid\n");
		printf("5, UNMAP == 1 and ANCHOR == 1\n");
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
		printf("Failed to send readcapacity16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("Readcapacity command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		printf("failed to unmarshall readcapacity16 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}

	num_blocks = rc16->returned_lba;
	lbppb = 1 << rc16->lbppbe;
	lbpme = rc16->lbpme;

	scsi_free_scsi_task(task);

	if (lbpme == 0) {
		printf("LBPME not set. Skip test for CPD page 0xB2 (logical block provisioning)\n");
		goto test2;
	}

	/* Check that id we have logical block provisioning we also have the VPD page for it */
	printf("Logical Block Provisioning is available. Check that VPD page 0xB2 exists ... ");

	/* See how big this inquiry data is */
	task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test2;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, full_size)) == NULL) {
			printf("[FAILED]\n");
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test2;
		}
	}

	inq_lbp = scsi_datain_unmarshall(task);
	if (inq_lbp == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto test2;
	}

	lbpws = inq_lbp->lbpws;
	anc_sup = inq_lbp->anc_sup;

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	if (lbpws == 0) {
		printf("Device does not support WRITE_SAME16 for UNMAP. All WRITE_SAME16 commands to unmap should fail.\n");
	}

test2:
	
	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}

	ret = 0;

	/* unmap the first 1 - 256 blocks at the start of the LUN */
	printf("Unmapping first 1-256 blocks ... ");
	if (lbpws == 0) {
		printf("(Should all fail since LBPWS is 0) ");
	}
	for (i=1; i<=256; i++) {
		/* only try unmapping whole physical blocks, of if unmap using ws16 is not supported
		   we test for all and they should all fail */
		if (lbpws == 1 && i % lbppb) {
			continue;
		}
		task = iscsi_writesame16_sync(iscsi, lun, NULL, 0,
					0, i,
					0, 1, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test3;
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
		if (lbpws) {
			if (task->status != SCSI_STATUS_GOOD) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto test3;
			}
		} else {
			if (task->status        != SCSI_STATUS_CHECK_CONDITION
			    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command should fail since LBPWS is 0 but failed with wrong sense code %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto test3;
			}
		}

		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test3:
	/* unmap the last 1 - 256 blocks at the end of the LUN */
	printf("Unmapping last 1-256 blocks ... ");
	if (lbpws == 0) {
		printf("(Should all fail since LBPWS is 0) ");
	}
	for (i=1; i<=256; i++) {
		/* only try unmapping whole physical blocks, of if unmap using ws16 is not supported
		   we test for all and they should all fail */
		if (lbpws == 1 && i % lbppb) {
			continue;
		}

		task = iscsi_writesame16_sync(iscsi, lun, NULL, 0,
					num_blocks + 1 - i, i,
					0, 1, 0, 0, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto test4;
		}
		if (lbpws) {
			if (task->status != SCSI_STATUS_GOOD) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto test4;
			}
		} else {
			if (task->status        != SCSI_STATUS_CHECK_CONDITION
			    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command should fail since LBPWS is 0 but failed with wrong sense code %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto test4;
			}
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");

test4:


	/* Test that UNMAP=0 and ANCHOR==1 fails with check condition */
	printf("Try UNMAP==0 and ANCHOR==1 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, NULL, 0,
					0, 64,
					1, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test5;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
	        printf("[FAILED]\n");
		printf("WRITESAME16 with UNMAP=0 ANCHOR=1 failed with wrong sense code %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto test5;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test5:

	/* Test UNMAP=1 and ANCHOR==1 */
	printf("Try UNMAP==1 and ANCHOR==1 ... ");
	if (anc_sup == 0) {
		printf("(ANC_SUP==0 so check condition expected) ");
	}
	task = iscsi_writesame16_sync(iscsi, lun, NULL, 0,
					0, 64,
					1, 1, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto test5;
	}
	if (anc_sup == 0) {
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			printf("[FAILED]\n");
			printf("WRITESAME16 with UNMAP=1 ANCHOR=1 failed with wrong sense code %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			ret = -1;
			goto test6;
		}
	} else {
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITESAME16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			ret = -1;
			goto test6;
		}
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

test6:



finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
