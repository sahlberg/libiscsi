/* 
   Copyright (C) 2013 by Lee Duncan <lee@gonzoleeman.net>
   
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


/*
 * list of persistent reservation types to test, in order
 */
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
test_prout_reserve_simple(void)
{
        struct scsi_persistent_reserve_in_report_capabilities *rcaps = NULL;
        uint16_t pr_type_mask = SCSI_PR_TYPE_MASK_ALL;
        struct test_prin_report_caps_types *type;
        struct scsi_task *tsk;
        int ret = 0;
        const unsigned long long key = rand_key();


        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve IN RESERVE works.");

        /* register our reservation key with the target */
        ret = prout_register_and_ignore(sd, key);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }        
        CU_ASSERT_EQUAL(ret, 0);

        ret = prin_report_caps(sd, &tsk, &rcaps);
        if (!ret && rcaps->tmv)
                pr_type_mask = rcaps->persistent_reservation_type_mask;

        /* test each supported reservatoin type */
        for (type = &report_caps_types_array[0]; type->mask != 0; type++) {
	        if (!(pr_type_mask & type->mask)) {
                        logging(LOG_NORMAL,
                                "PERSISTENT RESERVE op 0x%x not supported",
                                type->op);
                        continue;
                }

                /* reserve the target */
                ret = prout_reserve(sd, key, type->op);
                CU_ASSERT_EQUAL(ret, 0);

                /* verify target reservation */
                ret = prin_verify_reserved_as(sd,
                    pr_type_is_all_registrants(type->op) ? 0 : key,
                    type->op);
                CU_ASSERT_EQUAL(ret, 0);

                /* release our reservation */
                ret = prout_release(sd, key, type->op);
                CU_ASSERT_EQUAL(ret, 0);
        }

        scsi_free_scsi_task(tsk);

        /* remove our key from the target */
        ret = prout_register_key(sd, 0, key);
        CU_ASSERT_EQUAL(ret, 0);

}
