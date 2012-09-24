/* 
   Copyright (C) 2012 by Jon Grimm <jon.grimm@gmail.com>
   
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
#include <ctype.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0410_readtoc_basic(const char *initiator, const char *url, int data_loss, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task, *task1;
	struct scsi_inquiry_standard *inq;
	struct scsi_readcapacity10 *rc10;
	struct scsi_readtoc_list *list, *list1;
	int ret, lun, i, toc_device;
	int media, full_size;

	printf("0410_readtoc_basic:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test Read TOC command.\n");
		printf("  If device does not support, just verify appropriate error returned\n");
		printf("1, Check we can read the TOC: track 0, non-MSF.\n");
		printf("2, Make sure at least 4 bytes returned as header.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;

	printf("Read standard INQUIRY data ... ");
	/* Submit INQUIRY so we can find out the device type */
	task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 255);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send INQUIRY command : %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("INQUIRY command failed : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, 0, 0, full_size)) == NULL) {
			printf("[FAILED]\n");
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
	}
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("[FAILED]\n");
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

	printf("Check device-type is either of DISK, TAPE or CD/DVD  ... ");
	switch (inq->device_type) {
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS:
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQUENTIAL_ACCESS:
		toc_device = 0;
		break;
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MMC:
		toc_device = 1;
		break;
	default:
		printf("[FAILED]\n");
		printf("Device-type is not DISK, TAPE or CD/DVD. Device reported:%s\n", scsi_devtype_to_str(inq->device_type));
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

	printf("CD/DVD Device. Check if media present.\n");
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
 	        printf("[FAILED]\n");
		printf("Failed to send readcapacity10 command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
 	        printf("[FAILED]\n");
		printf("Readcapacity command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
 	        printf("[FAILED]\n");
		printf("failed to unmarshall readcapacity10 data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}

	/* LBA will return 0, if there is no media. */ 
	media = rc10->lba ? 1 : 0;


test1:

	printf("Read TOC format 0000b (TOC)\n");
	if (toc_device) {
		printf(" On MMC Device\n");
	} else {
		printf(" On non-MMC Device\n");
	}

	task = iscsi_readtoc_sync(iscsi, lun, 0, 0, 0, 255);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send READ TOC command : %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}

	/* If this is a non-MMC device, just verify that that comand failed
	   as expected and then bail */
	if (!toc_device) {
		if (task->status == SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("READ TOC Should have failed\n");
			ret = -1;
		} else if (task->status != SCSI_STATUS_CHECK_CONDITION
			   || task->sense.key != SCSI_SENSE_ILLEGAL_REQUEST 
			   || task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {		
			printf("[FAILED]\n");
			printf("READ TOC failed but ascq was wrong. Should have failed with ILLEGAL_REQUEST/INVALID OPERATOR. Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;       
		} else {
			printf("[OK]\n");
			ret = 0;
		}
		
		scsi_free_scsi_task(task);
		goto finished;
	}


	/* If no media, just check if we have appropriate error and bail. */
	if (!media) {
		printf("No media, check we get appropriate error.\n");
		if (task->status == SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("READ TOC Should have failed\n");
			ret = -1;
		} else if (task->status != SCSI_STATUS_CHECK_CONDITION
			   || task->sense.key != SCSI_SENSE_NOT_READY 
			   || (task->sense.ascq 
			       != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT 
			       && task->sense.ascq
			       != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
			       && task->sense.ascq
			       != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {		
			printf("[FAILED]\n");
			printf("READ TOC failed but ascq was wrong. Should "
			       "have failed with MEDIUM_NOT_PRESENT. "
			       "Sense:%s\n", iscsi_get_error(iscsi));
			ret = -1;       
		} else {
			printf("[OK]\n");
			ret = 0;
		}
		
		scsi_free_scsi_task(task);
		goto finished;
	}



	/* We should only still be here if we are an MMC device and 
	   have media. */
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("READ TOC command failed : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

test2:
	full_size = scsi_datain_getfullsize(task);
	if (full_size < 4) {
		printf("[FAILED]\n");
		printf("TOC Data Length %d < 4\n", full_size);
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	list = scsi_datain_unmarshall(task);
	if (list == NULL) {
		printf("[FAILED]\n");
		printf("Read TOC Unmarshall failed\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

test3: 
	printf("Check lba of 1 is same as lba 0\n");
	task1 = iscsi_readtoc_sync(iscsi, lun, 0, 1, 0, 255);
	if (task1 == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send READ TOC command : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	if (task1->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("READ TOC command failed : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		scsi_free_scsi_task(task1);
		ret = -1;
		goto finished;
	}


	full_size = scsi_datain_getfullsize(task1);
	if (full_size < 4) {
		printf("[FAILED]\n");
		printf("TOC Data Length %d < 4\n", full_size);
		scsi_free_scsi_task(task);
		scsi_free_scsi_task(task1);
		ret = -1;
		goto finished;
	}
	list1 = scsi_datain_unmarshall(task1);
	if (list1 == NULL) {
		printf("[FAILED]\n");
		printf("Read TOC Unmarshall failed\n");
		scsi_free_scsi_task(task);
		scsi_free_scsi_task(task1);
		ret = -1;
		goto finished;
	}
	scsi_free_scsi_task(task1);

	if (list->num != list1->num ||
	    list->first != list1->first ||
	    list->last != list1->last) {
		printf("[FAILED]\n");
		printf("Read TOC header of lba 0 != TOC of lba 1.\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}

	for (i=0; i<list->num; i++) {
		printf(" TOC descriptor %i\n", i);
		if (list->desc[i].desc.toc.adr != list1->desc[i].desc.toc.adr ||
		    list->desc[i].desc.toc.control != list1->desc[i].desc.toc.control ||
		    list->desc[i].desc.toc.track != list1->desc[i].desc.toc.track ||
		    list->desc[i].desc.toc.lba != list1->desc[i].desc.toc.lba) {
			printf("[FAILED]\n");
			printf("Read TOC descriptors of lba 0 != TOC of lba 1.\n");
			ret = -1;
			scsi_free_scsi_task(task);
			goto finished;
		}
	}


	printf("[OK]\n");
	scsi_free_scsi_task(task);


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
