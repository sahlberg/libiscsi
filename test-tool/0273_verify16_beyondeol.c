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

int T0273_verify16_beyondeol(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret, i, lun;
	unsigned char *buf = NULL;

	printf("0273_verify16_beyond_eol:\n");
	printf("========================\n");
	if (show_info) {
		printf("Test that VERIFY16 fails if reading beyond end-of-lun.\n");
		printf("1, Verify 2-256 blocks one block beyond end-of-lun.\n");
		printf("2, Verify 1-256 blocks at LBA 2^63\n");
		printf("3, Verify 1-256 blocks at LBA -1\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	buf = malloc(256 * block_size);

	/* verify 2 - 256 blocks beyond the end of the device */
	printf("Verifying 2-256 blocks beyond end-of-device.\n");
	for (i = 2; i <= 256; i++) {
		ret = verify16_lbaoutofrange(iscsi, lun, num_blocks, i * block_size, block_size, 0, 1, 1, buf);
		if (ret != 0) {
			goto finished;
		}
	}

	/* verify 1 - 256 blocks at LBA 2^63 */
	printf("Verifying 1-256 blocks at LBA 2^63.\n");
	for (i = 1; i <= 256; i++) {
		ret = verify16_lbaoutofrange(iscsi, lun, 0x8000000000000000, i * block_size, block_size, 0, 1, 1, buf);
		if (ret != 0) {
			goto finished;
		}
	}

	/* verify 1 - 256 blocks at LBA -1 */
	printf("Verifying 1-256 blocks at LBA -1.\n");
	for (i = 1; i <= 256; i++) {
		ret = verify16_lbaoutofrange(iscsi, lun, 0xffffffffffffffff, i * block_size, block_size, 0, 1, 1, buf);
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
