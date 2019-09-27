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
#include <stdbool.h>
#include <arpa/inet.h>
#include <CUnit/CUnit.h>
#include <poll.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

struct test_iscsi_sendtargets_state {
        int dispatched;
        int completed;
        int failed;
        int succeeded;
        bool expect_failure;
};

static int
test_iscsi_sendtargets_txrx(struct test_iscsi_sendtargets_state *state)
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
test_iscsi_sendtargets_cb(struct iscsi_context *iscsi _U_, int status,
                          void *command_data _U_, void *private_data)
{
        struct test_iscsi_sendtargets_state *state = private_data;

        state->completed++;
        if (state->expect_failure) {
                CU_ASSERT_NOT_EQUAL(status, 0);
        } else {
                CU_ASSERT_EQUAL(status, 0);
        }

        if (status != 0) {
                logging(LOG_VERBOSE, "non-zero Text response %d",
                        state->completed);
                state->failed++;
                return;
        }
        state->succeeded++;
        logging(LOG_VERBOSE, "zero Text response %d", state->completed);
}

static void
test_iscsi_sendtargets_simple_cb(struct iscsi_context *iscsi, int status,
                                 void *command_data, void *private_data)
{
        struct iscsi_discovery_address *da;

        test_iscsi_sendtargets_cb(iscsi, status, command_data, private_data);
        for (da = command_data; da != NULL; da = da->next) {
                struct iscsi_target_portal *po;
                logging(LOG_VERBOSE, "Target: %s", da->target_name);
                for (po = da->portals; po != NULL; po = po->next) {
                        logging(LOG_VERBOSE, "+ Portal: %s", po->portal);
                }
        }
}

void
test_iscsi_sendtargets_simple(void)
{
        struct test_iscsi_sendtargets_state state;
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SendTargets in FFP");

        CHECK_FOR_ISCSI(sd);

        memset(&state, 0, sizeof(state));
        ret = iscsi_discovery_async(sd->iscsi_ctx,
                                    test_iscsi_sendtargets_simple_cb, &state);
        CU_ASSERT_EQUAL(ret, 0);
        state.dispatched++;

        state.expect_failure = false;
        ret = test_iscsi_sendtargets_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(state.failed, 0);
        CU_ASSERT_EQUAL(state.succeeded, 1);
}

int
test_iscsi_text_req_queue(struct iscsi_context *iscsi,
                          const char *kv_data,
                          iscsi_command_cb cb,
                          struct test_iscsi_sendtargets_state *state)
{
        struct iscsi_pdu *pdu;
        int ret;

        pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_TEXT_REQUEST,
                                 ISCSI_PDU_TEXT_RESPONSE,
                                 iscsi_itt_post_increment(iscsi),
                                 ISCSI_PDU_DROP_ON_RECONNECT);
        CU_ASSERT_PTR_NOT_NULL_FATAL(pdu);

        iscsi_pdu_set_immediate(pdu);
        iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);
        iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_TEXT_FINAL);
        iscsi_pdu_set_ttt(pdu, 0xffffffff);

        ret = iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)kv_data,
                                 strlen(kv_data) + 1);
        CU_ASSERT_EQUAL_FATAL(ret, 0);

        pdu->callback = cb;
        pdu->private_data = state;

        ret = iscsi_queue_pdu(iscsi, pdu);
        CU_ASSERT_EQUAL_FATAL(ret, 0);
        state->dispatched++;
        logging(LOG_VERBOSE, "queued Text request %d with %s",
                state->dispatched, kv_data);

        return 0;
}

void
test_iscsi_sendtargets_invalid(void)
{
        struct test_iscsi_sendtargets_state state;
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test invalid SendTargets Text requests");

        CHECK_FOR_ISCSI(sd);

        memset(&state, 0, sizeof(state));
        ret = test_iscsi_text_req_queue(sd->iscsi_ctx,
                                        "SendTargetsPlease=All", /* bad key */
                                        test_iscsi_sendtargets_cb,
                                        &state);
        CU_ASSERT_EQUAL(ret, 0);

        state.expect_failure = true;
        ret = test_iscsi_sendtargets_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);

        ret = test_iscsi_text_req_queue(sd->iscsi_ctx,
                                        "SendTargets=Alle", /* bad val */
                                        test_iscsi_sendtargets_cb,
                                        &state);
        CU_ASSERT_EQUAL(ret, 0);

        state.expect_failure = true;
        ret = test_iscsi_sendtargets_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);

        ret = test_iscsi_text_req_queue(sd->iscsi_ctx,
                                        "SendTargets=A", /* bad val */
                                        test_iscsi_sendtargets_cb,
                                        &state);
        CU_ASSERT_EQUAL(ret, 0);

        state.expect_failure = true;
        ret = test_iscsi_sendtargets_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);

        ret = test_iscsi_text_req_queue(sd->iscsi_ctx,
                                        "sENDtARGETS=aLL", /* bad case */
                                        test_iscsi_sendtargets_cb,
                                        &state);
        CU_ASSERT_EQUAL(ret, 0);

        state.expect_failure = true;
        ret = test_iscsi_sendtargets_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);

        ret = test_iscsi_text_req_queue(sd->iscsi_ctx,
                                        "SendTargets=All", /* valid */
                                        test_iscsi_sendtargets_cb,
                                        &state);
        CU_ASSERT_EQUAL(ret, 0);

        state.expect_failure = false;
        ret = test_iscsi_sendtargets_txrx(&state);
        CU_ASSERT_EQUAL(ret, 0);
        CU_ASSERT_EQUAL(state.failed, 4);
        CU_ASSERT_EQUAL(state.succeeded, 1);
}
