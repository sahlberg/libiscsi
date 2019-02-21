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
#include "iscsi-multipath.h"

void
test_preventallow_2_itnexuses(void)
{
        int ret;
        struct scsi_device *sd2;

        CHECK_FOR_SBC;
        CHECK_FOR_REMOVABLE;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that PREVENT MEDIUM REMOVAL are seen on other nexuses as well");

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

        logging(LOG_VERBOSE, "Create a second connection to the target");
        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;

        logging(LOG_VERBOSE, "Try to eject the medium on the second connection");
        STARTSTOPUNIT(sd2, 0, 0, 0, 0, 1, 0,
                      EXPECT_REMOVAL_PREVENTED);

        logging(LOG_VERBOSE, "Logout the second connection from target");
        mpath_sd2_put(sd2);

        logging(LOG_VERBOSE, "Clear PREVENT and load medium in case target failed");
        logging(LOG_VERBOSE, "Test we can clear PREVENT flag");
        PREVENTALLOW(sd, 0);

        logging(LOG_VERBOSE, "Load the medium");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 1,
                      EXPECT_STATUS_GOOD);
}
