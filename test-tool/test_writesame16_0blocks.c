/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
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
#include "iscsi-test-cu.h"

void
test_writesame16_0blocks(void)
{
	int ret;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

	if (!inq_bl) {
		CU_PASS("BlockLimits VPD is not available. Skipping test.\n");
		return;
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test WRITESAME16 0-blocks at LBA==0 (WSNZ=%d)",
		inq_bl->wsnz);
	memset(scratch, 0, block_size);

	if (inq_bl->wsnz) {
		ret = writesame16(sd, 0,
				  block_size, 0, 0, 0, 0, 0, scratch,
				  EXPECT_INVALID_FIELD_IN_CDB);
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 does not support 0-blocks.");
		CU_ASSERT_EQUAL(ret, 0);
		return;
	}

	ret = writesame16(sd, 0, block_size, 0, 0, 0, 0, 0, scratch,
			  EXPECT_STATUS_GOOD);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 is not implemented.");
		CU_PASS("[SKIPPED] Target does not support WRITESAME16. Skipping test");
		return;
	} else if (ret == -3) {
		CU_PASS("[SKIPPED] Target does not support WRITESAME16 with NUMBER OF LOGICAL BLOCKS == 0");
	} else if (ret == -4) {
		CU_PASS("[SKIPPED] Number of WRITESAME16 logical blocks to be written exceeds MAXIMUM WRITE SAME LENGTH");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test WRITESAME16 0-blocks one block past end-of-LUN");
	ret = writesame16(sd, num_blocks + 1,
			  block_size, 0, 0, 0, 0, 0, scratch,
			  EXPECT_LBA_OOB);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test WRITESAME16 0-blocks at LBA==2^63");
	ret = writesame16(sd, 0x8000000000000000ULL,
			  block_size, 0, 0, 0, 0, 0, scratch,
			  EXPECT_LBA_OOB);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test WRITESAME16 0-blocks at LBA==-1");
	ret = writesame16(sd, -1,
			  block_size, 0, 0, 0, 0, 0, scratch,
			  EXPECT_LBA_OOB);
	CU_ASSERT_EQUAL(ret, 0);
}
