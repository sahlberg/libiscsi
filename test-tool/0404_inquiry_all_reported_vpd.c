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
#include <ctype.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0404_inquiry_all_reported_vpd(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_inquiry_supported_pages *sup_inq;
	int ret, lun, i;
	int full_size;
	enum scsi_inquiry_pagecode page_code;

	printf("0404_inquiry_all_reported_vpd:\n");
	printf("==========================\n");
	if (show_info) {
		printf("Check the INQUIRY SUPPORTED VPD page.\n");
		printf("1, Check we can read the SUPPORTED VPD page.\n");
		printf("2, Verify we can read each reported page and check the qualifier,device-type and page code on the returned data\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;



	printf("Read SUPPORTED VPD data ... ");
	/* See how big this inquiry data is */
	page_code = SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES;
	task = iscsi_inquiry_sync(iscsi, lun, 1, page_code, 255);
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
		if ((task = iscsi_inquiry_sync(iscsi, lun, 1, page_code, full_size)) == NULL) {
			printf("[FAILED]\n");
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
	}
	sup_inq = scsi_datain_unmarshall(task);
	if (sup_inq == NULL) {
		printf("[FAILED]\n");
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

	printf("Read each page and verify qualifier, type and page code:\n");
	for (i = 0; i < sup_inq->num_pages; i++) {
		struct scsi_task *pc_task;

		printf("Verify page 0x%02x can be read ... ", sup_inq->pages[i]);
		pc_task = iscsi_inquiry_sync(iscsi, lun, 1, sup_inq->pages[i], 255);
		if (pc_task == NULL) {
			printf("[FAILED]\n");
			printf("Failed to send INQUIRY command : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			continue;
		}
		if (pc_task->status != SCSI_STATUS_GOOD) {
			printf("[FAILED]\n");
			printf("Failed to read VPD page : %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(pc_task);
			ret = -1;
			continue;
		}
		printf("[OK]\n");

		printf("Verify page 0x%02x qualifier   ... ", sup_inq->pages[i]);
		if ((pc_task->datain.data[0] & 0xe0) >> 5 != sup_inq->qualifier) {
			printf("[FAILED]\n");
			printf("Qualifier differs between VPD pages: %x != %x\n",
			       pc_task->datain.data[0] & 0xe0, sup_inq->qualifier);
			ret = -1;
			scsi_free_scsi_task(pc_task);
			continue;
		} else {
			printf("[OK]\n");
 		}

		printf("Verify page 0x%02x device type ... ", sup_inq->pages[i]);
		if ((pc_task->datain.data[0] & 0x1f) != sup_inq->device_type) {
			printf("[FAILED]\n");
			printf("Device Type differs between VPD pages: %x != %x\n",
			       pc_task->datain.data[0] & 0x1f, sup_inq->device_type);
			ret = -1;
			scsi_free_scsi_task(pc_task);
			continue;
		} else {
			printf("[OK]\n");
 		}

		printf("Verify page 0x%02x page code   ... ", sup_inq->pages[i]);
		if (pc_task->datain.data[1] != sup_inq->pages[i]) {
			printf("[FAILED]\n");
			printf("Page code is wrong: %x != %x\n",
			       pc_task->datain.data[1], sup_inq->pages[i]);
			ret = -1;
			scsi_free_scsi_task(pc_task);
			continue;
		} else {
			printf("[OK]\n");
 		}

		scsi_free_scsi_task(pc_task);
	}

	scsi_free_scsi_task(task);

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
