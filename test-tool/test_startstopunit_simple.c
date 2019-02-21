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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_startstopunit_simple(void)
{ 
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test basic STARTSTOPUNIT");


        logging(LOG_VERBOSE, "Test we can eject removable the media with IMMED==1");
        if (inq->rmb) {
                logging(LOG_VERBOSE, "Media is removable. STARTSTOPUNIT should work");
                ret = startstopunit(sd, 1, 0, 0, 0, 1, 0,
                                    EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
        } else {
                const char *err = "[SKIPPED] Media is not removable.";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;

        }


        logging(LOG_VERBOSE, "Test TESTUNITREADY that medium is ejected.");
        TESTUNITREADY(sd,
                      EXPECT_NO_MEDIUM);

        logging(LOG_VERBOSE, "Test we can load the removable the media with IMMED==1");
        STARTSTOPUNIT(sd, 1, 0, 0, 0, 1, 1,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify we can read from the media.");
        TESTUNITREADY(sd,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test we can eject removable the media with IMMED==1");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 0,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test TESTUNITREADY that medium is ejected.");
        TESTUNITREADY(sd,
                      EXPECT_NO_MEDIUM);


        logging(LOG_VERBOSE, "Test we can load the removable the media with IMMED==1");
        STARTSTOPUNIT(sd, 0, 0, 0, 0, 1, 1,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify we can access the media again.");
        TESTUNITREADY(sd,
                      EXPECT_STATUS_GOOD);
}
