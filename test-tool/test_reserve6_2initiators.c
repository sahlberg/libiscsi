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
#include "iscsi-multipath.h"

void
test_reserve6_2initiators(void)
{
        int ret;
        struct scsi_device *sd2;

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
        RESERVE6(sd);

        logging(LOG_NORMAL, "Verify that the first initiator can re-RESERVE6 the same reservation");
        RESERVE6(sd);

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;

        logging(LOG_NORMAL, "Try to take out a RESERVE6 from the second initiator");
        ret = reserve6_conflict(sd2);
        CU_ASSERT_EQUAL(ret, 0);


        logging(LOG_NORMAL, "Try to RELEASE from the second initiator. Should be a nop");
        RELEASE6(sd2);

        logging(LOG_NORMAL, "Test we can still send MODE SENSE from the first initiator");
        MODESENSE6(sd, NULL, 0, SCSI_MODESENSE_PC_CURRENT,
                   SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
                   EXPECT_STATUS_GOOD);        

        logging(LOG_NORMAL, "MODE SENSE should fail from the second initiator");
        MODESENSE6(sd2, NULL, 0, SCSI_MODESENSE_PC_CURRENT,
                   SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
                   EXPECT_RESERVATION_CONFLICT);

        logging(LOG_NORMAL, "RESERVE6 from the second initiator should still fail");
        ret = reserve6_conflict(sd2);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_NORMAL, "RELEASE6 from the first initiator");
        RELEASE6(sd);

        logging(LOG_NORMAL, "RESERVE6 from the second initiator should work now");
        RESERVE6(sd2);

        logging(LOG_NORMAL, "RELEASE6 from the second initiator");
        RELEASE6(sd2);

        mpath_sd2_put(sd2);
}
