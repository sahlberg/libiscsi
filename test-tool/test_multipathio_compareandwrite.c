/*
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   Copyright (C) 2015 David Disseldorp

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
#include "iscsi-multipath.h"

void
test_multipathio_compareandwrite(void)
{
	int io_bl = 1;	/* 1 block CAW IOs */
	int path;
	int i, ret;
	unsigned char *buf = alloca(2 * io_bl * block_size);
	int maxbl;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;
	MPATH_SKIP_IF_UNAVAILABLE(mp_sds, mp_num_sds);

	if (inq_bl) {
		maxbl = inq_bl->max_cmp;
	} else {
		/* Assume we are not limited */
		maxbl = 256;
	}
	if (maxbl < io_bl) {
		CU_PASS("[SKIPPED] MAXIMUM_COMPARE_AND_WRITE_LENGTH too small");
		return;
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Initialising data prior to COMPARE_AND_WRITE");

	memset(buf, 0, io_bl * block_size);
	ret = writesame10(mp_sds[0], 0,
			  block_size, 256, 0, 0, 0, 0, buf,
			  EXPECT_STATUS_GOOD);
	if (ret == -2) {
		CU_PASS("[SKIPPED] Target does not support WRITESAME10. Skipping test");
		return;
	}
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test multipath COMPARE_AND_WRITE");
	for (i = 0; i < 256; i++) {

		for (path = 0; path < mp_num_sds; path++) {
			logging(LOG_VERBOSE,
				"Test COMPARE_AND_WRITE(%d->%d) using path %d",
				path, path + 1, path);

			/* compare data is first half */
			memset(buf, path, io_bl * block_size);
			/* write data is the second half, wrap around */
			memset(buf + io_bl * block_size, path + 1,
			       io_bl * block_size);

			ret = compareandwrite(mp_sds[path], i,
					      buf, 2 * io_bl * block_size,
					      block_size, 0, 0, 0, 0,
					      EXPECT_STATUS_GOOD);
			if (ret == -2) {
				CU_PASS("[SKIPPED] Target does not support "
					"COMPARE_AND_WRITE. Skipping test");
				return;
			}
			CU_ASSERT_EQUAL(ret, 0);

			logging(LOG_VERBOSE,
				"Test bad COMPARE_AND_WRITE(%d->%d)",
				path, path + 1);

			ret = compareandwrite(mp_sds[path], i,
					      buf, 2 * io_bl * block_size,
					      block_size, 0, 0, 0, 0,
					      EXPECT_MISCOMPARE);
			CU_ASSERT_EQUAL(ret, 0);
		}
	}
}
