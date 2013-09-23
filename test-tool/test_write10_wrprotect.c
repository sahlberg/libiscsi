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
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_write10_wrprotect(void)
{
	int i, ret;
	unsigned char *buf = alloca(block_size);

	/*
	 * Try out different non-zero values for WRPROTECT.
	 */
	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test WRITE10 with non-zero WRPROTECT");

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;

	if (!inq->protect || (rc16 != NULL && !rc16->prot_en)) {
		logging(LOG_VERBOSE, "Device does not support/use protection information. All commands should fail.");
		for (i = 1; i < 8; i++) {
			ret = write10_invalidfieldincdb(iscsic, tgt_lun, 0,
						       block_size, block_size,
						       i, 0, 0, 0, 0, buf);
			if (ret == -2) {
				logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
				CU_PASS("WRITE10 is not implemented.");
				return;
			}	
			CU_ASSERT_EQUAL(ret, 0);
		}
		return;
	}

	logging(LOG_NORMAL, "No tests for devices that support protection information yet.");
}
