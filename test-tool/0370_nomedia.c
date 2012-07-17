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

int T0370_nomedia(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	struct scsi_inquiry_standard *inq;
	int ret, lun, removable;
	uint32_t block_size;
	uint64_t num_blocks;
	int full_size;
	unsigned char buf[2048];

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

	/* find the size of the LUN */
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		printf("failed to unmarshall READCAPACITY16 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	block_size = rc16->block_length;
	num_blocks = rc16->returned_lba;
	scsi_free_scsi_task(task);

	/* See how big this inquiry data is */
	task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, 0, 0, full_size)) == NULL) {
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			return -1;
		}
	}
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	removable = inq->rmb;

	scsi_free_scsi_task(task);

	ret = 0;


	if (!removable) {
		printf("Media is not removable. Skipping test.\n");
		goto finished;
	}



	printf("Try to eject the media ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret++;
		scsi_free_scsi_task(task);
		goto test2;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test2;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");




/*
 * TESTS THAT READ FROM THE MEDIUM
 */

test2:

	printf("Test TESTUNITREADY ... ");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test3;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test3;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test3:

	printf("Test SYNCHRONIZECACHE10 ... ");
	task = iscsi_synchronizecache10_sync(iscsi, lun, 0, 1, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send SYNCHRONIZECACHE10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test4;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("SYNCHRONIZECACHE10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test4;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test4:

	printf("Test SYNCHRONIZECACHE16 ... ");
	task = iscsi_synchronizecache16_sync(iscsi, lun, 0, 1, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send SYNCHRONIZECACHE16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test5;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("SYNCHRONIZECACHE16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test5;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test5:

	printf("Test READ10 ... ");
	task = iscsi_read10_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test6;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READ10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test6;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test6:

	printf("Test READ12 ... ");
	task = iscsi_read12_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ12 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test7;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READ12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test7;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test7:


	printf("Test READ16 ... ");
	task = iscsi_read16_sync(iscsi, lun, 0, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test8;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READ16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test8;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test8:

	printf("Test READCAPACITY10 ... ");
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READCAPACITY10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test9;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READCAPACITY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test9;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test9:

	printf("Test READCAPACITY16 ... ");
	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test10;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("READCAPACITY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test10;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test10:

	printf("Test GETLBASTATUS ... ");
	task = iscsi_get_lba_status_sync(iscsi, lun, 0, 64);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send GETLBASTATUS command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test11;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("GETLBASTATUS after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test11;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test11:

	printf("Test PREFETCH10 ... ");
	task = iscsi_prefetch10_sync(iscsi, lun, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test12;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("PREFETCH10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test12;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test12:


	printf("Test PREFETCH16 ... ");
	task = iscsi_prefetch16_sync(iscsi, lun, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test13;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("PREFETCH16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test13;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test13:

	printf("Test VERIFY10 ... ");
	task = iscsi_verify10_sync(iscsi, lun, buf, block_size, 0, 0, 0, 1, block_size);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test14;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("VERIFY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test14;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test14:

	printf("Test VERIFY12 ... ");
	task = iscsi_verify12_sync(iscsi, lun, buf, block_size, 0, 0, 0, 1, block_size);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY12 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test15;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("VERIFY12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test15;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test15:

	printf("Test VERIFY16 ... ");
	task = iscsi_verify16_sync(iscsi, lun, buf, block_size, 0, 0, 0, 1, block_size);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test16;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("VERIFY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test16;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test16:

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping all WRITE tests\n");
		goto cleanup;
	}


/*
 * TESTS THAT WRITE TO THE MEDIUM
 */

	printf("Test WRITE10 ... ");
	task = iscsi_write10_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test17;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITE10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test17;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test17:

	printf("Test WRITE12 ... ");
	task = iscsi_write12_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE12 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test18;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITE12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test18;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test18:

	printf("Test WRITE16 ... ");
	task = iscsi_write16_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test19;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITE16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test19;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test19:

	printf("Test WRITEVERIFY10 ... ");
	task = iscsi_writeverify10_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test20;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test20;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test20:

	printf("Test WRITEVERIFY12 ... ");
	task = iscsi_writeverify12_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY12 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test21;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test21;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test21:

	printf("Test WRITEVERIFY16 ... ");
	task = iscsi_writeverify16_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITEVERIFY16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test22;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITEVERIFY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test22;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test22:
	printf("Test ORWRITE ... ");
	task = iscsi_orwrite_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send ORWRITE command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test23;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("ORWRITE after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test23;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test23:
	printf("Test COMPAREWRITE ... ");
	task = iscsi_compareandwrite_sync(iscsi, lun, 0, buf, block_size, block_size, 0, 0, 1, 1, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send COMPAREWRITE command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test24;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("COMPAREWRITE after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test24;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test24:

	printf("Test WRITESAME10 ... ");
	task = iscsi_writesame10_sync(iscsi, lun, buf, block_size, 0, 1, 0, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME10 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test25;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITESAME10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test25;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test25:
	printf("Test WRITESAME16 ... ");
	task = iscsi_writesame16_sync(iscsi, lun, buf, block_size, 0, 1, 0, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITESAME16 command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test26;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("WRITESAME16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test26;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test26:

	printf("Test UNMAP ... ");
	task = iscsi_unmap_sync(iscsi, lun, 0, 0, NULL, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send UNMAP command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto test27;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("UNMAP after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		ret++;
		scsi_free_scsi_task(task);
		goto test27;
	}	
	scsi_free_scsi_task(task);

	printf("[OK]\n");

test27:


cleanup:
	printf("Try to mount the media again ... ");
	task = iscsi_startstopunit_sync(iscsi, lun, 1, 0, 0, 0, 1, 1);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send STARTSTOPUNIT command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("STARTSTOPUNIT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret++;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);

	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n", iscsi_get_error(iscsi));
		ret++;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY command: failed with sense after STARTSTOPUNIT %s\n", iscsi_get_error(iscsi));
		ret++;
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
