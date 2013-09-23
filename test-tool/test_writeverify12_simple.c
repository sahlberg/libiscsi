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
test_writeverify12_simple(void)
{
	int i, ret;
	unsigned char *buf = alloca(256 * block_size);

	CHECK_FOR_DATALOSS;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test WRITEVERIFY12 of 1-256 blocks at the start of the LUN");

	for (i = 1; i <= 256; i++) {
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = writeverify12(iscsic, tgt_lun, 0, i * block_size,
		    block_size, 0, 0, 0, 0, buf);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] WRITE1VERIFY12 is not implemented.");
			CU_PASS("WRITEVERIFY12 is not implemented.");
			return;
		}	
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test WRITE12 of 1-256 blocks at the end of the LUN");
	for (i = 1; i <= 256; i++) {
		if (maximum_transfer_length && maximum_transfer_length < i) {
			break;
		}
		ret = writeverify12(iscsic, tgt_lun, num_blocks - i,
		    i * block_size, block_size, 0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}

}
