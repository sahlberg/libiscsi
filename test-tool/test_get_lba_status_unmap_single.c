/* 
   Copyright (C) 2014 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#include <inttypes.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_get_lba_status_unmap_single(void)
{
	int ret;
	uint64_t i;
	unsigned char *buf = alloca(257 * block_size);
	struct unmap_list list[1];
	enum scsi_provisioning_type provisioning;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_THIN_PROVISIONING;
	CHECK_FOR_LBPU;

	memset(buf, 'A', 257 * block_size);

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test GET_LBA_STATUS for a single unmapped block "
		"at offset 0-255");
	logging(LOG_VERBOSE, "We have %d logical blocks per physical block",
		lbppb);

	logging(LOG_VERBOSE, "Write the first %i blocks with a known "
		"pattern and thus map the blocks", 256 + lbppb);
	ret = write10(iscsic, tgt_lun, 0, (256 + lbppb) * block_size,
		      block_size, 0, 0, 0, 0, 0, buf,
		      EXPECT_STATUS_GOOD);
	CU_ASSERT_EQUAL(ret, 0);

	for (i = 0; i + lbppb <= 256; i += lbppb) {
		logging(LOG_VERBOSE, "Unmap a single physical block at LBA:%"
			PRIu64 " (number of logical blocks: %d)", i, lbppb);
		list[0].lba = i;
		list[0].num = lbppb;
		ret = unmap(iscsic, tgt_lun, 0, list, 1);
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read the status of the block at LBA:%"
			PRIu64, i);
		ret = get_lba_status(iscsic, tgt_lun, i, 24, NULL);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support GET_LBA_STATUS. Skipping test");
			return;
		}
		if (ret != 0) {
			CU_FAIL("[FAILED] GET_LBA_STATUS command failed");
			return;
		}
		logging(LOG_VERBOSE, "Read the status of the block at LBA:%"
			PRIu64, i + lbppb);
		ret = get_lba_status(iscsic, tgt_lun, i + lbppb, 24, &provisioning);
		if (ret != 0) {
			CU_FAIL("[FAILED] GET_LBA_STATUS command failed");
			return;
		}
		if (provisioning != SCSI_PROVISIONING_TYPE_MAPPED) {
			CU_FAIL("[FAILED] LBA should be mapped but isn't");
			return;
		}
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test GET_LBA_STATUS for a single range of 1-255 "
		"blocks at offset 0");
	for (i = lbppb; i + lbppb <= 256; i += lbppb) {
		logging(LOG_VERBOSE, "Write the first %i blocks with a known "
			"pattern and thus map the blocks", (256 + lbppb));
		ret = write10(iscsic, tgt_lun, 0, (256 + lbppb) * block_size,
			      block_size, 0, 0, 0, 0, 0, buf,
			      EXPECT_STATUS_GOOD);

		logging(LOG_VERBOSE, "Unmap %" PRIu64 " blocks at LBA 0", i);
		list[0].lba = 0;
		list[0].num = i;
		ret = unmap(iscsic, tgt_lun, 0, list, 1);
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read the status of the block at LBA:0");

		ret = get_lba_status(iscsic, tgt_lun, 0, 24, NULL);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support GET_LBA_STATUS. Skipping test");
			return;
		}
		if (ret != 0) {
			CU_FAIL("[FAILED] GET_LBA_STATUS command failed");
			return;
		}
		logging(LOG_VERBOSE, "Read the status of the block at LBA:%" PRIu64, i + 1);
		ret = get_lba_status(iscsic, tgt_lun, i + 1, 24, &provisioning);
		if (ret != 0) {
			CU_FAIL("[FAILED] GET_LBA_STATUS command failed");
			return;
		}
		if (provisioning != SCSI_PROVISIONING_TYPE_MAPPED) {
			CU_FAIL("[FAILED] LBA should be mapped but isn't");
			return;
		}
	}
}
