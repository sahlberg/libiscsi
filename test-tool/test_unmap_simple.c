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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

static void
init_lun_with_data(unsigned char *buf, uint64_t lba)
{
	int ret;

	memset(buf, 'a', 256 * block_size);
	ret = write10(iscsic, tgt_lun, lba, 256 * block_size,
		    block_size, 0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);
}

void
test_unmap_simple(void)
{
	int i, ret;
	struct unmap_list list[257];
	unsigned char *buf = alloca(256 * block_size);
	unsigned char *zbuf = alloca(256 * block_size);

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test basic UNMAP");

	CHECK_FOR_DATALOSS;
	CHECK_FOR_THIN_PROVISIONING;
	CHECK_FOR_SBC;


	logging(LOG_VERBOSE, "Test UNMAP of 1-256 blocks at the start of the "
		"LUN as a single descriptor");

	logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
	init_lun_with_data(buf, 0);

	for (i = 1; i <= 256; i++) {
		logging(LOG_VERBOSE, "UNMAP blocks 0-%d", i);
		list[0].lba = 0;
		list[0].num = i;
		ret = unmap(iscsic, tgt_lun, 0, list, 1);
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read blocks 0-%d", i);
		ret = read10(iscsic, tgt_lun, 0, i * block_size,
		    block_size, 0, 0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);

		if (rc16 && rc16->lbprz) {
			logging(LOG_VERBOSE, "LBPRZ==1 All UNMAPPED blocks "
				"should read back as 0");
			if (memcmp(buf, zbuf, i * block_size)) {
				logging(LOG_NORMAL, "[FAILED] Blocks did not "
					"read back as zero");
				CU_FAIL("[FAILED] Blocks did not read back "
					"as zero");
			} else {
				logging(LOG_VERBOSE, "[SUCCESS] Blocks read "
					"back as zero");
			}
		}
	}

	logging(LOG_VERBOSE, "Test UNMAP of 1-256 blocks at the start of the "
		"LUN with one descriptor per block");

	logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
	init_lun_with_data(buf, 0);

	CU_ASSERT_EQUAL(ret, 0);
	for (i = 0; i < 256; i++) {
		list[i].lba = i;
		list[i].num = 1;
		ret = unmap(iscsic, tgt_lun, 0, list, i + 1);
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read blocks 0-%d", i);
		ret = read10(iscsic, tgt_lun, 0, i * block_size,
		    block_size, 0, 0, 0, 0, 0, buf);
		CU_ASSERT_EQUAL(ret, 0);

		if (rc16 && rc16->lbprz) {
			logging(LOG_VERBOSE, "LBPRZ==1 All UNMAPPED blocks "
				"should read back as 0");
			if (memcmp(buf, zbuf, i * block_size)) {
				logging(LOG_NORMAL, "[FAILED] Blocks did not "
					"read back as zero");
				CU_FAIL("[FAILED] Blocks did not read back "
					"as zero");
			} else {
				logging(LOG_VERBOSE, "[SUCCESS] Blocks read "
					"back as zero");
			}
		}
	}
}
