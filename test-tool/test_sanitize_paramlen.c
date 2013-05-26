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
test_sanitize_paramlen(void)
{ 
	int i, ret;
	struct iscsi_data data;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test SANITIZE ParameterListLength");

	CHECK_FOR_SANITIZE;

	data.size = 8;
	data.data = alloca(data.size);
	memset(data.data, 0, data.size);


	logging(LOG_VERBOSE, "BLOCK_ERASE parameter list length must be 0");
	logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
		"BLOCK ERASE");
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_BLOCK_ERASE, 8, &data);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not implemented.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}


	logging(LOG_VERBOSE, "CRYPTO_ERASE parameter list length must be 0");
	logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
		"CRYPTO ERASE");
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_CRYPTO_ERASE, 8, &data);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not implemented.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "EXIT_FAILURE_MODE parameter list length must "
		"be 0");
	logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
		"EXIT_FAILURE_MODE");
	ret = sanitize_invalidfieldincdb(iscsic, tgt_lun,
		       0, 0, SCSI_SANITIZE_EXIT_FAILURE_MODE, 8, &data);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE is not implemented.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}


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


	logging(LOG_VERBOSE, "Test OVERWRITE with ParamLen:%d is an "
			"error.", i);

	data.size = block_size + 8;
	data.data = alloca(data.size);
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
}
