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
test_readcapacity16_protection(void)
{
        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test READCAPACITY16 Protection Information");

        CHECK_FOR_SBC;

        if (rc16 == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 is not implemented on this target.");
                CU_PASS("READCAPACITY16 is not implemented.");
                return;
        }

        if (!inq->protect) {
                logging(LOG_VERBOSE, "This device does not support PI. "
                        "Verify that all relevant fields in READCAPACITY16 "
                        "are 0");

                logging(LOG_VERBOSE, "Verify that PROT_EN is 0");
                if (rc16->prot_en) {
                        logging(LOG_VERBOSE, "[FAILED] PROT_EN is set but "
                                "the device does not claim support for "
                                "protection information in the standard "
                                "inquiry VPD.");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] PROT_EN is 0");
                }
                CU_ASSERT_EQUAL(rc16->prot_en, 0);

                logging(LOG_VERBOSE, "Verify that P_TYPE is 0");
                if (rc16->p_type) {
                        logging(LOG_VERBOSE, "[FAILED] P_TYPE is non-zero but "
                                "the device does not claim support for "
                                "protection information in the standard "
                                "inquiry VPD.");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] P_TYPE is 0");
                }
                CU_ASSERT_EQUAL(rc16->p_type, 0);

                logging(LOG_VERBOSE, "Verify that P_I_EXP is 0");
                if (rc16->p_i_exp) {
                        logging(LOG_VERBOSE, "[FAILED] P_I_EXP is non-zero but "
                                "the device does not claim support for "
                                "protection information in the standard "
                                "inquiry VPD.");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] P_I_EXP is 0");
                }
                CU_ASSERT_EQUAL(rc16->p_i_exp, 0);

                return;
        }

        logging(LOG_VERBOSE, "This device supports PI. "
                "Verify that all relevant fields are sane");
        if (!rc16->prot_en) {
                logging(LOG_VERBOSE, "Protection is not enabled. Verify "
                        "that all relevant fields are zero");

                logging(LOG_VERBOSE, "Verify that P_TYPE is 0");
                if (rc16->p_type) {
                        logging(LOG_VERBOSE, "[FAILED] P_TYPE is non-zero but "
                                "protection information is not enabled.");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] P_TYPE is 0");
                }
                CU_ASSERT_EQUAL(rc16->p_type, 0);

                logging(LOG_VERBOSE, "Verify that P_I_EXP is 0");
                if (rc16->p_i_exp) {
                        logging(LOG_VERBOSE, "[FAILED] P_I_EXP is non-zero but "
                                "protection information is not enabled");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] P_I_EXP is 0");
                }
                CU_ASSERT_EQUAL(rc16->p_i_exp, 0);

                return;
        }

        logging(LOG_VERBOSE, "Protection is enabled. Verify "
                "that all relevant fields are sane");
        switch (rc16->p_type) {
        case 0:
        case 1:
        case 2:
             break;
        default:
                logging(LOG_VERBOSE, "[FAILED] P_TYPE is invalid. Must be "
                        "0,1,2 but was %d", rc16->p_type);
                CU_FAIL("P_TYPE is invalid");
        }
}
