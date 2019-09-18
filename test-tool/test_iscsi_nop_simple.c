/*
   Copyright (C) 2019 SUSE LLC
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
#include <arpa/inet.h>
#include <CUnit/CUnit.h>
#include <poll.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

struct test_iscsi_nop_state {
        int dispatched;
        int completed;
        char nop_buf[1024];
        size_t nop_datalen;
};

static int
test_iscsi_nop_txrx(struct test_iscsi_nop_state *state)
{
        while (state->completed < state->dispatched) {
                struct pollfd pfd;
                int ret;

                pfd.fd = iscsi_get_fd(sd->iscsi_ctx);
                pfd.events = iscsi_which_events(sd->iscsi_ctx);

                ret = poll(&pfd, 1, -1);
                if (ret < 0) {
                        return ret;
                }

                ret = iscsi_service(sd->iscsi_ctx, pfd.revents);
                if (ret != 0) {
                        return ret;
                }
        }
        return 0;
}

static void
test_iscsi_nop_cb(struct iscsi_context *iscsi _U_, int status,
                  void *command_data, void *private_data)
{
        struct test_iscsi_nop_state *state = private_data;
        struct iscsi_data *data = command_data;

        CU_ASSERT_EQUAL(status, 0);
        CU_ASSERT_PTR_NOT_NULL(data);
        CU_ASSERT_EQUAL_FATAL(state->nop_datalen, data->size);
        if (state->nop_datalen > 0) {
                CU_ASSERT_EQUAL(0, memcmp(state->nop_buf, data->data,
                                          state->nop_datalen));
        }

        state->completed++;
}

void
test_iscsi_nop_simple(void)
{
        struct test_iscsi_nop_state state;
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Nop Out Pings");

        CHECK_FOR_ISCSI(sd);

        memset(&state, 0, sizeof(state));
        strncpy(state.nop_buf, "nopping", sizeof(state.nop_buf));
        state.nop_datalen = strlen(state.nop_buf) + 1;
        ret = iscsi_nop_out_async(sd->iscsi_ctx, test_iscsi_nop_cb,
                                  (unsigned char *)state.nop_buf,
                                  state.nop_datalen, &state);
        CU_ASSERT_EQUAL(ret, 0);
        state.dispatched++;

        ret = test_iscsi_nop_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);

        /* no data */
        state.nop_datalen = 0;
        ret = iscsi_nop_out_async(sd->iscsi_ctx, test_iscsi_nop_cb,
                                  NULL, state.nop_datalen, &state);
        CU_ASSERT_EQUAL(ret, 0);
        state.dispatched++;

        ret = test_iscsi_nop_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);
}
