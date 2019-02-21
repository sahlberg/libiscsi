/*
   Copyright (C) 2015 David Disseldorp

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
#include <arpa/inet.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

static struct test_prin_report_caps_types {
        enum scsi_persistent_reservation_type_mask mask;
        enum scsi_persistent_out_type op;
} report_caps_types_array[] = {
        { SCSI_PR_TYPE_MASK_WR_EX_AR,
          SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS },
        { SCSI_PR_TYPE_MASK_EX_AC_RO,
          SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY },
        { SCSI_PR_TYPE_MASK_WR_EX_RO,
          SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY },
        { SCSI_PR_TYPE_MASK_EX_AC,
          SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS },
        { SCSI_PR_TYPE_MASK_WR_EX,
          SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE },
        { SCSI_PR_TYPE_MASK_EX_AC_AR,
          SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS },
        { 0, 0 }
};

void
test_prin_report_caps_simple(void)
{
        int ret = 0;
        const unsigned long long key = rand_key();
        struct scsi_task *tsk;
        struct scsi_persistent_reserve_in_report_capabilities *rcaps = NULL;
        struct test_prin_report_caps_types *type;

        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE,
                "Test Persistent Reserve In REPORT CAPABILITIES works.");

        /* register our reservation key with the target */
        ret = prout_register_and_ignore(sd, key);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);

        ret = prin_report_caps(sd, &tsk, &rcaps);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_NOT_EQUAL(rcaps, NULL);

        if (!rcaps)
                return;

        logging(LOG_VERBOSE,
                "Checking PERSISTENT RESERVE IN REPORT CAPABILITIES fields.");
        CU_ASSERT_EQUAL(rcaps->length, 8);
        CU_ASSERT_TRUE(rcaps->allow_commands <= 5);
        CU_ASSERT_EQUAL(rcaps->persistent_reservation_type_mask
                        & ~SCSI_PR_TYPE_MASK_ALL, 0);

        for (type = &report_caps_types_array[0]; type->mask != 0; type++) {
                if (!(rcaps->persistent_reservation_type_mask & type->mask)) {
                        logging(LOG_NORMAL,
                                "PERSISTENT RESERVE op 0x%x not supported",
                                type->op);
                        continue;
                }

                logging(LOG_VERBOSE,
                        "PERSISTENT RESERVE OUT op 0x%x supported, testing",
                        type->op);

                /* reserve the target */
                ret = prout_reserve(sd, key, type->op);
                CU_ASSERT_EQUAL(ret, 0);

                /* verify target reservation */
                ret = prin_verify_reserved_as(sd,
                                pr_type_is_all_registrants(type->op) ? 0 : key,
                                type->op);
                CU_ASSERT_EQUAL(0, ret);

                /* release the target */
                ret = prout_release(sd, key, type->op);
                CU_ASSERT_EQUAL(ret, 0);
        }

        scsi_free_scsi_task(tsk);
        rcaps = NULL;        /* freed with tsk */

        /* drop registration */
        ret = prout_register_key(sd, 0, key);
        CU_ASSERT_EQUAL(ret, 0);
}
