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
#include <stdlib.h>
#include <string.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_read10_rdprotect(void)
{
	int i;


	if (device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		CU_PASS("[SKIPPED] LUN is not SBC device. Skipping test");
		return;
	}

	/*
	 * Try out Different non-zero values for RDPROTECT.
	 * They should all fail.
	 */

	/* Read10 with non-zero RDPROTECT ... */
	for (i = 1; i < 8; i++) {
		struct scsi_task *task_ret;

		task = malloc(sizeof(struct scsi_task));
		CU_ASSERT_PTR_NOT_NULL(task);

		memset(task, 0, sizeof(struct scsi_task));
		task->cdb[0] = SCSI_OPCODE_READ10;
		task->cdb[1] = (i<<5)&0xe0;
		task->cdb[8] = 1;
		task->cdb_size = 10;
		task->xfer_dir = SCSI_XFER_READ;
		task->expxferlen = block_size;

		task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
		CU_ASSERT_PTR_NOT_NULL(task_ret);

		CU_ASSERT_EQUAL(task->status, SCSI_STATUS_CHECK_CONDITION);
		CU_ASSERT_EQUAL(task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
		CU_ASSERT_EQUAL(task->sense.ascq, SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB);
		scsi_free_scsi_task(task);
		task = NULL;
	}
}
