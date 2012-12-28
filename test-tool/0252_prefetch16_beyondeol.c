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

int T0252_prefetch16_beyondeol(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret, i, lun;

	printf("0252_prefetch16_beyondeol:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test PREFETCH16 for blocks beyond the EOL.\n");
		printf("1, Prefetch 1-256 blocks one block beyond end-of-lun.\n");
		printf("2, Prefetch 1-256 blocks at LBA 2^63 should fail.\n");
		printf("3, Prefetch 1-256 blocks at LBA -1 should fail.\n");
		printf("4, Prefetch 1-256 blocks all but one block beyond eol\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;


	/* prefetch 1-256 blocks, one block beyond the end-of-lun */
	printf("Prefetch last 1-256 blocks one block beyond eol.\n");
	for (i = 1; i <= 256; i++) {
		ret = prefetch16_lbaoutofrange(iscsi, lun, num_blocks + 2 - i, i, 0, 0);
		if (ret != 0) {
			goto finished;
		}
	}


	/* Prefetch 1 - 256 blocks at LBA 2^63 */
	printf("Prefetch 1-256 blocks at LBA 2^63.\n");
	for (i = 1; i <= 256; i++) {
		ret = prefetch16_lbaoutofrange(iscsi, lun, 0x8000000000000000, i, 0, 0);
		if (ret != 0) {
			goto finished;
		}
	}


	/* prefetch 1 - 256 blocks at LBA -1 */
	printf("Prefetch 1-256 blocks at LBA -1.\n");
	for (i = 1; i <= 256; i++) {
		ret = prefetch16_lbaoutofrange(iscsi, lun, -1, i, 0, 0);
		if (ret != 0) {
			goto finished;
		}
	}


	/* prefetch 2-256 blocks, all but one block beyond the eol */
	printf("Prefetch 1-255 blocks beyond eol starting at last block.\n");
	for (i=2; i<=256; i++) {
		ret = prefetch16_lbaoutofrange(iscsi, lun, num_blocks, i, 0, 0);
		if (ret != 0) {
			goto finished;
		}
	}


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
