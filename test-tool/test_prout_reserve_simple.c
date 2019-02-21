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
static enum scsi_persistent_out_type pr_types_to_test[] = {
        SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
        SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS,
        SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY,
        SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY,
        SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS,
        SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS,
        0
};


void
test_prout_reserve_simple(void)
{
        int ret = 0;
        int i;
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

        /* test each reservatoin type */
        for (i = 0; pr_types_to_test[i] != 0; i++) {
                enum scsi_persistent_out_type pr_type = pr_types_to_test[i];

                /* reserve the target */
                ret = prout_reserve(sd, key, pr_type);
                CU_ASSERT_EQUAL(ret, 0);

                /* verify target reservation */
                ret = prin_verify_reserved_as(sd,
                    pr_type_is_all_registrants(pr_type) ? 0 : key,
                    pr_type);
                CU_ASSERT_EQUAL(ret, 0);

                /* release our reservation */
                ret = prout_release(sd, key, pr_type);
                CU_ASSERT_EQUAL(ret, 0);
        }

        /* remove our key from the target */
        ret = prout_register_key(sd, 0, key);
        CU_ASSERT_EQUAL(ret, 0);

}
