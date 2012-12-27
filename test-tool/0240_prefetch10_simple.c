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

int T0240_prefetch10_simple(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret, i, lun;

	printf("0240_prefetch10_simple:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test basic PREFETCH10 functionality.\n");
		printf("1, Verify we can prefetch the first 0-256 blocks of the LUN.\n");
		printf("2, Verify we can prefetch the last 0-256 blocks of the LUN.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;

	/* prefetch the first 0 - 256 blocks at the start of the LUN */
	printf("Prefetching first 0-256 blocks.\n");
	for (i = 0; i <= 256; i++) {
		ret = prefetch10(iscsi, lun, 0, i, 0, 0);
		if (ret != 0) {
			goto finished;
		}
	}


	/* Prefetch the last 0 - 256 blocks at the end of the LUN */
	printf("Prefetching last 0-256 blocks.\n");
	for (i = 0; i <= 256; i++) {
		ret = prefetch10(iscsi, lun, num_blocks - i, i, 0, 0);
		if (ret != 0) {
			goto finished;
		}
	}


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
