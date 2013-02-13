/* 
   Copyright (C) 2012 by Lee Duncan <lee@gonzoleeman.net>
   
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
#include <string.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_read10_rdprotect(void)
{
	int i, ret;


	if (device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		CU_PASS("[SKIPPED] LUN is not SBC device. Skipping test");
		return;
	}

	/*
	 * Try out different non-zero values for RDPROTECT.
	 * They should all fail.
	 */
	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test READ10 with non-zero RDPROTECT");
	for (i = 1; i < 8; i++) {
		ret = read10_invalidfieldincdb(iscsic, tgt_lun, 0,
					       block_size, block_size,
					       i, 0, 0, 0, 0, NULL);
		CU_ASSERT_EQUAL(ret, 0);
	}
}
