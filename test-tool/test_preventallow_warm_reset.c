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
test_preventallow_warm_reset(void)
{
	int ret;

	CHECK_FOR_SBC;
	CHECK_FOR_REMOVABLE;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test that Target Warm Reset clears PREVENT MEDIUM REMOVAL");

	logging(LOG_VERBOSE, "Set the PREVENT flag");
	ret = preventallow(iscsic, tgt_lun, 1);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Try to eject the medium");
	ret = startstopunit_preventremoval(iscsic, tgt_lun, 0, 0, 0, 0, 1, 0);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify we can still access the media.");
	ret = testunitready(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	
	logging(LOG_VERBOSE, "Perform warm reset on target");
	ret = iscsi_task_mgmt_target_warm_reset_sync(iscsic);
	CU_ASSERT_EQUAL(ret, 0);
	logging(LOG_VERBOSE, "Wait until all unit attentions clear");
	while (testunitready(iscsic, tgt_lun) != 0);


	logging(LOG_VERBOSE, "Try to eject the medium");
	ret = startstopunit(iscsic, tgt_lun, 0, 0, 0, 0, 1, 0);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify we can not access the media.");
	ret = testunitready_nomedium(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Load the medium");
	ret = startstopunit(iscsic, tgt_lun, 0, 0, 0, 0, 1, 0);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Clear PREVENT and load medium in case target failed");
	logging(LOG_VERBOSE, "Test we can clear PREVENT flag");
	ret = preventallow(iscsic, tgt_lun, 0);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Load the medium");
	ret = startstopunit(iscsic, tgt_lun, 0, 0, 0, 0, 1, 1);
	CU_ASSERT_EQUAL(ret, 0);

}
