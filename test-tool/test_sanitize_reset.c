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

static void sanitize_cb(struct iscsi_context *iscsi _U_, int status _U_,
       void *command_data _U_, void *private_data _U_)
{
}

void
test_sanitize_reset(void)
{ 
	int ret;
	struct scsi_command_descriptor *cd;
	struct scsi_task *sanitize_task;
	struct scsi_task *rl_task;
	struct iscsi_data data;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test SANITIZE with Task/Lun/Target/Session reset");

	CHECK_FOR_SANITIZE;
	CHECK_FOR_DATALOSS;

	logging(LOG_VERBOSE, "Check that SANITIZE OVERWRITE will continue "
		"even after Task/Lun/Target/* reset.");
	cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
				    SCSI_SANITIZE_OVERWRITE);
	if (cd == NULL) {
		logging(LOG_NORMAL, "[SKIPPED] SANITIZE OVERWRITE is not "
			"implemented according to REPORT_SUPPORTED_OPCODES.");
		CU_PASS("SANITIZE is not implemented.");
		return;
	}

	logging(LOG_VERBOSE, "Send an asyncronous SANITIZE to the target.");
	data.size = block_size + 4;
	data.data = alloca(data.size);
	memset(&data.data[4], 0, block_size);

	data.data[0] = 0x01;
	data.data[1] = 0x00;
	data.data[2] = block_size >> 8;
	data.data[3] = block_size & 0xff;
	sanitize_task = iscsi_sanitize_task(iscsic, tgt_lun,
					    0, 0, SCSI_SANITIZE_OVERWRITE,
					    data.size, &data,
					    sanitize_cb, NULL);
	CU_ASSERT_NOT_EQUAL(sanitize_task, NULL);
	/* just send something so that we know the sanitize command is sent
	 * to the target
	 */
	rl_task = iscsi_reportluns_sync(iscsic, 0, 64);
	if (rl_task) {
		scsi_free_scsi_task(rl_task);
	}


	logging(LOG_VERBOSE, "Sleep for three seconds incase the target is "
		"slow to start the SANITIZE");
	sleep(3);

	logging(LOG_VERBOSE, "Verify that the SANITIZE has started and that "
		"TESTUNITREADY fails with SANITIZE_IN_PROGRESS");
	ret = testunitready_sanitize(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify that STARTSTOPUNIT fails with "
		"SANITIZE_IN_PROGRESS");
	ret = startstopunit_sanitize(iscsic, tgt_lun, 1, 0, 1, 0, 1, 0);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify that READ16 fails with "
		"SANITIZE_IN_PROGRESS");
	ret = read16_sanitize(iscsic, tgt_lun, 0, block_size,
			block_size, 0, 0, 0, 0, 0, NULL);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify that INQUIRY is still allowed while "
		"SANITIZE is in progress");
	ret = inquiry(iscsic, tgt_lun, 0, 0, 255, NULL);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Send an ABORT TASK");
	ret = iscsi_task_mgmt_abort_task_sync(iscsic, sanitize_task);
	if (ret != 0) {
		logging(LOG_NORMAL, "ABORT TASK failed. %s",
			iscsi_get_error(iscsic));
	}

	logging(LOG_VERBOSE, "Send an ABORT TASK SET");
	ret = iscsi_task_mgmt_abort_task_set_sync(iscsic, tgt_lun);
	if (ret != 0) {
		logging(LOG_NORMAL, "ABORT TASK SET failed. %s",
			iscsi_get_error(iscsic));
	}

	logging(LOG_VERBOSE, "Send a LUN Reset");
	ret = iscsi_task_mgmt_lun_reset_sync(iscsic, tgt_lun);
	if (ret != 0) {
		logging(LOG_NORMAL, "LUN reset failed. %s", iscsi_get_error(iscsic));
	}

	logging(LOG_VERBOSE, "Send a Warm Reset");
	ret = iscsi_task_mgmt_target_warm_reset_sync(iscsic);
	if (ret != 0) {
		logging(LOG_NORMAL, "Warm reset failed. %s", iscsi_get_error(iscsic));
	}

	logging(LOG_VERBOSE, "Send a Cold Reset");
	ret = iscsi_task_mgmt_target_cold_reset_sync(iscsic);
	if (ret != 0) {
		logging(LOG_NORMAL, "Cold reset failed. %s", iscsi_get_error(iscsic));
	}

	logging(LOG_VERBOSE, "Disconnect from the target.");
	iscsi_destroy_context(iscsic);

	logging(LOG_VERBOSE, "Sleep for one seconds incase the target is "
		"slow to reset");
	sleep(1);

	logging(LOG_VERBOSE, "Reconnect to target");
	iscsic = iscsi_context_login(initiatorname1, tgt_url, &tgt_lun);
	if (iscsic == NULL) {
		logging(LOG_VERBOSE, "Failed to login to target");
		return;
	}

	logging(LOG_VERBOSE, "Verify that the SANITIZE is still going.");
	ret = testunitready_sanitize(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Wait until the SANITIZE operation has finished");
	while (testunitready_clear_ua(iscsic, tgt_lun)) {
		sleep(60);
	}
}
