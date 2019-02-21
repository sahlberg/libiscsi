/*
   Copyright (C) 2013 by Lee Duncan <lee@gonzoleeman.net>
   Copyright (C) 2018 David Disseldorp

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


void
test_prin_read_keys_truncate(void)
{
        const unsigned long long key = rand_key();
        struct scsi_persistent_reserve_in_read_keys *rk = NULL;
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve IN READ_KEYS works when "
                "truncated.");

        ret = prout_register_and_ignore(sd, key);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);

        /*
         * alloc_len=8 restricts the response buffer to only accepting the
         * PRGENERATION and ADDITIONAL LENGTH fields.
         */
        ret = prin_read_keys(sd, &task, &rk, 8);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE IN is not implemented.");
                prout_register_key(sd, 0, key);
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);

        if (rk) {
                /*
                 * SPC5r17: 6.16.2 READ KEYS service action
                 * The ADDITIONAL LENGTH field indicates the number of bytes in
                 * the Reservation key list. The contents of the ADDITIONAL
                 * LENGTH field are not altered based on the allocation length.
                 */
                CU_ASSERT_NOT_EQUAL(rk->additional_length, 0);
                /* key array should have been truncated in the response */
                CU_ASSERT_EQUAL(rk->num_keys, 0);
        }

        /* remove our key from the target */
        ret = prout_register_key(sd, 0, key);
        CU_ASSERT_EQUAL(0, ret);
}
