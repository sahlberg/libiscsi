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

int T0243_prefetch10_0blocks(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret, lun;

	printf("0243_prefetch10_0blocks:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test that PREFETCH10 works correctly when transfer length is 0 blocks.\n");
		printf("Transfer Length == 0 means to PREFETCH until the end of the LUN.\n");
		printf("1, Prefetch at LBA:0 should work.\n");
		printf("2, Prefetch at one block beyond end-of-lun should fail. (only on LUNs with less than 2^31 blocks)\n");
		printf("3, Prefetch at LBA:2^31 should fail (only on LUNs with less than 2^31 blocks).\n");
		printf("4, Prefetch at LBA:-1 should fail (only on LUNs with less than 2^31 blocks).\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;


	/* prefetch 0blocks at the start of the LUN */
	printf("PREFETCH10 0blocks at LBA==0.\n");
	ret = prefetch10(iscsi, lun, 0, 0, 0, 0);
	if (ret != 0) {
		goto finished;
	}


	/* Prefetch 0 blocks beyond end of the LUN */
	printf("PREFETCH10 0blocks at one block beyond <end-of-LUN>.\n");
	if (num_blocks > 0x80000000) {
	        printf("[SKIPPED]\n");
		printf("LUN is too big, skipping test\n");
		goto finished;
	}
	ret = prefetch10_lbaoutofrange(iscsi, lun, num_blocks + 2, 0, 0, 0);
	if (ret != 0) {
		goto finished;
	}


	/* Prefetch 0blocks at LBA:2^31 */
	printf("PREFETCH10 0blocks at LBA:2^31.\n");
	ret = prefetch10_lbaoutofrange(iscsi, lun, 0x80000000, 0, 0, 0);
	if (ret != 0) {
		goto finished;
	}


	/* Prefetch 0blocks at LBA:-1 */
	printf("PREFETCH10 0blocks at LBA:-1.\n");
	ret = prefetch10_lbaoutofrange(iscsi, lun, 0xffffffff, 0, 0, 0);
	if (ret != 0) {
		goto finished;
	}


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
