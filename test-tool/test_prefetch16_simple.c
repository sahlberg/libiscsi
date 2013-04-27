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
test_prefetch16_simple(void)
{
	int i, ret;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test PREFETCH16 of 1-256 blocks at the start of the LUN");
	for (i = 1; i <= 256; i++) {
		ret = prefetch16(iscsic, tgt_lun, 0, i, 0, 0);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] PREFETCH16 is not implemented.");
			CU_PASS("PREFETCH16 is not implemented.");
			return;
		}	
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test PREFETCH16 of 1-256 blocks at the end of the LUN");
	for (i = 1; i <= 256; i++) {
		ret = prefetch16(iscsic, tgt_lun, num_blocks - i, i, 0, 0);
		CU_ASSERT_EQUAL(ret, 0);
	}
}
