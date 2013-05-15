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

int T0190_writesame16_unmap(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int full_size;
	struct scsi_inquiry_logical_block_provisioning *lbp;
	int ret, i, lun;

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

	ret = 0;

	if (rc16 == NULL || rc16->lbpme == 0){
		printf("LBPME not set. Skip test for CPD page 0xB2 (logical block provisioning)\n");
		goto finished;
	}

	/* Check that id we have logical block provisioning we also have the VPD page for it */
	printf("Logical Block Provisioning is available. Check that VPD page 0xB2 exists ... ");

	/* See how big this inquiry data is */
	task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, full_size)) == NULL) {
			printf("[FAILED]\n");
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
	}

	lbp = scsi_datain_unmarshall(task);
	if (lbp == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

	if (lbp->lbpws == 0) {
		printf("Device does not support WRITE_SAME16 for UNMAP. All WRITE_SAME16 commands to unmap should fail.\n");
	}

	
	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}

	ret = 0;

	/* unmap the first 1 - 256 blocks at the start of the LUN */
	printf("Unmapping first 1-256 blocks ... ");
	if (lbp->lbpws == 0) {
		printf("(Should all fail since LBPWS is 0) ");
	}
	for (i=1; i<=256; i++) {
		/* only try unmapping whole physical blocks, of if unmap using ws16 is not supported
		   we test for all and they should all fail */
		if (lbp->lbpws == 1 && i % lbppb) {
			continue;
		}
		task = iscsi_writesame16_sync(iscsi, lun, 0,
					      NULL, 0,
					      i,
					      0, 1, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
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
		if (lbp->lbpws) {
			if (task->status != SCSI_STATUS_GOOD) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto finished;
			}
		} else {
			if (task->status        != SCSI_STATUS_CHECK_CONDITION
			    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command should fail since LBPWS is 0 but failed with wrong sense code %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto finished;
			}
		}

		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* unmap the last 1 - 256 blocks at the end of the LUN */
	printf("Unmapping last 1-256 blocks ... ");
	if (lbp->lbpws == 0) {
		printf("(Should all fail since LBPWS is 0) ");
	}
	for (i=1; i<=256; i++) {
		/* only try unmapping whole physical blocks, of if unmap using ws16 is not supported
		   we test for all and they should all fail */
		if (lbp->lbpws == 1 && i % lbppb) {
			continue;
		}

		task = iscsi_writesame16_sync(iscsi, lun, num_blocks + 1 - i,
					      NULL, 0,
					      i,
					      0, 1, 0, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (lbp->lbpws) {
			if (task->status != SCSI_STATUS_GOOD) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto finished;
			}
		} else {
			if (task->status        != SCSI_STATUS_CHECK_CONDITION
			    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
			    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			        printf("[FAILED]\n");
				printf("WRITESAME16 command should fail since LBPWS is 0 but failed with wrong sense code %s\n", iscsi_get_error(iscsi));
				scsi_free_scsi_task(task);
				ret = -1;
				goto finished;
			}
		}
		scsi_free_scsi_task(task);
	}
	printf("[OK]\n");


	/* Test that UNMAP=0 and ANCHOR==1 fails with check condition */
	printf("Try UNMAP==0 and ANCHOR==1 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, 0,
				      NULL, 0,
				      64,
				      1, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
	    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
	        printf("[FAILED]\n");
		printf("WRITESAME16 with UNMAP=0 ANCHOR=1 failed with wrong sense code %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	/* Test UNMAP=1 and ANCHOR==1 */
	printf("Try UNMAP==1 and ANCHOR==1 ... ");
	if (lbp->anc_sup == 0) {
		printf("(ANC_SUP==0 so check condition expected) ");
	}
	task = iscsi_writesame16_sync(iscsi, lun, 0,
				      NULL, 0,
				      64,
				      1, 1, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (lbp->anc_sup == 0) {
		if (task->status        != SCSI_STATUS_CHECK_CONDITION
		    || task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		    || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
			printf("[FAILED]\n");
			printf("WRITESAME16 with UNMAP=1 ANCHOR=1 failed with wrong sense code %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			ret = -1;
			goto finished;
		}
	} else {
		if (task->status != SCSI_STATUS_GOOD) {
		        printf("[FAILED]\n");
			printf("WRITESAME16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			ret = -1;
			goto finished;
		}
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
