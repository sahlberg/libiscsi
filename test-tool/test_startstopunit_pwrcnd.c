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
test_startstopunit_pwrcnd(void)
{ 
        int i;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test STARTSTOPUNIT PowerCondition");
        if (!inq->rmb) {
                logging(LOG_VERBOSE, "[SKIPPED] LUN is not removable. "
                        "Skipping test.");
                return;
        }

        logging(LOG_VERBOSE, "Test that media is not ejected when PC!=0");
        for (i = 1; i < 16; i++) {
                STARTSTOPUNIT(sd, 1, 0, i, 0, 1, 0,
                              EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Test TESTUNITREADY that medium is not ejected.");
                TESTUNITREADY(sd,
                              EXPECT_STATUS_GOOD);
        }

        logging(LOG_VERBOSE, "In case the target did eject the medium, load it again.");
        STARTSTOPUNIT(sd, 1, 0, 0, 0, 1, 1,
                      EXPECT_STATUS_GOOD);
}
