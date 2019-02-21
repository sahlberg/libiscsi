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
test_reserve6_target_cold_reset(void)
{
        int ret;
        struct scsi_device *sd2;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that RESERVE6 is released on target cold reset");

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This RESERVE6 test is only "
                        "supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, "Take out a RESERVE6 from the first initiator");
        RESERVE6(sd);

        logging(LOG_VERBOSE, "Send a Cold Reset to the target");
        ret = iscsi_task_mgmt_target_cold_reset_sync(sd->iscsi_ctx);
        if (ret != 0) {
                const char *err = "[SKIPPED] Task Management function"
                        "for ColdReset is not working/implemented\n";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, "Sleep for three seconds in case the target is slow to reset");
        sleep(3);

        logging(LOG_VERBOSE, "Create a second connection to the target");
        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;

        logging(LOG_VERBOSE, "RESERVE6 from the second initiator should work now");
        RESERVE6(sd2);

        logging(LOG_VERBOSE, "RELEASE6 from the second initiator");
        RELEASE6(sd2);

        mpath_sd2_put(sd2);
}
