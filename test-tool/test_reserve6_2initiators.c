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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_reserve6_2initiators(void)
{
	int ret;
	struct scsi_device sd2;
	memset(&sd2, 0, sizeof(sd2));
	sd2.iscsi_url = sd->iscsi_url;
	sd2.iscsi_lun = sd->iscsi_lun;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test RESERVE6/RELEASE6 across two initiators");

	if (sd->iscsi_ctx == NULL) {
		const char *err = "[SKIPPED] This RESERVE6 test is only "
			"supported for iSCSI backends";
		logging(LOG_NORMAL, "%s", err);
		CU_PASS(err);
		return;
	}

	logging(LOG_NORMAL, "Take out a RESERVE6 from the first initiator");
	ret = reserve6(sd);
	if (ret == -2) {
		logging(LOG_VERBOSE, "[SKIPPED] Target does not support RESERVE6. Skipping test");
		CU_PASS("[SKIPPED] Target does not support RESERVE6. Skipping test");
		return;
	}
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_NORMAL, "Verify that the first initiator can re-RESERVE6 the same reservation");
	ret = reserve6(sd);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Create a second connection to the target");
	sd2.iscsi_ctx = iscsi_context_login(initiatorname2, sd2.iscsi_url, &sd2.iscsi_lun);
	if (sd2.iscsi_ctx == NULL) {
		logging(LOG_VERBOSE, "Failed to login to target");
		return;
	}

	logging(LOG_NORMAL, "Try to take out a RESERVE6 from the second initiator");
	ret = reserve6_conflict(&sd2);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_NORMAL, "Try to RELEASE from the second initiator. Should be a nop");
	ret = release6(&sd2);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_NORMAL, "Test we can still send MODE SENSE from the first initiator");
	ret = modesense6(sd, NULL, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
			 EXPECT_STATUS_GOOD);	
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_NORMAL, "MODE SENSE should fail from the second initiator");
	ret = modesense6(&sd2, NULL, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
			 EXPECT_RESERVATION_CONFLICT);


	logging(LOG_NORMAL, "RESERVE6 from the second initiator should still fail");
	ret = reserve6_conflict(&sd2);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_NORMAL, "RELEASE6 from the first initiator");
	ret = release6(sd);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_NORMAL, "RESERVE6 from the second initiator should work now");
	ret = reserve6(&sd2);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_NORMAL, "RELEASE6 from the second initiator");
	ret = release6(&sd2);
	CU_ASSERT_EQUAL(ret, 0);

	iscsi_logout_sync(sd2.iscsi_ctx);
	iscsi_destroy_context(sd2.iscsi_ctx);
}
