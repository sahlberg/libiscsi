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
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"


void
test_read10_invalid(void)
{
	struct iscsi_data data;
	char buf[4096];
	struct scsi_task *task_ret;


	/* Try a read10 of 1 block but xferlength == 0 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 0;

	/*
	 * we dont want autoreconnect since some targets will drop the session
	 * on this condition.
	 */
	iscsi_set_noautoreconnect(iscsic, 1);

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_CANCELLED); /* XXX redundant? */
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);
	CU_ASSERT_EQUAL(task->residual, (int64_t)block_size);
	scsi_free_scsi_task(task);
	task = NULL;

	/* in case the previous test failed the session */
	iscsi_set_noautoreconnect(iscsic, 0);

	/* Try a read10 of 1 block but xferlength == 1024 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 1024;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);
	CU_ASSERT_EQUAL(task->residual, (int64_t)block_size);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try a read10 of 1 block but xferlength == 200 */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 200;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);
	CU_ASSERT_EQUAL(task->residual, (int64_t)block_size - 200);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try a read10 of 2 blocks but xferlength == 'block_size' */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 2;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = block_size;

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, NULL);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);
	CU_ASSERT_EQUAL(task->residual, (int64_t)block_size);
	scsi_free_scsi_task(task);
	task = NULL;

	/* Try a read10 of 1 block but make it a data-out write on the iscsi layer */
	task = malloc(sizeof(struct scsi_task));
	CU_ASSERT_PTR_NOT_NULL(task);

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_READ10;
	task->cdb[8] = 1;
	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = sizeof(buf);

	data.size = sizeof(buf);
	data.data = (unsigned char *)&buf[0];

	task_ret = iscsi_scsi_command_sync(iscsic, tgt_lun, task, &data);
	CU_ASSERT_PTR_NOT_NULL(task_ret);
	CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(task);
	task = NULL;
}
