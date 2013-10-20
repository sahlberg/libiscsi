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
test_reserve6_lun_reset(void)
{
	int ret;


	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test that RESERVE6 is released on lun reset");


	logging(LOG_VERBOSE, "Take out a RESERVE6 from the first initiator");
	ret = reserve6(iscsic, tgt_lun);
	if (ret == -2) {
		logging(LOG_VERBOSE, "[SKIPPED] Target does not support RESERVE6. Skipping test");
		CU_PASS("[SKIPPED] Target does not support RESERVE6. Skipping test");
		return;
	}
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Send a LUN Reset");
	ret = iscsi_task_mgmt_lun_reset_sync(iscsic, tgt_lun);
	if (ret != 0) {
		logging(LOG_NORMAL, "LUN reset failed. %s", iscsi_get_error(iscsic));
	}
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Sleep for three seconds incase the target is slow to reset");
	sleep(3);


	logging(LOG_VERBOSE, "Create a second connection to the target");
	iscsic2 = iscsi_context_login(initiatorname2, tgt_url, &tgt_lun);
	if (iscsic2 == NULL) {
		logging(LOG_VERBOSE, "Failed to login to target");
		return;
	}

	logging(LOG_VERBOSE, "RESERVE6 from the second initiator should work now");
	ret = reserve6(iscsic2, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "RELEASE6 from the second initiator");
	ret = release6(iscsic2, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	iscsi_logout_sync(iscsic2);
	iscsi_destroy_context(iscsic2);
	iscsic2 = NULL;
}
