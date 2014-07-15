/* 
   Copyright (C) 2014 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#include <inttypes.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_get_lba_status_unmap_single(void)
{
	int ret;
	uint64_t i;
	unsigned char *buf = alloca(257 * block_size);
	struct scsi_task *task_ret;
	struct unmap_list list[1];
	struct scsi_get_lba_status *lbas = NULL;
	struct scsi_lba_status_descriptor *lbasd = NULL;

	CHECK_FOR_DATALOSS;

	memset(buf, 'A', 257 * block_size);

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test GET_LBA_STATUS for a single unmapped block "
		"at offset 0-255");
	for (i = 0; i < 256; i++) {
		logging(LOG_VERBOSE, "Write the first 257 blocks with a known "
			"pattern and thus map the blocks");
		ret = write10(iscsic, tgt_lun, 0, 257 * block_size,
			block_size, 0, 0, 0, 0, 0, buf);

		logging(LOG_VERBOSE, "Unmap a single block at LBA:%" PRIu64 "", i);
		list[0].lba = i;
		list[0].num = 1;
		ret = unmap(iscsic, tgt_lun, 0, list, 1);
		if (ret == -2) {
			CU_PASS("[SKIPPED] Target does not support UNMAP. "
				"Skipping test");
			return;
		}
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read the status of the block at LBA:%"
			PRIu64, i);
		task_ret = get_lba_status_task(iscsic, tgt_lun, i, 24);
		if (!task_ret) {
			CU_PASS("[SKIPPED] Target does not support "
				"GET_LBA_STATUS. Skipping test");
			return;
		}

		lbas = scsi_datain_unmarshall(task_ret);
		if (lbas == NULL) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"failed to unmarschall data.");
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"failed to unmarschall data.");
			scsi_free_scsi_task(task_ret);
			return;
		}
		lbasd = &lbas->descriptors[0];
	

		logging(LOG_VERBOSE, "Verify that the LBA in the first "
			"descriptor matches the LBA in the CDB");
		if (lbasd->lba != i) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"LBA offset in first descriptor does not match "
				"request (0x%" PRIx64 " != 0x%" PRIx64 ").",
				lbasd->lba, i);
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"LBA offset in first descriptor does not match "
				"teh LBA in the CDB.");
			scsi_free_scsi_task(task_ret);
			return;
		}

		logging(LOG_VERBOSE, "Verify that the first descriptor is for "
			"a single block");
		if (lbasd->num_blocks != 1) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"The returned range is not for a single block");
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"The returned range is not for a single block");
			scsi_free_scsi_task(task_ret);
			return;
		}

		logging(LOG_VERBOSE, "Verify that the first descriptor is for "
			"a deallocated block");
		if (lbasd->provisioning != SCSI_PROVISIONING_TYPE_DEALLOCATED) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"The block at LBA %" PRIu64 " is not "
				"deallocated", i);
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"LBA wrong provisioning status for deallocated "
				"block.");
			scsi_free_scsi_task(task_ret);
			return;
		}
		scsi_free_scsi_task(task_ret);
	}

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test GET_LBA_STATUS for a single range of 1-255 "
		"blocks at offset 0");
	for (i = 1; i < 256; i++) {
		logging(LOG_VERBOSE, "Write the first 257 blocks with a known "
			"pattern and thus map the blocks");
		ret = write10(iscsic, tgt_lun, 0, 257 * block_size,
			block_size, 0, 0, 0, 0, 0, buf);

		logging(LOG_VERBOSE, "Unmap %" PRIu64 " blocks at LBA 0", i);
		list[0].lba = 0;
		list[0].num = i;
		ret = unmap(iscsic, tgt_lun, 0, list, 1);
		CU_ASSERT_EQUAL(ret, 0);

		logging(LOG_VERBOSE, "Read the status of the block at LBA 0");
		task_ret = get_lba_status_task(iscsic, tgt_lun, 0, 24);
		lbas = scsi_datain_unmarshall(task_ret);
		if (lbas == NULL) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"failed to unmarschall data.");
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"failed to unmarschall data.");
			scsi_free_scsi_task(task_ret);
			return;
		}
		lbasd = &lbas->descriptors[0];
	

		logging(LOG_VERBOSE, "Verify that the LBA in the first "
			"descriptor matches the LBA in the CDB");
		if (lbasd->lba != 0) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"LBA offset in first descriptor does not match "
				"request (0x%" PRIx64 " != 0x%" PRIx64 ").",
				lbasd->lba, i);
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"LBA offset in first descriptor does not match "
				"teh LBA in the CDB.");
			scsi_free_scsi_task(task_ret);
			return;
		}

		logging(LOG_VERBOSE, "Verify that the first descriptor is for "
			"%" PRIu64 " blocks", i);
		if (lbasd->num_blocks != i) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"The returned range is not for a single block");
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"The returned range is not for a single block");
			scsi_free_scsi_task(task_ret);
			return;
		}

		logging(LOG_VERBOSE, "Verify that the first descriptor is for "
			"a deallocated block");
		if (lbasd->provisioning != SCSI_PROVISIONING_TYPE_DEALLOCATED) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"The block at LBA %" PRIu64 " is not "
				"deallocated", i);
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"LBA wrong provisioning status for deallocated "
				"block.");
			scsi_free_scsi_task(task_ret);
			return;
		}
		scsi_free_scsi_task(task_ret);


		logging(LOG_VERBOSE, "Read the status of the block at LBA %"
			PRIu64, i - 1);
		task_ret = get_lba_status_task(iscsic, tgt_lun, i - 1, 24);
		lbas = scsi_datain_unmarshall(task_ret);
		if (lbas == NULL) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"failed to unmarschall data.");
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"failed to unmarschall data.");
			scsi_free_scsi_task(task_ret);
			return;
		}
		lbasd = &lbas->descriptors[0];
	

		logging(LOG_VERBOSE, "Verify that the LBA in the first "
			"descriptor matches the LBA in the CDB");
		if (lbasd->lba != i - 1) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"LBA offset in first descriptor does not match "
				"request (0x%" PRIx64 " != 0x%" PRIx64 ").",
				lbasd->lba, i);
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"LBA offset in first descriptor does not match "
				"teh LBA in the CDB.");
			scsi_free_scsi_task(task_ret);
			return;
		}

		logging(LOG_VERBOSE, "Verify that the first descriptor is for "
			"a single block");
		if (lbasd->num_blocks != 1) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"The returned range is not for a single block");
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"The returned range is not for a single block");
			scsi_free_scsi_task(task_ret);
			return;
		}

		logging(LOG_VERBOSE, "Verify that the first descriptor is for "
			"a deallocated block");
		if (lbasd->provisioning != SCSI_PROVISIONING_TYPE_DEALLOCATED) {
			logging(LOG_NORMAL, "[FAILED] GET_LBA_STATUS command: "
				"The block at LBA %" PRIu64 " is not "
				"deallocated", i);
			CU_FAIL("[FAILED] GET_LBA_STATUS command: "
				"LBA wrong provisioning status for deallocated "
				"block.");
			scsi_free_scsi_task(task_ret);
			return;
		}
		scsi_free_scsi_task(task_ret);
	}
}
