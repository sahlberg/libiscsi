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


static void
verify_persistent_reserve_access(struct iscsi_context *iscsi1, int lun1,
    struct iscsi_context *iscsi2, int lun2,
    const enum scsi_persistent_out_type pr_type,
    int reg_i2_can_read,
    int reg_i2_can_write,
    int unreg_i2_can_read,
    int unreg_i2_can_write)
{
	int ret;
	const unsigned long long key = rand_key();
	const unsigned long long key2 = rand_key();


	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE,
	    "Verify access for reservation type: %s",
	    scsi_pr_type_str(pr_type));

	/* send TURs to clear possible check conditions */
	(void) testunitready_clear_ua(iscsi1, lun1);
	(void) testunitready_clear_ua(iscsi2, lun2);

	/* register our reservation key with the target */
	ret = prout_register_and_ignore(iscsi1, lun1, key);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] PERSISTEN RESERVE OUT is not implemented.");
		CU_PASS("PERSISTENT RESERVE OUT is not implemented.");
		return;
	}	
	CU_ASSERT_EQUAL(0, ret);
	ret = prout_register_and_ignore(iscsi2, lun2, key2);
	CU_ASSERT_EQUAL(0, ret);

	/* reserve the target through initiator 1 */
	ret = prout_reserve(iscsi1, lun1, key, pr_type);
	CU_ASSERT_EQUAL(0, ret);

	/* verify target reservation */
	ret = prin_verify_reserved_as(iscsi1, lun1,
	    pr_type_is_all_registrants(pr_type) ? 0 : key,
	    pr_type);
	CU_ASSERT_EQUAL(0, ret);

	read_write_buf = malloc(512);	       /* allocate a buffer */
	CU_ASSERT_PTR_NOT_NULL(read_write_buf);

	/* make sure init1 can read */
	ret = verify_read_works(iscsi1, lun1, read_write_buf);
	CU_ASSERT_EQUAL(0, ret);

	/* make sure init1 can write */
	ret = verify_write_works(iscsi1, lun1, read_write_buf);
	CU_ASSERT_EQUAL(0, ret);

	/* verify registered init2 read access */
	if (reg_i2_can_read)
		ret = verify_read_works(iscsi2, lun2, read_write_buf);
	else
		ret = verify_read_fails(iscsi2, lun2, read_write_buf);
	CU_ASSERT_EQUAL(0, ret);

	/* verify registered init2 write access */
	if (reg_i2_can_write)
		ret = verify_write_works(iscsi2, lun2, read_write_buf);
	else
		ret = verify_write_fails(iscsi2, lun2, read_write_buf);
	CU_ASSERT_EQUAL(0, ret);

	/* unregister init2 */
	ret = prout_register_key(iscsi2, lun2, 0, key);
	CU_ASSERT_EQUAL(0, ret);

	/* verify unregistered init2 read access */
	if (unreg_i2_can_read)
		ret = verify_read_works(iscsi2, lun2, read_write_buf);
	else
		ret = verify_read_fails(iscsi2, lun2, read_write_buf);
	CU_ASSERT_EQUAL(0, ret);

	/* verify unregistered init2 write access */
	if (unreg_i2_can_write)
		ret = verify_write_works(iscsi2, lun2, read_write_buf);
	else
		ret = verify_write_fails(iscsi2, lun2, read_write_buf);
	CU_ASSERT_EQUAL(0, ret);

	/* release our reservation */
	ret = prout_release(iscsi1, lun1, key, pr_type);
	CU_ASSERT_EQUAL(0, ret);

	/* remove our key from the target */
	ret = prout_register_key(iscsi1, lun1, 0, key);
	CU_ASSERT_EQUAL(0, ret);
}

void
test_prout_reserve_access_ea(void)
{
	verify_persistent_reserve_access(iscsic, tgt_lun, iscsic2, tgt_lun2,
	    SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS,
	    0, 0, 0, 0);
}

void
test_prout_reserve_access_we(void)
{
	verify_persistent_reserve_access(iscsic, tgt_lun, iscsic2, tgt_lun2,
	    SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
	    1, 0, 1, 0);
}

void
test_prout_reserve_access_earo(void)
{
	verify_persistent_reserve_access(iscsic, tgt_lun, iscsic2, tgt_lun2,
	    SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY,
	    1, 1, 0, 0);
}

void
test_prout_reserve_access_wero(void)
{
	verify_persistent_reserve_access(iscsic, tgt_lun, iscsic2, tgt_lun2,
	    SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY,
	    1, 1, 1, 0);
}

void
test_prout_reserve_access_eaar(void)
{
	verify_persistent_reserve_access(iscsic, tgt_lun, iscsic2, tgt_lun2,
	    SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS,
	    1, 1, 0, 0);
}

void
test_prout_reserve_access_wear(void)
{
	verify_persistent_reserve_access(iscsic, tgt_lun, iscsic2, tgt_lun2,
	    SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS,
	    1, 1, 1, 0);
}
