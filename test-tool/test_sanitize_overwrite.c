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

static void
init_lun_with_data(uint64_t lba)
{
	int ret;
	unsigned char *buf = alloca(256 * block_size);

	memset(buf, 'a', 256 * block_size);
	ret = write16(iscsic, tgt_lun, lba, 256 * block_size,
		    block_size, 0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);
}

static void
check_lun_is_wiped(uint64_t lba, char c)
{
	int ret;
	unsigned char *rbuf = alloca(256 * block_size);
	unsigned char *zbuf = alloca(256 * block_size);

	ret = read16(iscsic, tgt_lun, lba, 256 * block_size,
		    block_size, 0, 0, 0, 0, 0, rbuf);
	CU_ASSERT_EQUAL(ret, 0);

	memset(zbuf, c, 256 * block_size);

	if (memcmp(zbuf, rbuf, 256 * block_size)) {
		logging(LOG_NORMAL, "[FAILED] Blocks did not "
			"read back as zero");
		CU_FAIL("[FAILED] Blocks did not read back "
			"as zero");
	} else {
		logging(LOG_VERBOSE, "[SUCCESS] Blocks read "
			"back as zero");
	}
}

void
test_sanitize_overwrite(void)
{ 
	int i, ret;
	struct iscsi_data data;
	struct scsi_command_descriptor *cd;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE");

	CHECK_FOR_SANITIZE;
	CHECK_FOR_DATALOSS;

	logging(LOG_VERBOSE, "Check that SANITIZE OVERWRITE is supported "
		"in REPORT_SUPPORTED_OPCODES");
	cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
				    SCSI_SANITIZE_OVERWRITE);
	if (cd == NULL) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE OVERWRITE is not "
			"implemented according to REPORT_SUPPORTED_OPCODES.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	}

	logging(LOG_VERBOSE, "Verify that we have BlockDeviceCharacteristics "
		"VPD page.");
	if (inq_bdc == NULL) {
		logging(LOG_NORMAL, "[FAILED] SANITIZE OVERWRITE opcode is "
			"supported but BlockDeviceCharacteristics VPD page is "
			"missing");
		CU_FAIL("[FAILED] BlockDeviceCharacteristics VPD "
			"page is missing");
	}

	logging(LOG_VERBOSE, "Check MediumRotationRate whether this is a HDD "
		"or a SSD device.");
	if (inq_bdc && inq_bdc->medium_rotation_rate == 0) {
		logging(LOG_NORMAL, "This is a HDD device");
		logging(LOG_NORMAL, "[WARNING] SANITIZE OVERWRITE opcode is "
			"supported but MediumRotationRate is 0 "
			"indicating that this is an SSD. Only HDDs should "
			"implement OVERWRITE");
	} else {
		logging(LOG_NORMAL, "This is a SSD device");
	}

	logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
	init_lun_with_data(0);
	logging(LOG_VERBOSE, "Write 'a' to the last 256 LBAs");
	init_lun_with_data(num_blocks - 256);

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

	logging(LOG_VERBOSE, "Check that the first 256 LBAs are wiped.");
	check_lun_is_wiped(0, 0xaa);
	logging(LOG_VERBOSE, "Check that the last 256 LBAs are wiped.");
	check_lun_is_wiped(num_blocks - 256, 0xaa);


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

	logging(LOG_VERBOSE, "OVERWRITE parameter list length must "
			"be > 4 and < blocksize+5");
	for (i = 0; i < 5; i++) {
		logging(LOG_VERBOSE, "Test OVERWRITE with ParamLen:%d is an "
			"error.", i);

		ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
			       0, 0, SCSI_SANITIZE_OVERWRITE, i, &data);
		if (ret == -2) {
			logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not "
				"implemented.");
			CU_PASS("SANITIZE is not implemented.");
			return;
		} else {
			CU_ASSERT_EQUAL(ret, 0);
		}
	}


	logging(LOG_VERBOSE, "Test OVERWRITE with ParamLen:%zd (blocksize+5) "
		"is an error.", block_size + 5);

	data.size = block_size + 8;
	data.data = alloca(block_size + 8); /* so we can send IP > blocksize */
	memset(data.data, 0, data.size);
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, block_size + 5, &data);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not "
			"implemented.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "Test OVERWRITE COUNT == 0 is an error");
	data.size = block_size + 4;

	data.data[0] = 0x00;
	data.data[1] = 0x00;
	data.data[2] = block_size >> 8;
	data.data[3] = block_size & 0xff;
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test INITIALIZATION PATTERN LENGTH == 0 is an "
		"error");
	data.size = block_size + 4;

	data.data[0] = 0x00;
	data.data[1] = 0x00;
	data.data[2] = 0x00;
	data.data[3] = 0x00;
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test INITIALIZATION PATTERN LENGTH == %zd  > %zd "
		"(blocksize) is an error", block_size + 4, block_size);

	data.size = block_size + 4;

	data.data[0] = 0x00;
	data.data[1] = 0x00;
	data.data[2] = (block_size + 4) >> 8;
	data.data[3] = (block_size + 4) & 0xff;
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data);
	CU_ASSERT_EQUAL(ret, 0);
}
