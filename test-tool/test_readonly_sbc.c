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
test_readonly_sbc(void)
{
	int ret;
	unsigned char buf[4096];
	struct unmap_list list[1];

	CHECK_FOR_DATALOSS;
	CHECK_FOR_READONLY;
	CHECK_FOR_SBC;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test that Medium write commands fail for READ-ONLY SBC devices");


	logging(LOG_VERBOSE, "Test WRITE10 fails with WRITE_PROTECTED");
	ret = write10_writeprotected(iscsic, tgt_lun, 0, block_size, block_size,
				     0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test WRITE12 fails with WRITE_PROTECTED");
	ret = write12_writeprotected(iscsic, tgt_lun, 0, block_size, block_size,
				     0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test WRITE16 fails with WRITE_PROTECTED");
	ret = write16_writeprotected(iscsic, tgt_lun, 0, block_size, block_size,
				     0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test WRITE_SAME10 fails with WRITE_PROTECTED");
	ret = writesame10_writeprotected(iscsic, tgt_lun, 0, block_size, 1,
					 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITE_SAME10 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test WRITE_SAME16 fails with WRITE_PROTECTED");
	ret = writesame16_writeprotected(iscsic, tgt_lun, 0, block_size, 1,
					 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITE_SAME16 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test WRITE_SAME10 UNMAP fails with WRITE_PROTECTED");
	ret = writesame10_writeprotected(iscsic, tgt_lun, 0,
					 block_size, 1,
					 0, 1, 0, 0, NULL);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITE_SAME10 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test WRITE_SAME16 UNMAP fails with WRITE_PROTECTED");
	ret = writesame16_writeprotected(iscsic, tgt_lun, 0,
					 block_size, 1,
					 0, 1, 0, 0, NULL);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITE_SAME16 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test UNMAP of one physical block fails with WRITE_PROTECTED");
	list[0].lba = 0;
	list[0].num = lbppb;
	ret = unmap_writeprotected(iscsic, tgt_lun, 0, list, 1);
	if (ret == -2) {
		logging(LOG_VERBOSE, "UNMAP not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test UNMAP of one logical block fails with WRITE_PROTECTED");
	list[0].lba = 0;
	list[0].num = 1;
	ret = unmap_writeprotected(iscsic, tgt_lun, 0, list, 1);
	if (ret == -2) {
		logging(LOG_VERBOSE, "UNMAP not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test WRITEVERIFY10 fails with WRITE_PROTECTED");
	ret = writeverify10_writeprotected(iscsic, tgt_lun, 0,
					   block_size, block_size,
					   0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITEVERIFY10 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test WRITEVERIFY12 fails with WRITE_PROTECTED");
	ret = writeverify12_writeprotected(iscsic, tgt_lun, 0,
					   block_size, block_size,
					   0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITEVERIFY12 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test WRITEVERIFY16 fails with WRITE_PROTECTED");
	ret = writeverify16_writeprotected(iscsic, tgt_lun, 0,
					   block_size, block_size,
					   0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_VERBOSE, "WRITEVERIFY16 not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	logging(LOG_VERBOSE, "Test ORWRITE fails with WRITE_PROTECTED");
	ret = orwrite_writeprotected(iscsic, tgt_lun, 0,
				     block_size, block_size,
				     0, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_VERBOSE, "ORWRITE not supported on target. Skipped.");
	}
	CU_ASSERT_NOT_EQUAL(ret, -1);

	/* NOT implemented yet */
	logging(LOG_VERBOSE, "Test for COMPAREANDWRITE not implemented yet.");
}
