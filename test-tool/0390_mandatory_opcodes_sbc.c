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

int T0390_mandatory_opcodes_sbc(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret = 0, lun;
	unsigned char data[4096]; 

	printf("0390_mandatory_opcodes_sbc:\n");
	printf("===========================\n");
	if (show_info) {
		printf("Test support for all mandatory opcodes for SBC devices\n");
		printf("1, Verify FORMAT UNIT is available\n");
		printf("2, Verify INQUIRY is available\n");
		printf("3, Verify MAINTENANCE IN is available (if SCCS bit is set)\n");
		printf("4, Verify MAINTENANCE OUT is available (if SCCS bit is set)\n");
		printf("5, Verify READ CAPACITY10 is available\n");
		printf("6, Verify READ CAPACITY16 is available\n");
		printf("7, Verify RECEIVE DIAGNOSTIC RESULT is available (if ENCSERV bit is set)\n");
		printf("8, Verify REDUNDANCY GROUP IN is available (if SCCS bit is set)\n");
		printf("9, Verify REDUNDANCY GROUP OUT is available (if SCCS bit is set)\n");
		printf("10, Verify REPORT LUNS is available\n");
		printf("11, Verify REQUEST SENSE is available\n");
		printf("12, Verify SEND DIAGNOSTIC is available\n");
		printf("13, Verify SPARE IN is available (if SCCS bit is set)\n");
		printf("14, Verify SPARE OUT is available (if SCCS bit is set)\n");
		printf("15, Verify TEST UNIT READY is available\n");
		printf("16, Verify UNAMP is available (if LBPME bit is set)\n");
		printf("17, Verify VOLUME SET IN is available (if SCCS bit is set)\n");
		printf("18, Verify VOLUME SET OUT is available (if SCCS bit is set)\n");
		printf("19, Verify WRITE SAME16 is available (if LBPME bit is set)\n");
		printf("20, Verify WRITE SAME32 is available (if LBPME bit is set)\n");
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

	printf("Test FORMAT UNIT ... ");
	printf("[TEST NOT IMPLEMENTED YET]\n");


	printf("Test INQUIRY.\n");
	if (inquiry(iscsi, lun, 0, 0, 64, NULL) == -1) {
		ret = -1;
	}


	printf("Test MAINTENANCE IN ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test MAINTENANCE OUT ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test READ CAPACITY10 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun,0 ,0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ CAPACITY10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("READ CAPACITY10 command: failed with sense %s\n", iscsi_get_error(iscsi));
		ret = -1;
	} else {
		printf("[OK]\n");
	}
	scsi_free_scsi_task(task);


	printf("Test READ CAPACITY16 ... ");
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ CAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("READ CAPACITY16 command: failed with sense %s\n", iscsi_get_error(iscsi));
		ret = -1;
	} else {
		printf("[OK]\n");
	}
	scsi_free_scsi_task(task);


	printf("Test RECEIVE DIAGNOSTIC RESULT ... ");
	if (inq->encserv == 0) {
		printf("[ENCSERV == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test REDUNDANCY GROUP IN ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test REDUNDANCY GROUP OUT ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test REPORT LUNS ... ");
	task = iscsi_reportluns_sync(iscsi, 0, 64);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send REPORT LUNS command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("REPORT LUNS command: failed with sense %s\n", iscsi_get_error(iscsi));
		ret = -1;
	} else {
		printf("[OK]\n");
	}
	scsi_free_scsi_task(task);


	printf("Test REQUEST SENSE ... ");
	printf("[TEST NOT IMPLEMENTED YET]\n");


	printf("Test SEND DIAGNOSTIC ... ");
	printf("[TEST NOT IMPLEMENTED YET]\n");


	printf("Test SPARE IN ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
		goto finished;
	}
	printf("[TEST NOT IMPLEMENTED YET]\n");


	printf("Test SPARE OUT ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test TEST UNIT READY.\n");
	if (testunitready(iscsi, lun) == -1) {
		ret = -1;
	}


	printf("Test UNMAP ... ");
	if (rc16 == NULL || rc16->lbpme == 0){
		printf("[LBPME == 0, SKIPPING TEST]\n");
	} else {
		task = iscsi_unmap_sync(iscsi, lun, 0, 0, NULL, 0);
		if (task == NULL) {
		        printf("[FAILED]\n");
			printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("UNMAP command: failed with sense %s\n", iscsi_get_error(iscsi));
			ret = -1;
		} else {
			printf("[OK]\n");
		}
		scsi_free_scsi_task(task);
	}


	printf("Test VOLUME SET IN ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test VOLUME SET OUT ... ");
	if (inq->sccs == 0) {
		printf("[SCCS == 0, SKIPPING TEST]\n");
	} else {
		printf("[TEST NOT IMPLEMENTED YET]\n");
	}


	printf("Test WRITE SAME16 ... ");
	if (rc16 == NULL || rc16->lbpme == 0){
		printf("[LBPME == 0, SKIPPING TEST]\n");
	} else {
		task = iscsi_writesame16_sync(iscsi, lun, 0,
					      data, block_size,
					      1,
					      0, 1, 0, 0);
		if (task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send WRITE SAME16 command: %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
		if (task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("WRITE SAME16 command: failed with sense %s\n", iscsi_get_error(iscsi));
			ret = -1;
		} else {
			printf("[OK]\n");
		}
		scsi_free_scsi_task(task);
	}

	printf("Test WRITE SAME32 ... ");
	printf("[TEST NOT IMPLEMENTED YET]\n");


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
