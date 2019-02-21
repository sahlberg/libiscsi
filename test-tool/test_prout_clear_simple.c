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

void
test_prout_clear_simple(void)
{
        int ret = 0;
        uint32_t old_gen;
        const unsigned long long key = rand_key();
        struct scsi_task *tsk;
        struct scsi_persistent_reserve_in_read_keys *rk = NULL;

        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve OUT CLEAR works.");

        /* register our reservation key with the target */
        ret = prout_register_and_ignore(sd, key);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);

        ret = prin_read_keys(sd, &tsk, &rk, 16384);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_NOT_EQUAL(rk, NULL);
        if (!rk)
                goto out;

        CU_ASSERT_NOT_EQUAL(rk->num_keys, 0);
        /* retain PR generation number to check for increments */
        old_gen = rk->prgeneration;

        scsi_free_scsi_task(tsk);
        rk = NULL;        /* freed with tsk */

        /* reserve the target */
        ret = prout_reserve(sd, key,
                            SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS);
        CU_ASSERT_EQUAL(ret, 0);

        /* verify target reservation */
        ret = prin_verify_reserved_as(sd, key,
                                SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS);
        CU_ASSERT_EQUAL(ret, 0);

        /* clear reservation and registration */
        ret = prout_clear(sd, key);
        CU_ASSERT_EQUAL(ret, 0);

        ret = prin_verify_not_reserved(sd);
        CU_ASSERT_EQUAL(ret, 0);

        ret = prin_read_keys(sd, &tsk, &rk, 16384);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_NOT_EQUAL(rk, NULL);
        if (!rk)
                goto out;

        CU_ASSERT_EQUAL(rk->num_keys, 0);
        /* generation incremented once for CLEAR (not for RESERVE) */
        CU_ASSERT_EQUAL(rk->prgeneration, old_gen + 1);

out:
        scsi_free_scsi_task(tsk);
        rk = NULL;        /* freed with tsk */
}
