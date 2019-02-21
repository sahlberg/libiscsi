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


void
test_prout_register_simple(void)
{
        const unsigned long long key = rand_key();
        int ret = 0;


        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve IN REGISTER works.");

        /* register our reservation key with the target */
        ret = prout_register_and_ignore(sd, key);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }        
        CU_ASSERT_EQUAL(ret, 0);

        /* verify we can read the registration */
        ret = prin_verify_key_presence(sd, key, 1);
        CU_ASSERT_EQUAL(ret, 0);

        /* try to reregister, which should fail */
        ret = prout_reregister_key_fails(sd, key+1);
        CU_ASSERT_EQUAL(ret, 0);

        /* release from the target */
        ret = prout_register_key(sd, 0, key);
        CU_ASSERT_EQUAL(ret, 0);

        /* Verify the registration is gone */
        ret = prin_verify_key_presence(sd, key, 0);
        CU_ASSERT_EQUAL(ret, 0);
}
