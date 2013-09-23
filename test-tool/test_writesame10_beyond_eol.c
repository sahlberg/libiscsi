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
test_writesame10_beyond_eol(void)
{ 
	int i, ret;
	unsigned char *buf = alloca(block_size);

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

	if (num_blocks >= 0x80000000) {
		CU_PASS("LUN is too big for write-beyond-eol tests with WRITESAME10. Skipping test.\n");
		return;
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test WRITESAME10 1-256 blocks one block beyond the end");
	memset(buf, 0, block_size);
	for (i = 1; i <= 256; i++) {
		ret = writesame10_lbaoutofrange(iscsic, tgt_lun, num_blocks - i + 1,
						block_size, i,
						0, 0, 0, 0, buf);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support WRITESAME10. Skipping test");
			return;
		}
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test WRITESAME10 1-256 blocks at LBA==2^31");
	for (i = 1; i <= 256; i++) {
		ret = writesame10_lbaoutofrange(iscsic, tgt_lun, 0x80000000,
						block_size, i,
						0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test WRITESAME10 1-256 blocks at LBA==-1");
	for (i = 1; i <= 256; i++) {
		ret = writesame10_lbaoutofrange(iscsic, tgt_lun, -1,
						block_size, i,
						0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test WRITESAME10 2-256 blocks all but one block beyond the end");
	for (i = 2; i <= 256; i++) {
		ret = writesame10_lbaoutofrange(iscsic, tgt_lun, num_blocks - 1,
						block_size, i,
						0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}
}
