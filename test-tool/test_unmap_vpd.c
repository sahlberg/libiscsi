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
test_unmap_vpd(void)
{
        int ret;
        struct unmap_list list[1];

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test UNMAP availability is consistent with VPD settings");

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, "Check if UNMAP is available.");
        list[0].lba = 0;
        list[0].num = 0;
        ret = unmap(sd, 0, list, 1,
                    EXPECT_STATUS_GOOD);
        if (ret != 0) {
                logging(LOG_VERBOSE, "UNMAP is not available. Verify that VPD "
                        "settings reflect this.");

                logging(LOG_VERBOSE, "Verify that LBPU is clear.");
                if (inq_lbp && inq_lbp->lbpu) {
                        logging(LOG_NORMAL, "[FAILED] UNMAP is not implemented "
                                "but LBPU is set");
                        CU_FAIL("[FAILED] UNMAP is unavailable but LBPU==1");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] LBPU is clear.");
                }
        } else {
                logging(LOG_VERBOSE, "UNMAP is available. Verify that VPD "
                        "settings reflect this.");

                logging(LOG_VERBOSE, "Verify that LBPME is set.");
                if (rc16 && rc16->lbpme) {
                        logging(LOG_VERBOSE, "[SUCCESS] LBPME is set.");
                } else {
                        logging(LOG_NORMAL, "[FAILED] UNMAP is implemented "
                                "but LBPME is not set");
                        CU_FAIL("[FAILED] UNMAP is available but LBPME==0");
                }

                logging(LOG_VERBOSE, "Verify that LBPU is set.");
                if (inq_lbp && inq_lbp->lbpu) {
                        logging(LOG_VERBOSE, "[SUCCESS] LBPU is set.");
                } else {
                        logging(LOG_NORMAL, "[FAILED] UNMAP is implemented "
                                "but LBPU is not set");
                        CU_FAIL("[FAILED] UNMAP is available but LBPU==0");
                }
        }
}
