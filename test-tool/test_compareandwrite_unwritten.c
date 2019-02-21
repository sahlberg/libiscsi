/*
   Copyright (C) SUSE LINUX GmbH 2016
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
#include <inttypes.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_compareandwrite_unwritten(void)
{
	int i, n;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

        n = 256;
        if (n + 0U > num_blocks)
                n = num_blocks;
        
	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1-%d blocks at the "
		"start of the LUN, 1 block at a time", n);
	for (i = 1; i <= n; i++) {
		int caw_ret;
		unsigned char read_buf[block_size];

		/* semi-unique 'compare' buffer to trigger miscompare */
		memset(scratch, '%', block_size);
		memset(scratch + block_size, 'B', block_size);

		caw_ret = compareandwrite(sd, i, scratch, 2 * block_size,
				      block_size,
				      0, 0, 0, 0,
				      EXPECT_MISCOMPARE);
		if (caw_ret == -2) {
			CU_PASS("[SKIPPED] COMPARE AND WRITE not supported");
			return;
		}
		READ16(sd, NULL, i, block_size,
		       block_size, 0, 0, 0, 0, 0, read_buf,
		       EXPECT_STATUS_GOOD);

		if (caw_ret == 0) {
			/* miscompare - confirm unwritten block doesn't match compare buf */
			CU_ASSERT_NOT_EQUAL(memcmp(read_buf, scratch,
						   block_size), 0);
		} else {
			/*
			 * unexpected CAW success, or failure with an ASCQ that
			 * doesn't indicate miscompare - tolerate only the
			 * former, by checking that the range now carries data
			 * matching the CAW write buffer.
			 */
			CU_ASSERT_EQUAL(memcmp(read_buf, scratch + block_size,
					       block_size), 0);
		}
	}
}
