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

//printf("2, Verify we can read a descriptor at the end of the lun.\n");

void
test_get_lba_status_simple(void)
{
	int ret;
	struct unmap_list list[257];

	CHECK_FOR_DATALOSS;
	CHECK_FOR_THIN_PROVISIONING;
	CHECK_FOR_SBC;

	logging(LOG_VERBOSE, "");
	logging(LOG_VERBOSE, "Verify we can read a descriptor at the start of the lun.");
	ret = get_lba_status(iscsic, tgt_lun, 0,


#if 0
	logging(LOG_VERBOSE, "Test GET_LBA_STATUS for 1-255 blocks at the start of the LUN");
	for (i = 1; i <= 256; i++) {
		list[0].lba = 0;
		list[0].num = i;
		ret = unmap(iscsic, tgt_lun, 0, list, 1);
		CU_ASSERT_EQUAL(ret, 0);
	}
#endif
}
