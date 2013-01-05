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
test_read10_beyond_eol(void)
{ 
	int i;

	if (num_blocks >= 0x80000000) {
		CU_PASS("LUN is too big for read-beyond-eol tests with READ10. Skipping test.\n");
		return;
	}

	/* read 1-256 blocks, one block beyond the end-of-lun */
	for (i = 1; i <= 256; i++) {
		task = iscsi_read10_sync(iscsic, tgt_lun, num_blocks + 2 - i,
		    i * block_size, block_size, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL(task);
		CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
		CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
		CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
		CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
		scsi_free_scsi_task(task);
		task = NULL;
	}

	/* Reading 1-256 blocks at LBA 2^31 */
	for (i = 1; i <= 256; i++) {
		task = iscsi_read10_sync(iscsic, tgt_lun, 0x80000000,
		    i * block_size, block_size, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL(task);
		CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
		CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
		CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
		CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
		scsi_free_scsi_task(task);
		task = NULL;
	}

	/* read 1 - 256 blocks at LBA -1 */
	for (i = 1; i <= 256; i++) {
		task = iscsi_read10_sync(iscsic, tgt_lun, -1, i * block_size,
		    block_size, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL(task);
		CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
		CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
		CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
		CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
		scsi_free_scsi_task(task);
		task = NULL;
	}

	/* read 2-256 blocks, all but one block beyond the eol */
	for (i = 2; i <= 256; i++) {
		task = iscsi_read10_sync(iscsic, tgt_lun, num_blocks,
		    i * block_size, block_size, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL(task);
		CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_GOOD);
		CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
		CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
		CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE);
		scsi_free_scsi_task(task);
		task = NULL;
	}
}
