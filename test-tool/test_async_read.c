/*
   Copyright (C) SUSE LINUX GmbH 2016

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
#include <signal.h>
#include <poll.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"
#include "iscsi-multipath.h"

struct tests_async_read_state {
	uint32_t dispatched;
	uint32_t completed;
	uint32_t prev_cmdsn;
};

static void
test_async_read_cb(struct iscsi_context *iscsi __attribute__((unused)),
		   int status, void *command_data, void *private_data)
{
	struct scsi_task *atask = command_data;
	struct tests_async_read_state *state = private_data;

	state->completed++;
	logging(LOG_VERBOSE, "READ10 completed: %d of %d (CmdSN=%d)",
		state->completed, state->dispatched, atask->cmdsn);
	CU_ASSERT_NOT_EQUAL(status, SCSI_STATUS_CHECK_CONDITION);

	if ((state->completed > 1) && (atask->cmdsn != state->prev_cmdsn + 1)) {
		logging(LOG_VERBOSE,
			"out of order completion (CmdSN=%d, prev=%d)",
			atask->cmdsn, state->prev_cmdsn);
	}
	state->prev_cmdsn = atask->cmdsn;

	scsi_free_scsi_task(atask);
}

void
test_async_read(void)
{
	int i, ret;
	struct tests_async_read_state state = { 0, 0, 0 };
	const int blocksize = 512;
	const int blocks_per_io = 8;
	const int num_ios = 1000;
	/* IOs in flight concurrently, so need a buffer large enough for all */
	unsigned char *buf = calloc(blocksize * blocks_per_io, num_ios);

	CU_ASSERT_NOT_EQUAL(buf, NULL);
	if (!buf)
		goto out;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;
	if (sd->iscsi_ctx == NULL) {
                CU_PASS("[SKIPPED] Non-iSCSI");
		goto out;
	}

	if (maximum_transfer_length
	 && (maximum_transfer_length < (blocks_per_io * num_ios))) {
		CU_PASS("[SKIPPED] device too small for async_read test");
		goto out;
	}

	for (i = 0; i < num_ios; i++) {
		uint32_t lba = i * blocks_per_io;
		struct scsi_task *atask;

		atask = scsi_cdb_read10(lba, blocks_per_io * blocksize,
				       blocksize, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(atask);

		ret = scsi_task_add_data_in_buffer(atask,
						   blocks_per_io * blocksize,
						   &buf[lba * blocksize]);
		CU_ASSERT_EQUAL(ret, 0);

		ret = iscsi_scsi_command_async(sd->iscsi_ctx, sd->iscsi_lun,
					       atask, test_async_read_cb, NULL,
					       &state);
		CU_ASSERT_EQUAL(ret, 0);

		state.dispatched++;
		logging(LOG_VERBOSE, "READ10 dispatched: %d of %d (cmdsn=%d)",
			state.dispatched, num_ios, atask->cmdsn);
	}

	while (state.completed < state.dispatched) {
		struct pollfd pfd;

		pfd.fd = iscsi_get_fd(sd->iscsi_ctx);
		pfd.events = iscsi_which_events(sd->iscsi_ctx);

		ret = poll(&pfd, 1, -1);
		CU_ASSERT_NOT_EQUAL(ret, -1);

		ret = iscsi_service(sd->iscsi_ctx, pfd.revents);
		CU_ASSERT_EQUAL(ret, 0);
	}

out:
	free(buf);
}
