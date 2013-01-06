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
test_read10_flags(void)
{ 
	struct scsi_task *task_ret;


	fprintf(stderr, "DEBUG: %s: entering\n", __FUNCTION__);

	/* This test is only valid for SBC devices */
	if (device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		CU_PASS("[SKIPPED] LUN is not SBC device. Skipping test");
		return;
	}

	/* Try out READ10 with DPO : 1 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[1] = 0x10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try out READ10 with FUA : 1  FUA_NV : 0 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[1] = 0x08;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try out READ10 with FUA : 1  FUA_NV : 1 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[1] = 0x0a;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try out READ10 with FUA : 0  FUA_NV : 1 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[1] = 0x02;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try out READM10 with DPO : 1 FUA : 1  FUA_NV : 1 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[1] = 0x18;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;
}
