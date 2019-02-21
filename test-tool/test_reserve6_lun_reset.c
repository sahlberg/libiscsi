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
        struct scsi_device  sd2;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that RESERVE6 is released on lun reset");

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This RESERVE6 test is only "
                        "supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, "Take out a RESERVE6 from the first initiator");
        RESERVE6(sd);

        logging(LOG_VERBOSE, "Send a LUN Reset");
        ret = iscsi_task_mgmt_lun_reset_sync(sd->iscsi_ctx, sd->iscsi_lun);
        if (ret != 0) {
                logging(LOG_NORMAL, "LUN reset failed. %s", iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Sleep for three seconds in case the target is slow to reset");
        sleep(3);


        logging(LOG_VERBOSE, "Create a second connection to the target");
        memset(&sd2, 0, sizeof(sd2));
        sd2.iscsi_url = sd->iscsi_url;
        sd2.iscsi_lun = sd->iscsi_lun;
        sd2.iscsi_ctx = iscsi_context_login(initiatorname2, sd2.iscsi_url, &sd2.iscsi_lun);
        if (sd2.iscsi_ctx == NULL) {
                logging(LOG_VERBOSE, "Failed to login to target");
                return;
        }

        logging(LOG_VERBOSE, "RESERVE6 from the second initiator should work now");
        RESERVE6(&sd2);

        logging(LOG_VERBOSE, "RELEASE6 from the second initiator");
        RELEASE6(&sd2);

        iscsi_logout_sync(sd2.iscsi_ctx);
        iscsi_destroy_context(sd2.iscsi_ctx);
}
