/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronneisahlberg@gmail.com>
   
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
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"


void
test_verify12_beyond_eol(void)
{ 
	int i, ret;
	unsigned char *buf = alloca(256 * block_size);

	if (num_blocks >= 0x80000000) {
		CU_PASS("LUN is too big for read-beyond-eol tests with VERIFY12. Skipping test.\n");
		return;
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test VERIFY12 1-256 blocks one block beyond the end");
	for (i = 1; i <= 256; i++) {
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = verify12_lbaoutofrange(iscsic, tgt_lun, num_blocks + 1 - i,
					   i * block_size, block_size,
					   0, 0, 1, buf);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] VERIFY12 is not implemented.");
			CU_PASS("[SKIPPED] Target does not support VERIFY12. Skipping test");
			return;
		}
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test VERIFY12 1-256 blocks at LBA==2^31");
	for (i = 1; i <= 256; i++) {
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = verify12_lbaoutofrange(iscsic, tgt_lun, 0x80000000,
					   i * block_size, block_size,
					   0, 0, 1, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test VERIFY12 1-256 blocks at LBA==-1");
	for (i = 1; i <= 256; i++) {
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = verify12_lbaoutofrange(iscsic, tgt_lun, -1, i * block_size,
					   block_size, 0, 0, 1, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test VERIFY12 2-256 blocks all but one block beyond the end");
	for (i = 2; i <= 256; i++) {
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = verify12_lbaoutofrange(iscsic, tgt_lun, num_blocks - 1,
					   i * block_size, block_size,
					   0, 0, 1, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}
}
