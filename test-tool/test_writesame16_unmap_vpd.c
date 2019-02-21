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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_writesame16_unmap_vpd(void)
{
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITESAME16 UNMAP availability is "
                "consistent with VPD settings");

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, "Check if WRITESAME16 can be used for UNMAP.");

        logging(LOG_VERBOSE, "Unmap 1 block using WRITESAME16");
        memset(scratch, 0, block_size);
        ret = writesame16(sd, 0, block_size, 1, 0, 1, 0, 0, scratch,
                          EXPECT_STATUS_GOOD);
        if (ret != 0) {
                logging(LOG_VERBOSE, "WRITESAME16 UNMAP is not available. "
                        "Verify that VPD settings reflect this.");

                logging(LOG_VERBOSE, "Verify that LBPWS is clear.");
                if (inq_lbp && inq_lbp->lbpws) {
                        logging(LOG_NORMAL, "[FAILED] WRITESAME16 UNMAP is not "
                                "implemented but LBPWS is set");
                        CU_FAIL("[FAILED] WRITESAME16 UNMAP is unavailable but "
                                "LBPWS==1");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] LBPWS is clear.");
                }
        } else {
                logging(LOG_VERBOSE, "WRITESAME16 UNMAP is available. Verify "
                        "that VPD settings reflect this.");

                logging(LOG_VERBOSE, "Verify that LBPME is set.");
                if (rc16 && rc16->lbpme) {
                        logging(LOG_VERBOSE, "[SUCCESS] LBPME is set.");
                } else {
                        logging(LOG_NORMAL, "[FAILED] WRITESAME16 UNMAP is "
                                "implemented but LBPME is not set");
                        CU_FAIL("[FAILED] UNMAP is available but LBPME==0");
                }

                logging(LOG_VERBOSE, "Verify that LBPWS is set.");
                if (inq_lbp && inq_lbp->lbpws) {
                        logging(LOG_VERBOSE, "[SUCCESS] LBPWS is set.");
                } else {
                        logging(LOG_NORMAL, "[FAILED] WRITESAME16 UNMAP is "
                                "implemented but LBPWS is not set");
                        CU_FAIL("[FAILED] UNMAP is available but LBPWS==0");
                }
        }
}
