/* 
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
test_readcapacity16_alloclen(void)
{
	int i, ret;


	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test that READCAPACITY16 with alloc_len 0-15 is not an error");

	for (i = 0; i < 16; i++) {
		ret = readcapacity16(iscsic, tgt_lun, i);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 is not implemented on this target and it does not claim SBC-3 support.");
			CU_PASS("READCAPACITY16 is not implemented and no SBC-3 support claimed.");
			return;
		}	
		CU_ASSERT_EQUAL(ret, 0);
	}
}
