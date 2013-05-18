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
test_report_supported_opcodes_rctd(void)
{
	int i, ret;
	struct scsi_task *rso_task;
	struct scsi_report_supported_op_codes *rsoc;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test READ_SUPPORTED_OPCODES RCTD flag");


	logging(LOG_VERBOSE, "Test READ_SUPPORTED_OPCODES report ALL opcodes "
		"without timeout descriptors");
	ret = report_supported_opcodes(iscsic, tgt_lun,
		0, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
		65535, &rso_task);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] READ_SUPPORTED_OPCODES is not "
			"implemented.");
		CU_PASS("READ_SUPPORTED_OPCODES is not implemented.");
		return;
	}
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0) {
		return;
	}
	
	logging(LOG_VERBOSE, "Unmarshall the DATA-IN buffer");
	rsoc = scsi_datain_unmarshall(rso_task);
	CU_ASSERT_NOT_EQUAL(rsoc, NULL);


	logging(LOG_VERBOSE, "Verify that all returned command descriptors "
		"lack timeout description");
	for (i = 0; i < rsoc->num_descriptors; i++) {
		if (rsoc->descriptors[i].ctdp) {
			logging(LOG_NORMAL, "[FAILED] Command descriptor with "
				"CTDP set");
			CU_FAIL("[FAILED] Command descriptor with "
				"CTDP set");
		}
	}

	scsi_free_scsi_task(rso_task);


	logging(LOG_VERBOSE, "Test READ_SUPPORTED_OPCODES report ALL opcodes "
		"with timeout descriptors");
	ret = report_supported_opcodes(iscsic, tgt_lun,
		1, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
		65535, &rso_task);
	CU_ASSERT_EQUAL(ret, 0);
	if (ret != 0) {
		return;
	}
	
	logging(LOG_VERBOSE, "Unmarshall the DATA-IN buffer");
	rsoc = scsi_datain_unmarshall(rso_task);
	CU_ASSERT_NOT_EQUAL(rsoc, NULL);

	logging(LOG_VERBOSE, "Verify that all returned command descriptors "
		"have a timeout description");
	for (i = 0; i < rsoc->num_descriptors; i++) {
		if (!rsoc->descriptors[i].ctdp) {
			logging(LOG_NORMAL, "[FAILED] Command descriptor "
				"without CTDP set");
			CU_FAIL("[FAILED] Command descriptor without "
				"CTDP set");
		}
	}

	logging(LOG_VERBOSE, "Verify that all timeout descriptors have the "
		"correct length");
	for (i = 0; i < rsoc->num_descriptors; i++) {
		if (rsoc->descriptors[i].to.descriptor_length != 0x0a) {
			logging(LOG_NORMAL, "[FAILED] Command descriptor "
				"with invalid TimeoutDescriptor length");
			CU_FAIL("[FAILED] Command descriptor with "
				"invalid TimeoutDescriptor length");
		}
	}

	scsi_free_scsi_task(rso_task);
}
