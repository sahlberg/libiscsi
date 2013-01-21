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

int T0283_verify12_beyondeol(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret, i, lun;
	unsigned char *buf = NULL;

	printf("0283_verify12_beyond_eol:\n");
	printf("========================\n");
	if (show_info) {
		printf("Test that VERIFY12 fails if reading beyond end-of-lun.\n");
		printf("This test is skipped for LUNs with more than 2^31 blocks\n");
		printf("1, Verify 2-256 blocks one block beyond end-of-lun.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;

	if (num_blocks >= 0x80000000) {
		printf("[SKIPPED]\n");
		printf("LUN is too big for read-beyond-eol tests with VERIFY12. Skipping test.\n");
		ret = -2;
		goto finished;
	}

	buf = malloc(256 * block_size);

	/* verify 2 - 256 blocks beyond the end of the device */
	printf("Verifying 2-256 blocks beyond end-of-device.\n");
	for (i = 2; i <= 256; i++) {
		ret = verify12_lbaoutofrange(iscsi, lun, num_blocks, i * block_size, block_size, 0, 1, 1, buf);
		if (ret != 0) {
			goto finished;
		}
	}


finished:
	free(buf);
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
