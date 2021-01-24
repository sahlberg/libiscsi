/*
   Copyright (C) SUSE LLC 2020

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
#include <stdlib.h>
#include <poll.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_report_luns_simple(void)
{
	struct scsi_task *rl_task;
	struct scsi_reportluns_list *rl_list;
	int full_report_size;
	uint32_t i;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test REPORT LUNS");

	rl_task = iscsi_reportluns_sync(sd->iscsi_ctx, 0, 512);
	CU_ASSERT_PTR_NOT_NULL_FATAL(rl_task);

	full_report_size = scsi_datain_getfullsize(rl_task);
	if (full_report_size > rl_task->datain.size) {
		logging(LOG_NORMAL,
			"[SKIPPED] REPORT LUNS response truncated.");
		CU_PASS("REPORT LUNS response truncated.");
	}

	rl_list = scsi_datain_unmarshall(rl_task);
	if (rl_list == NULL) {
		fprintf(stderr, "failed to unmarshall reportluns datain blob\n");
		exit(10);
	}

	for (i = 0; i < rl_list->num; i++) {
		uint32_t j;
		logging(LOG_VERBOSE, "LUN[%u]: 0x%02hx", (unsigned)i,
			(unsigned short)rl_list->luns[i]);
		/* inefficiently check for duplicates */
		for (j = i + 1; j < rl_list->num; j++)
			CU_ASSERT_NOT_EQUAL(rl_list->luns[i], rl_list->luns[j]);
	}

	scsi_free_scsi_task(rl_task);
}

struct tests_report_luns_state {
	uint32_t io_dispatched;
	uint32_t io_completed;
};

static void
test_report_luns_cb(struct iscsi_context *iscsi __attribute__((unused)),
		    int status, void *command_data,
		    void *private_data)
{
	struct scsi_task *rl_task = command_data;
	struct tests_report_luns_state *state = private_data;

	state->io_completed++;
	CU_ASSERT_EQUAL(status, SCSI_STATUS_GOOD);
	scsi_free_scsi_task(rl_task);
}

static void
test_report_luns_invalid_cb(struct iscsi_context *iscsi __attribute__((unused)),
			    int status, void *command_data, void *private_data)
{
	struct scsi_task *rl_task = command_data;
	struct tests_report_luns_state *state = private_data;

	state->io_completed++;
	CU_ASSERT_EQUAL(status, SCSI_STATUS_CHECK_CONDITION);
	CU_ASSERT_EQUAL(rl_task->sense.key, SCSI_SENSE_ILLEGAL_REQUEST);
	CU_ASSERT_EQUAL(rl_task->sense.ascq,
			SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB);
	scsi_free_scsi_task(rl_task);
}

void
test_report_luns_alloclen(void)
{
	struct tests_report_luns_state state = { };
	struct scsi_task *rl_task;
	int ret;
	int i;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test REPORT LUNS - ALLOCATION LENGTH");

	/*
	 * SPC-3 6.21 REPORT LUNS command:
	 * NOTE 30 - Device servers compliant with SPC return CHECK CONDITION
	 * status, with the sense key set to ILLEGAL REQUEST, and the additional
	 * sense code set to INVALID FIELD IN CDB when the allocation length is
	 * less than 16 bytes.
	 *
	 * Don't use iscsi_reportluns_task(), as it blocks invalid alloc len
	 */
	for (i = 0; i < 32; i++) {
		iscsi_command_cb cb;

		rl_task = scsi_reportluns_cdb(0,	/* SELECT REPORT */
					      i);	/* ALLOCATION LENGTH */
		CU_ASSERT_PTR_NOT_NULL_FATAL(rl_task);

		if (i < 16)
			cb = test_report_luns_invalid_cb;
		else
			cb = test_report_luns_cb;
		/* report luns are always sent to lun 0 */
		ret = iscsi_scsi_command_async(sd->iscsi_ctx, 0, rl_task, cb,
					       NULL, &state);
		CU_ASSERT_EQUAL(ret, 0);
		state.io_dispatched++;
	}

	logging(LOG_VERBOSE, "Awaiting completion of %d REPORT LUNS requests",
		state.io_dispatched);
	while (state.io_completed < state.io_dispatched) {
		struct pollfd pfd;

		pfd.fd = iscsi_get_fd(sd->iscsi_ctx);
		pfd.events = iscsi_which_events(sd->iscsi_ctx);

		ret = poll(&pfd, 1, -1);
		CU_ASSERT_NOT_EQUAL(ret, -1);

		ret = iscsi_service(sd->iscsi_ctx, pfd.revents);
		CU_ASSERT_EQUAL(ret, 0);
	}
}
