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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_write16_wrprotect(void)
{
	int i, ret;
	unsigned char *buf;

	if (!data_loss) {
		CU_PASS("[SKIPPED] --dataloss flag is not set. Skipping test.");
		return;	
	}

	if (device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		CU_PASS("[SKIPPED] LUN is not SBC device. Skipping test");
		return;
	}

	/*
	 * Try out different non-zero values for WRPROTECT.
	 * They should all fail.
	 */
	logging(LOG_VERBOSE, "");
	logging(LOG_VERBOSE, "Test WRITE16 with non-zero WRPROTECT");
	buf = malloc(block_size);
	for (i = 1; i < 8; i++) {
		ret = write16_invalidfieldincdb(iscsic, tgt_lun, 0,
					       block_size, block_size,
					       i, 0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);
	}
	free(buf);
}
