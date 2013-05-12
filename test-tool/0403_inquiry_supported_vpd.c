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

int T0403_inquiry_supported_vpd(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_inquiry_supported_pages *std_inq;
	size_t i;
	int ret, lun, j;
	int full_size;
	int page_code;
	enum scsi_inquiry_pagecode required_spc_pages[] = {
		SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES,
		SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION
	};

	printf("0403_inquiry_supported_vpd:\n");
	printf("==========================\n");
	if (show_info) {
		printf("Check the INQUIRY SUPPORTED VPD page.\n");
		printf("1, Check we can read the SUPPORTED VPD page.\n");
		printf("2, Verify we have all mandatory SPC VPD pages\n");
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
	std_inq = scsi_datain_unmarshall(task);
	if (std_inq == NULL) {
		printf("[FAILED]\n");
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	printf("[OK]\n");

	printf("Verify we have all mandatory SPC VPD pages:\n");
	for (i = 0; i < sizeof(required_spc_pages) / sizeof(enum scsi_inquiry_pagecode); i++) {
		printf("Verify the target supports page 0x%02x ... ", required_spc_pages[i]);
		for (j = 0; j < std_inq->num_pages; j++) {
			if (required_spc_pages[i] == std_inq->pages[j]) {
				break;
			}
		}
		if (j == std_inq->num_pages) {
			printf("[FAILED]\n");
			printf("Target did not report page 0x%02x. This page is mandatory in SPC.\n", required_spc_pages[i]);
			ret = -1;
		} else {
			printf("[OK]\n");
		}
	}

	scsi_free_scsi_task(task);

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
