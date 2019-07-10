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

struct test_mpio_async_caw_state {
	uint32_t dispatched;
	uint32_t completed;
	uint32_t mismatches;
};

static void
test_mpio_async_caw_cb(struct iscsi_context *iscsi __attribute__((unused)),
		       int status, void *command_data, void *private_data)
{
	struct scsi_task *atask = command_data;
	struct test_mpio_async_caw_state *state = private_data;

	state->completed++;
	if (status == SCSI_STATUS_CHECK_CONDITION) {
		CU_ASSERT_EQUAL(atask->sense.key, SCSI_SENSE_MISCOMPARE);
		CU_ASSERT_EQUAL(atask->sense.ascq,
				SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY);
		state->mismatches++;
		logging(LOG_VERBOSE, "COMPARE_AND_WRITE mismatch: %d of %d "
			"(CmdSN=%d)",
			state->completed, state->dispatched, atask->cmdsn);

	} else {
		logging(LOG_VERBOSE, "COMPARE_AND_WRITE success: %d of %d "
			"(CmdSN=%d)",
			state->completed, state->dispatched, atask->cmdsn);
	}

	scsi_free_scsi_task(atask);
}

static void
test_mpio_async_caw_init_bufs(unsigned char *cmp_buf, unsigned char *wr_buf,
			      int blocksize, int num_mp_sds)
{
	int sd_i;

	/*
	 * Each compare and write attempts to modify on-disk data with the
	 * assumption that the previous operation was successful. E.g.
	 *
	 * session 0		session 1
	 * ---------            ---------
	 * 0->1 (good)
	 *			1->0 (good)
	 *
	 * This gives us some nice races if the target processes the requests
	 * out of order. E.g.
	 * 0->1 (good)
	 *			1->0 (good)
	 *			1->0 (mismatch!)
	 * 0->1 (good)
	 */

	for (sd_i = 0; sd_i < num_mp_sds; sd_i++) {
		int wr_val;
		int cmp_val = sd_i;

		if (sd_i == num_mp_sds - 1) {
			wr_val = 0;
		} else {
			wr_val = sd_i + 1;
		}

		memset(&cmp_buf[sd_i * blocksize], cmp_val, blocksize);
		memset(&wr_buf[sd_i * blocksize], wr_val, blocksize);
	}
}

void
test_mpio_async_caw(void)
{
	int i, ret;
	int sd_i;
	struct test_mpio_async_caw_state state = { 0, 0, 0 };
	int num_ios = 1000;
	uint32_t lba = 0;
	unsigned char cmp_buf[block_size * mp_num_sds];
	unsigned char wr_buf[block_size * mp_num_sds];

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;
	MPATH_SKIP_IF_UNAVAILABLE(mp_sds, mp_num_sds);
	MPATH_SKIP_UNLESS_ISCSI(mp_sds, mp_num_sds);

	/* synchronously initialise zeros for first CAW */
	memset(wr_buf, 0, block_size);
	WRITESAME10(mp_sds[0], 0, block_size, 1, 0, 0, 0, 0, wr_buf,
		    EXPECT_STATUS_GOOD);

	test_mpio_async_caw_init_bufs(cmp_buf, wr_buf, block_size, mp_num_sds);

	for (i = 0; i < num_ios; i++) {
		/* queue a one-block CAW task on each MPIO sessions */
		for (sd_i = 0; sd_i < mp_num_sds; sd_i++) {
			struct scsi_task *atask;
			int buf_off = sd_i * block_size;

			atask = scsi_cdb_compareandwrite(lba, block_size * 2,
							 block_size,
							 0, 0, 0, 0, 0);
			CU_ASSERT_PTR_NOT_NULL_FATAL(atask);

			/* compare data is first, followed by write data */
			ret = scsi_task_add_data_out_buffer(atask,
							    block_size,
							    &cmp_buf[buf_off]);
			CU_ASSERT_EQUAL(ret, 0);

			ret = scsi_task_add_data_out_buffer(atask,
							    block_size,
							    &wr_buf[buf_off]);
			CU_ASSERT_EQUAL(ret, 0);

			ret = iscsi_scsi_command_async(mp_sds[sd_i]->iscsi_ctx,
						       mp_sds[sd_i]->iscsi_lun,
						       atask,
						       test_mpio_async_caw_cb,
						       NULL, &state);
			CU_ASSERT_EQUAL(ret, 0);

			state.dispatched++;
			logging(LOG_VERBOSE, "COMPARE_AND_WRITE dispatched: "
				"%d of %d (cmdsn=%d)",
				state.dispatched, num_ios, atask->cmdsn);
		}
	}

	while (state.completed < state.dispatched) {
		struct pollfd pfd[mp_num_sds];

		for (sd_i = 0; sd_i < mp_num_sds; sd_i++) {
			pfd[sd_i].fd = iscsi_get_fd(mp_sds[sd_i]->iscsi_ctx);
			pfd[sd_i].events
				= iscsi_which_events(mp_sds[sd_i]->iscsi_ctx);
		}

		ret = poll(pfd, mp_num_sds, -1);
		CU_ASSERT_NOT_EQUAL(ret, -1);

		for (sd_i = 0; sd_i < mp_num_sds; sd_i++) {
			if (!pfd[sd_i].revents) {
				continue;
			}
			ret = iscsi_service(mp_sds[sd_i]->iscsi_ctx,
					    pfd[sd_i].revents);
			CU_ASSERT_EQUAL(ret, 0);
		}
	}

	logging(LOG_VERBOSE, "[OK] All %d COMPARE_AND_WRITE IOs complete, with "
		"%d mismatch(es)", state.completed, state.mismatches);
}
