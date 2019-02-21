/*
   Copyright (C) 2016 David Disseldorp
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
test_writesame16_check(void)
{
	int i;
	int ws_max_blocks = 256;
	unsigned char read_buf[ws_max_blocks * block_size];

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test WRITESAME16 of 1-256 blocks at the start of the LUN");

	for (i = 1; i <= ws_max_blocks; i++) {
		/*
		 * fill the full buffer so that memcmp is straightforward,
		 * even though writesame is only using one block of it.
		 */
		memset(scratch, i, block_size * ws_max_blocks);
		WRITESAME16(sd, 0, block_size, i, 0, 0, 0, 0, scratch,
			    EXPECT_STATUS_GOOD);

		memset(read_buf, 0, i * block_size);
		READ16(sd, NULL, 0, i * block_size,
		       block_size, 0, 0, 0, 0, 0, read_buf,
		       EXPECT_STATUS_GOOD);

		CU_ASSERT_EQUAL(0, memcmp(read_buf, scratch, i));
	}

	logging(LOG_VERBOSE, "Test WRITESAME16 of 1-256 blocks at the end of the LUN");
	for (i = 1; i <= ws_max_blocks; i++) {
		memset(scratch, i, block_size * ws_max_blocks);
		WRITESAME16(sd, num_blocks - i,
			    block_size, i, 0, 0, 0, 0, scratch,
			    EXPECT_STATUS_GOOD);

		memset(read_buf, 0, i * block_size);
		READ16(sd, NULL, num_blocks - i, i * block_size,
		       block_size, 0, 0, 0, 0, 0, read_buf,
		       EXPECT_STATUS_GOOD);

		CU_ASSERT_EQUAL(0, memcmp(read_buf, scratch, i));
	}
}
