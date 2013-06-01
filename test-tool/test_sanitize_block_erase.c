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
test_sanitize_block_erase(void)
{ 
	int ret;
	struct iscsi_data data;
	struct scsi_command_descriptor *cd;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test SANITIZE BLOCK ERASE");

	CHECK_FOR_SANITIZE;

	logging(LOG_NORMAL, "Check that SANITIZE BLOCK_ERASE is supported "
		"in REPORT_SUPPORTED_OPCODES");
	cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
				    SCSI_SANITIZE_BLOCK_ERASE);
	if (cd == NULL) {
		if (inq_bdc && inq_bdc->wabereq) {
			logging(LOG_NORMAL, "[WARNING] SANITIZE BLOCK ERASE "
				"opcode is not supproted but WABEREQ is "
				"not 0");
		}
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE BLOCK_ERASE is not "
			"implemented according to REPORT_SUPPORTED_OPCODES.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	}
	if (inq_bdc == NULL) {
		logging(LOG_NORMAL, "[WARNING] SANITIZE BLOCK ERASE opcode is "
			"supported but BlockDeviceCharacteristics VPD page is "
			"missing");
	}
	if (inq_bdc && inq_bdc->medium_rotation_rate != 0) {
		logging(LOG_NORMAL, "[WARNING] SANITIZE BLOCK ERASE opcode is "
			"supported but MediumRotationRate is not 0 "
			"indicating that this is a HDD. Only SSDs should "
			"implement BLOCK ERASE");
	}
	if (inq_bdc && !inq_bdc->wabereq) {
		logging(LOG_NORMAL, "[WARNING] SANITIZE BLOCK ERASE "
			"opcode is supproted but WABEREQ is 0");
	}

	logging(LOG_VERBOSE, "Test we can perform basic BLOCK ERASE SANITIZE");

	ret = sanitize(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_BLOCK_ERASE, 0, NULL);
	CU_ASSERT_EQUAL(ret, 0);

	data.size = 8;
	data.data = alloca(data.size);
	memset(data.data, 0, data.size);

	logging(LOG_VERBOSE, "BLOCK_ERASE parameter list length must be 0");
	logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
		"BLOCK ERASE");
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_BLOCK_ERASE, 8, &data);
	CU_ASSERT_EQUAL(ret, 0);
}
