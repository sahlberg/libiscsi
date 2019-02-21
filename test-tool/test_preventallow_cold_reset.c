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
test_preventallow_cold_reset(void)
{
        int ret;

        CHECK_FOR_SBC;
        CHECK_FOR_REMOVABLE;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that Target Warm Reset clears PREVENT MEDIUM REMOVAL");

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PREVENTALLOW test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, "Set the PREVENT flag");
        PREVENTALLOW(sd, 1);

        logging(LOG_VERBOSE, "Try to eject the medium");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 0,
                      EXPECT_REMOVAL_PREVENTED);

        logging(LOG_VERBOSE, "Verify we can still access the media.");
        TESTUNITREADY(sd,
                      EXPECT_STATUS_GOOD);
        
        logging(LOG_VERBOSE, "Perform cold reset on target");
        ret = iscsi_task_mgmt_target_cold_reset_sync(sd->iscsi_ctx);
        if (ret != 0) {
                const char *err = "[SKIPPED] Task Management function"
                        "for ColdReset is not working/implemented\n";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, "Wait until all unit attentions clear");
        while (testunitready(sd, EXPECT_STATUS_GOOD) != 0)
                ;

        logging(LOG_VERBOSE, "Try to eject the medium");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 0,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify we can not access the media.");
        TESTUNITREADY(sd,
                      EXPECT_NO_MEDIUM);

        logging(LOG_VERBOSE, "Load the medium");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 0,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Clear PREVENT and load medium in case target failed");
        logging(LOG_VERBOSE, "Test we can clear PREVENT flag");
        PREVENTALLOW(sd, 0);

        logging(LOG_VERBOSE, "Load the medium");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 1,
                      EXPECT_STATUS_GOOD);
}
