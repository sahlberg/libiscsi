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
#include <alloca.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_sanitize_overwrite(void)
{ 
	int ret;
	struct iscsi_data data;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE");

	CHECK_FOR_SANITIZE;


	logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with initialization pattern of one full block");
	data.size = block_size + 4;
	data.data = alloca(data.size);
	memset(&data.data[4], 0xaa, block_size);

	data.data[0] = 0x01;
	data.data[1] = 0x00;
	data.data[2] = block_size >> 8;
	data.data[3] = block_size & 0xff;
	ret = sanitize(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with initialization pattern of one half block");
	data.size = block_size / 2 + 4;

	data.data[2] = (block_size / 2) >> 8;
	data.data[3] = (block_size / 2 ) & 0xff;

	ret = sanitize(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with initialization pattern of 4 bytes");
	data.size = 4 + 4;

	data.data[2] = 0;
	data.data[3] = 4;

	ret = sanitize(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data);
	CU_ASSERT_EQUAL(ret, 0);
}
