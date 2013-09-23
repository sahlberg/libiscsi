
/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_writesame10_simple(void)
{
	int i, ret;
	unsigned char *buf = alloca(block_size);

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test WRITESAME10 of 1-256 blocks at the start of the LUN");

	memset(buf, 0, block_size);
	for (i = 1; i <= 256; i++) {
		ret = writesame10(iscsic, tgt_lun, 0,
				  block_size, i,
				  0, 0, 0, 0, buf);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support WRITESAME10. Skipping test");
			return;
		}
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test WRITESAME10 of 1-256 blocks at the end of the LUN");
	for (i = 1; i <= 256; i++) {
		ret = writesame10(iscsic, tgt_lun, num_blocks - i,
				  block_size, i,
				  0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}

}
