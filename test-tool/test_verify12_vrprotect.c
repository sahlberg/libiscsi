
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
test_verify12_vrprotect(void)
{
	int i, ret;

	logging(LOG_VERBOSE, "");
	logging(LOG_VERBOSE, "Test VERIFY12 with non-zero VRPROTECT");

	for (i = 1; i < 8; i++) {
		unsigned char *buf = malloc(block_size);

		ret = read12(iscsic, tgt_lun, 0, block_size,
		    block_size, 0, 0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);

		ret = verify12_invalidfieldincdb(iscsic, tgt_lun, 0, block_size,
						 block_size, i, 0, 1, buf);
		free(buf);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support VERIFY12. Skipping test");
			return;
		}
		CU_ASSERT_EQUAL(ret, 0);
	}
}
