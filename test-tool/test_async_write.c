/*
   Copyright (C) SUSE LLC 2016-2020

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

struct tests_async_write_state {
	uint32_t io_dispatched;
	uint32_t io_completed;
	uint32_t prev_cmdsn;
	uint32_t logout_sent;
	uint32_t logout_cmpl;
};

static void
test_async_write_cb(struct iscsi_context *iscsi __attribute__((unused)),
		   int status, void *command_data, void *private_data)
{
	struct scsi_task *atask = command_data;
	struct tests_async_write_state *state = private_data;

	if (state->logout_cmpl) {
		CU_ASSERT_EQUAL(status, SCSI_STATUS_CANCELLED);
		logging(LOG_VERBOSE, "WRITE10 cancelled after logout");
		return;
	}

	state->io_completed++;
	logging(LOG_VERBOSE, "WRITE10 completed: %d of %d (CmdSN=%d)",
		state->io_completed, state->io_dispatched, atask->cmdsn);
	CU_ASSERT_NOT_EQUAL(status, SCSI_STATUS_CHECK_CONDITION);

	if ((state->io_completed > 1)
	 && (atask->cmdsn != state->prev_cmdsn + 1)) {
		logging(LOG_VERBOSE,
			"out of order completion (CmdSN=%d, prev=%d)",
			atask->cmdsn, state->prev_cmdsn);
	}
	state->prev_cmdsn = atask->cmdsn;

	scsi_free_scsi_task(atask);
}

void
test_async_write(void)
{
	int i, ret;
	struct tests_async_write_state state = { };
	int blocks_per_io = 8;
	int num_ios = 1000;
	/* IOs in flight concurrently, but all using the same src buffer */
	unsigned char *buf;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;
	CHECK_FOR_ISCSI(sd);

	if (maximum_transfer_length
	 && (maximum_transfer_length < (blocks_per_io * num_ios))) {
		CU_PASS("[SKIPPED] device too small for async_write test");
		return;
	}

	buf = calloc(block_size, blocks_per_io);
	CU_ASSERT(buf != NULL);
	if (!buf)
		return;

	for (i = 0; i < num_ios; i++) {
		uint32_t lba = i * blocks_per_io;
		struct scsi_task *atask;

		atask = scsi_cdb_write10(lba, blocks_per_io * block_size,
					 block_size, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(atask);

		ret = scsi_task_add_data_out_buffer(atask,
						    blocks_per_io * block_size,
						    buf);
		CU_ASSERT_EQUAL(ret, 0);

		ret = iscsi_scsi_command_async(sd->iscsi_ctx, sd->iscsi_lun,
					       atask, test_async_write_cb, NULL,
					       &state);
		CU_ASSERT_EQUAL(ret, 0);

		state.io_dispatched++;
		logging(LOG_VERBOSE, "WRITE10 dispatched: %d of %d (cmdsn=%d)",
			state.io_dispatched, num_ios, atask->cmdsn);
	}

	while (state.io_completed < state.io_dispatched) {
		struct pollfd pfd;

		pfd.fd = iscsi_get_fd(sd->iscsi_ctx);
		pfd.events = iscsi_which_events(sd->iscsi_ctx);

		ret = poll(&pfd, 1, -1);
		CU_ASSERT_NOT_EQUAL(ret, -1);

		ret = iscsi_service(sd->iscsi_ctx, pfd.revents);
		CU_ASSERT_EQUAL(ret, 0);
	}

	free(buf);
}

static void
test_async_io_logout_cb(struct iscsi_context *iscsi __attribute__((unused)),
			int status, void *command_data __attribute__((unused)),
			void *private_data)
{
	struct tests_async_write_state *state = private_data;

	state->logout_cmpl++;
	logging(LOG_VERBOSE, "Logout completed with %d IOs outstanding",
		state->io_dispatched - state->io_completed);
	CU_ASSERT_EQUAL(status, SCSI_STATUS_GOOD);
}

void
test_async_io_logout(void)
{
	int i, ret;
	struct tests_async_write_state state = { };
	int blocks_per_io = 8;
	int num_ios = 10;
	/* IOs in flight concurrently, but all using the same src buffer */
	unsigned char *buf;

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;
	CHECK_FOR_ISCSI(sd);

	if (maximum_transfer_length
	 && maximum_transfer_length < (blocks_per_io * num_ios)) {
		CU_PASS("[SKIPPED] device too small for async IO test");
		return;
	}

	buf = calloc(block_size, blocks_per_io);
	CU_ASSERT(buf != NULL);
	if (!buf)
		return;

	iscsi_set_noautoreconnect(sd->iscsi_ctx, 1);

	for (i = 0; i < num_ios; i++) {
		uint32_t lba = i * blocks_per_io;
		struct scsi_task *atask;

		atask = scsi_cdb_write10(lba, blocks_per_io * block_size,
					 block_size, 0, 0, 0, 0, 0);
		CU_ASSERT_PTR_NOT_NULL_FATAL(atask);

		ret = scsi_task_add_data_out_buffer(atask,
						    blocks_per_io * block_size,
						    buf);
		CU_ASSERT_EQUAL(ret, 0);

		ret = iscsi_scsi_command_async(sd->iscsi_ctx, sd->iscsi_lun,
					       atask, test_async_write_cb, NULL,
					       &state);
		CU_ASSERT_EQUAL(ret, 0);

		state.io_dispatched++;
		logging(LOG_VERBOSE, "WRITE10 dispatched: %d of %d (cmdsn=%d)",
			state.io_dispatched, num_ios, atask->cmdsn);
	}

	while (!state.logout_cmpl) {
		struct pollfd pfd;

		pfd.fd = iscsi_get_fd(sd->iscsi_ctx);
		pfd.events = iscsi_which_events(sd->iscsi_ctx);

		ret = poll(&pfd, 1, -1);
		CU_ASSERT_NOT_EQUAL(ret, -1);

		ret = iscsi_service(sd->iscsi_ctx, pfd.revents);
		CU_ASSERT_EQUAL(ret, 0);

		/* attempt logout after one of the dispatch IOs has completed */
		if (!state.logout_sent && state.io_completed > 0) {
			ret = iscsi_logout_async(sd->iscsi_ctx,
						 test_async_io_logout_cb,
						 &state);
			CU_ASSERT_EQUAL(ret, 0);

			state.logout_sent++;
			logging(LOG_VERBOSE,
				"Logout dispatched following %d IO completions",
				state.io_completed);
		}
	}

	iscsi_destroy_context(sd->iscsi_ctx);
	sd->iscsi_ctx = iscsi_context_login(initiatorname1, sd->iscsi_url, &sd->iscsi_lun);
	CU_ASSERT_PTR_NOT_NULL(sd->iscsi_ctx);
	free(buf);
}
