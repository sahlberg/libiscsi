/* 
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

void
test_modesense6_all_pages(void)
{
	struct scsi_mode_sense *ms;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test of MODESENSE6 AllPages");


	if (task != NULL) {
		scsi_free_scsi_task(task);
		task = NULL;
	}

	logging(LOG_VERBOSE, "Send MODESENSE6 command to fetch AllPages");
	task = iscsi_modesense6_sync(iscsic, tgt_lun, 0,
				     SCSI_MODESENSE_PC_CURRENT,
				     SCSI_MODEPAGE_RETURN_ALL_PAGES,
				     0, 255);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		logging(LOG_VERBOSE, "[FAILED] Failed to send MODE_SENSE6 "
			"command:%s",
			iscsi_get_error(iscsic));
		CU_FAIL("[FAILED] Failed to fetch the All Pages page.");
		return;
	}
	logging(LOG_VERBOSE, "[SUCCESS] All Pages fetched.");


	logging(LOG_VERBOSE, "Try to unmarshall the DATA-IN buffer.");
	ms = scsi_datain_unmarshall(task);
	if (ms == NULL) {
		logging(LOG_VERBOSE, "[FAILED] failed to unmarshall mode sense "
			"datain buffer");
		CU_FAIL("[FAILED] Failed to unmarshall the data-in buffer.");
		scsi_free_scsi_task(task);
		task = NULL;
		return;
	}
	logging(LOG_VERBOSE, "[SUCCESS] Unmarshalling successful.");


	logging(LOG_VERBOSE, "Verify that mode data length is >= 3");
	if (ms->mode_data_length >= 3) {
		logging(LOG_VERBOSE, "[SUCCESS] Mode data length is >= 3");
	} else {
		logging(LOG_VERBOSE, "[FAILED] Mode data length is < 3");
	}
	CU_ASSERT_TRUE(ms->mode_data_length >= 3);


	if (task != NULL) {
		scsi_free_scsi_task(task);
		task = NULL;
	}
}
