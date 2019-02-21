/* 
   Copyright (C) 2013 by Lee Duncan <leeman.duncan@gmail.com>

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
#include "iscsi-test-cu.h"
#include "iscsi-multipath.h"

static void
verify_persistent_reserve_ownership(struct scsi_device *sd1, struct scsi_device *sd2,
    const enum scsi_persistent_out_type pr_type,
    int resvn_is_shared)
{
        int ret;
        const unsigned long long key1 = rand_key();
        const unsigned long long key2 = rand_key();


        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE,
            "Verify ownership for reservation type: %s",
            scsi_pr_type_str(pr_type));

        /* send TURs to clear possible check conditions */
        (void) testunitready_clear_ua(sd1);
        (void) testunitready_clear_ua(sd2);

        /* register our reservation key with the target */
        ret = prout_register_and_ignore(sd1, key1);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
                return;
        }        
        CU_ASSERT_EQUAL(0, ret);
        ret = prout_register_and_ignore(sd2, key2);
        CU_ASSERT_EQUAL(0, ret);

        /* reserve the target through initiator 1 */
        ret = prout_reserve(sd1, key1, pr_type);
        CU_ASSERT_EQUAL(0, ret);

        /* verify target reservation */
        ret = prin_verify_reserved_as(sd1,
            pr_type_is_all_registrants(pr_type) ? 0 : key1,
            pr_type);
        CU_ASSERT_EQUAL(0, ret);

        /* unregister init1 */
        ret = prout_register_key(sd1, 0, key1);
        CU_ASSERT_EQUAL(0, ret);

        /* verify if reservation is still present */
        if (resvn_is_shared) {
                /* verify target reservation */
                ret = prin_verify_reserved_as(sd1,
                    pr_type_is_all_registrants(pr_type) ? 0 : key1,
                    pr_type);
                CU_ASSERT_EQUAL(0, ret);

                /* release our reservation */
                ret = prout_release(sd2, key2, pr_type);
                CU_ASSERT_EQUAL(0, ret);
        } else {
                /* verify target is not reserved now */
                ret = prin_verify_not_reserved(sd1);
                CU_ASSERT_EQUAL(0, ret);

                /* send TUR to clear possible check condition */
                (void) testunitready_clear_ua(sd2);
        }

        /* remove our remaining key from the target */
        ret = prout_register_key(sd2, 0, key2);
        CU_ASSERT_EQUAL(0, ret);
}

void
test_prout_reserve_ownership_ea(void)
{
        struct scsi_device *sd2;
        int ret;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;
        verify_persistent_reserve_ownership(sd, sd2,
            SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS, 0);
        mpath_sd2_put(sd2);
}

void
test_prout_reserve_ownership_we(void)
{
        struct scsi_device *sd2;
        int ret;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;
        verify_persistent_reserve_ownership(sd, sd2,
            SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE, 0);
        mpath_sd2_put(sd2);
}

void
test_prout_reserve_ownership_earo(void)
{
        struct scsi_device *sd2;
        int ret;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;
        verify_persistent_reserve_ownership(sd, sd2,
            SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY, 0);
        mpath_sd2_put(sd2);
}

void
test_prout_reserve_ownership_wero(void)
{
        struct scsi_device *sd2;
        int ret;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;
        verify_persistent_reserve_ownership(sd, sd2,
            SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY, 0);
        mpath_sd2_put(sd2);
}

void
test_prout_reserve_ownership_eaar(void)
{
        struct scsi_device *sd2;
        int ret;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;
        verify_persistent_reserve_ownership(sd, sd2,
            SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS, 1);
        mpath_sd2_put(sd2);
}

void
test_prout_reserve_ownership_wear(void)
{
        struct scsi_device *sd2;
        int ret;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This PERSISTENT RESERVE test is "
                        "only supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;
        verify_persistent_reserve_ownership(sd, sd2,
            SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS, 1);
        mpath_sd2_put(sd2);
}
