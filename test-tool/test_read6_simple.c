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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_read6_simple(void)
{
	int i, ret;


	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test READ6 of 1-255 blocks at the start of the LUN");
	for (i = 1; i <= 255; i++) {
		ret = read6(iscsic, tgt_lun, 0, i * block_size,
			    block_size, NULL);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] READ6 is not implemented.");
			CU_PASS("READ6 is not implemented.");
			return;
		}	
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test READ6 of 1-255 blocks at the end of the LUN");
	if (num_blocks >= 0x1fffff) {
		CU_PASS("LUN is too big for read-at-eol tests with READ6. Skipping test.\n");
	} else {
		for (i = 1; i <= 255; i++) {
			ret = read6(iscsic, tgt_lun, num_blocks - i,
				i * block_size, block_size, NULL);
			CU_ASSERT_EQUAL(ret, 0);
		}
	}

	logging(LOG_VERBOSE, "Transfer length == 0 means we want to transfer "
		"256 blocks");

	logging(LOG_VERBOSE, "Test sending a READ6 with transfer length == 0 "
		"(meaning 256 blocks)");
	/* 256 is converted to 0 when the CDB is marshalled by the helper */
	task = iscsi_read6_sync(iscsic, tgt_lun, 0,
				256 * block_size, block_size);
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ6 command: "
			"failed with sense. %s", iscsi_get_error(iscsic));
	}
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

	logging(LOG_VERBOSE, "Verify that we did get 256 blocks of data back");
	if (task->datain.size == (int)(256 * block_size)) {
		logging(LOG_VERBOSE, "[SUCCESS] Target returned 256 blocks of "
			"data");
	} else {
		logging(LOG_NORMAL, "[FAILED] Target did not return 256 "
			"blocks of data");
	}
	CU_ASSERT_EQUAL(task->datain.size, (int)(256 * block_size));
}
