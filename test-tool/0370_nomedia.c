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

int T0370_nomedia(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	int ret, lun;
	unsigned char buf[4096];

	printf("0370_nomedia:\n");
	printf("============\n");
	if (show_info) {
		printf("Test that media access commands fail correctly if media is ejected\n");
		printf("1, Verify we can eject the media\n");
		printf("2, Verify TESTUNITREADY\n");
		printf("3, Verify SYNCHRONIZECACHE10\n");
		printf("4, Verify SYNCHRONIZECACHE16\n");
		printf("5, Verify READ10\n");
		printf("6, Verify READ12\n");
		printf("7, Verify READ16\n");
		printf("8, Verify READCAPACITY10\n");
		printf("9, Verify READCAPACITY16\n");
		printf("10, Verify GETLBASTATUS\n");
		printf("11, Verify PREFETCH10\n");
		printf("12, Verify PREFETCH16\n");
		printf("13, Verify VERIFY10\n");
		printf("14, Verify VERIFY12\n");
		printf("15, Verify VERIFY16\n");
		printf("Write commands (only if --dataloss is specified)\n");
		printf("16, Verify WRITE10\n");
		printf("17, Verify WRITE12\n");
		printf("18, Verify WRITE16\n");
		printf("19, Verify WRITEVERIFY10\n");
		printf("20, Verify WRITEVERIFY12\n");
		printf("21, Verify WRITEVERIFY16\n");
		printf("22, Verify ORWRITE\n");
		printf("23, Verify COMPAREWRITE\n");
		printf("24, Verify WRITESAME10\n");
		printf("25, Verify WRITESAME16\n");
		printf("26, Verify UNMAP\n");

		printf("Verify we can load the media back again\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (!inq->rmb) {
		printf("Media is not removable. Skipping test.\n");
		ret = -2;
		goto finished;
	}

	ret = 0;

	printf("Try to eject the media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



/*
 * TESTS THAT READ FROM THE MEDIUM
 */
	printf("Test TESTUNITREADY.\n");
	ret = testunitready_nomedium(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


	printf("Test SYNCHRONIZECACHE10 ... ");
	task = iscsi_synchronizecache10_sync(iscsi, lun, 0, 1, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send SYNCHRONIZECACHE10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("SYNCHRONIZECACHE10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test SYNCHRONIZECACHE16 ... ");
	task = iscsi_synchronizecache16_sync(iscsi, lun, 0, 1, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send SYNCHRONIZECACHE16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("SYNCHRONIZECACHE16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test READ10 ... ");
	task = iscsi_read10_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READ10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test READ12 ... ");
	task = iscsi_read12_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READ12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test READ16 ... ");
	task = iscsi_read16_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READ16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test READCAPACITY10 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READCAPACITY10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READCAPACITY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test READCAPACITY16 ... ");
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READCAPACITY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test GETLBASTATUS ... ");
	task = iscsi_get_lba_status_sync(iscsi, lun, 0, 64);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send GETLBASTATUS command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("GETLBASTATUS after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test PREFETCH10.\n");
	ret = prefetch10_nomedium(iscsi, lun, 0, 1, 1, 0);
	if (ret != 0) {
		goto finished;
	}


	printf("Test PREFETCH16.\n");
	ret = prefetch16_nomedium(iscsi, lun, 0, 1, 1, 0);
	if (ret != 0) {
		goto finished;
	}


	printf("Test VERIFY10.\n");
	ret = verify10_nomedium(iscsi, lun, 0, block_size, block_size, 0, 0, 1, buf);
	if (ret != 0) {
		goto finished;
	}


	printf("Test VERIFY12.\n");
	ret = verify12_nomedium(iscsi, lun, 0, block_size, block_size, 0, 0, 1, buf);
	if (ret != 0) {
		goto finished;
	}

	printf("Test VERIFY16.\n");
	ret = verify16_nomedium(iscsi, lun, 0, block_size, block_size, 0, 0, 1, buf);
	if (ret != 0) {
		goto finished;
	}



	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping all WRITE tests\n");
		ret = -2;
		goto finished;
	}


/*
 * TESTS THAT WRITE TO THE MEDIUM
 */

	printf("Test WRITE10 ... ");
	task = iscsi_write10_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITE10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITE12 ... ");
	task = iscsi_write12_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITE12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITE16 ... ");
	task = iscsi_write16_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITE16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITEVERIFY10 ... ");
	task = iscsi_writeverify10_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITEVERIFY12 ... ");
	task = iscsi_writeverify12_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITEVERIFY16 ... ");
	task = iscsi_writeverify16_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test ORWRITE ... ");
	task = iscsi_orwrite_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send ORWRITE command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("ORWRITE after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test COMPAREWRITE ... ");
	task = iscsi_compareandwrite_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send COMPAREWRITE command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("COMPAREWRITE after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITESAME10 ... ");
	task = iscsi_writesame10_sync(iscsi, lun, 0,
				      buf, block_size,
				      1,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITESAME10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test WRITESAME16 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, 0,
				      buf, block_size,
				      1,
				      0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITESAME16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");


	printf("Test UNMAP ... ");
	task = iscsi_unmap_sync(iscsi, lun, 0, 0, NULL, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("UNMAP after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");



	printf("Try to mount the media again ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	printf("Check with TESTUNITREADY that the medium is present again.\n");
	ret = testunitready(iscsi, lun);
	if (ret != 0) {
		goto finished;
	}


finished:
	printf("Make sure the media is mounted again before the next test ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");

	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
