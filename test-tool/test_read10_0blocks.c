/* 
   Copyright (C) 2012 by Lee Duncan <lee@gonzoleeman.net>
   
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
#include "iscsi-test-cu.h"

void
test_read10_0blocks(void)
{
	/* read zero blocks at LBA 0 ... */
	task = iscsi_read10_sync(iscsic, tgt_lun, 0, 0, block_size,
	    0, 0, 0, 0, 0);
	CU_ASSERT_PTR_NOT_NULL(task);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;

	/* READ10 0blocks at one block beyond <end-of-LUN> ... */
	if (num_blocks > 0x80000000) {
		CU_PASS("[SKIPPED] LUN is too big");
		return;
	}
	task = iscsi_read10_sync(iscsic, tgt_lun, num_blocks + 1, 0,
	    block_size, 0, 0, 0, 0, 0);
	CU_ASSERT_PTR_NOT_NULL(task);
	CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
	CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
	CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
	scsi_free_scsi_task(task);
	task = NULL;

	/* READ10 0blocks at LBA 2^31 ... */
	task = iscsi_read10_sync(iscsic, tgt_lun, 0x80000000, 0, block_size,
	    0, 0, 0, 0, 0);
	CU_ASSERT_PTR_NOT_NULL(task);
	CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
	CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
	CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
	scsi_free_scsi_task(task);
	task = NULL;

	/* READ10 0blocks at LBA -1 ... */
	task = iscsi_read10_sync(iscsic, tgt_lun, -1, 0, block_size,
	    0, 0, 0, 0, 0);
	CU_ASSERT_PTR_NOT_NULL(task);
	CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
	CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
	CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
	scsi_free_scsi_task(task);
	task = NULL;
}
