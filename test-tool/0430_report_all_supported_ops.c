/*
   Copyright (C) 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   Copyright (C) 2012 by Jon Grimm <jon.grimm@gmail.com>

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
#include <string.h>
#include <ctype.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0430_report_all_supported_ops(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_report_supported_op_codes *tmp_rsoc;
	struct scsi_command_descriptor *desc;
	int ret, lun;
	int full_size, desc_size;
	int i;

	printf("0430_report_all_supported_ops:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test MaintenanceIn: Report Supported Operations.\n");
		printf("1, Report Supported Ops (no timeout information).\n");
		printf("2, Report Supported Ops (with timeout information).\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	ret = 0;

	printf("See if Report Supported Opcodes is supported... ");
	/* See how big data is */
	task = iscsi_report_supported_opcodes_sync(iscsi, lun,
			0, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
			4);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send Report Supported Opcodes command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("REPORT SUPPORTED OPCODES command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -2;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("REPORT SUPPORTED OPCODES command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	full_size = scsi_datain_getfullsize(task);

	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);
		/* we need more data for the full list */
		if ((task = iscsi_report_supported_opcodes_sync(iscsi, lun,
				0, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
				full_size)) == NULL) {
			printf("[FAILED]\n");
			printf("REPORT SUPPORTED OPCODES failed : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
	}
	tmp_rsoc = scsi_datain_unmarshall(task);
	if (tmp_rsoc == NULL) {
		printf("[FAILED]\n");
		printf("failed to unmarshall REPORT SUPPORTED OPCODES datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("Supported Commands: %d\n", tmp_rsoc->num_descriptors);
	printf("=======================\n");
	for (i = 0; i < tmp_rsoc->num_descriptors; i++) {
		printf("op:%x\tsa:%x\tcdb length:%d\n",
		       tmp_rsoc->descriptors[i].opcode,
		       tmp_rsoc->descriptors[i].sa,
		       tmp_rsoc->descriptors[i].cdb_len);
	}

	printf("\n[OK]\n");
	scsi_free_scsi_task(task);

	/*Report All Supported Operations including timeout info.*/
	printf("See if Report Supported Opcodes with Timeouts is supported... ");
	/* See how big data is */
	task = iscsi_report_supported_opcodes_sync(iscsi, lun,
			1, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
			4);
	if (task == NULL) {
		printf("[FAILED]\n");
		printf("Failed to send Report Supported Opcodes command : %s\n",
		       iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && (task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE
		|| task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB)) {
		printf("[SKIPPED]\n");
		printf("REPORT SUPPORTED OPCODES command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -2;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("[FAILED]\n");
		printf("REPORT SUPPORTED OPCODES command failed : %s\n",
		       iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}
	full_size = scsi_datain_getfullsize(task);

	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_report_supported_opcodes_sync(iscsi, lun,
				1, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
				full_size)) == NULL) {
			printf("[FAILED]\n");
			printf("REPORT SUPPORTED OPCODES failed : %s\n", iscsi_get_error(iscsi));
			ret = -1;
			goto finished;
		}
	}
	tmp_rsoc = scsi_datain_unmarshall(task);
	if (tmp_rsoc == NULL) {
		printf("[FAILED]\n");
		printf("failed to unmarshall REPORT SUPPORTED OPCODES datain blob\n");
		scsi_free_scsi_task(task);
		ret = -1;
		goto finished;
	}

	printf("Supported Commands (with timeout information): %d\n", tmp_rsoc->num_descriptors);
	printf("=======================\n");
	desc_size = sizeof (struct scsi_command_descriptor)
		+  sizeof (struct scsi_op_timeout_descriptor);
	desc = &tmp_rsoc->descriptors[0];
	for (i = 0; i < tmp_rsoc->num_descriptors; i++) {
		printf("op:%x\tsa:%x\tcdb_length:%d\ttimeout info: length:%d\tcommand specific:%x\tnominal processing%d\trecommended%d\n",
		       desc->opcode,
		       desc->sa,
		       desc->cdb_len,
		       desc->to.descriptor_length,
		       desc->to.command_specific,
		       desc->to.nominal_processing_timeout,
		       desc->to.recommended_timeout);
		desc = (struct scsi_command_descriptor *)((char *)desc +
							  desc_size);
	}

	printf("\n[OK]\n");
	scsi_free_scsi_task(task);

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
