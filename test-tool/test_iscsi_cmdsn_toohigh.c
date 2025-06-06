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
#include <arpa/inet.h>
#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

static int change_cmdsn;

static void my_iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
        switch (change_cmdsn) {
        case 1:
                /*
		 * change the cmdsn so it becomes too big. TODO: use
		 * iscsi_pdu_set_cmdsn(), which also changes pdu->cmdsn?
		 */
                scsi_set_uint32(&pdu->outdata.data[24], iscsi->maxcmdsn + 1);
                /*
                 * Fudge the cmdsn value back to where it should be if this pdu
                 * is ignored. The cmdsn value will be incremented by the
                 * orig_queue_pdu(), so it's off by one.
                 */
                iscsi->cmdsn = iscsi->expcmdsn - 1;
                break;
        }

        change_cmdsn = 0;
        orig_queue_pdu(iscsi, pdu);
}

void test_iscsi_cmdsn_toohigh(void)
{
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test sending invalid iSCSI CMDSN");
        logging(LOG_VERBOSE, "CMDSN MUST be in the range EXPCMDSN and MAXCMDSN");

        CHECK_FOR_ISCSI(sd);

        logging(LOG_VERBOSE, "RFC3720:3.2.2.1 CMDSN > MAXCMDSN must be silently ignored by the target");
        logging(LOG_VERBOSE, "Send a TESTUNITREADY with CMDSN == MAXCMDSN+1. Should be ignored by the target.");

        sd->iscsi_ctx->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
        sd->iscsi_ctx->target_max_recv_data_segment_length = block_size;
        /* override transport queue_pdu callback for PDU manipulation */
        sd->iscsi_ctx->drv->queue_pdu = my_iscsi_queue_pdu;
        change_cmdsn = 1;
        /* we don't want autoreconnect since some targets will incorrectly
         * drop the connection on this condition.
         */
        iscsi_set_noautoreconnect(sd->iscsi_ctx, 1);
        iscsi_set_timeout(sd->iscsi_ctx, 3);

        ret = testunitready(sd,
                            EXPECT_STATUS_TIMEOUT);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret == 0) {
                logging(LOG_VERBOSE, "[SUCCESS] We did not receive a reply");
        } else {
                logging(LOG_VERBOSE, "[FAILURE] We got a response from the target but SMDSN was outside of the window.");
        }

        iscsi_set_noautoreconnect(sd->iscsi_ctx, 0);
        logging(LOG_VERBOSE, "Send a TESTUNITREADY with CMDSN == EXPCMDSN. should work again");
        TESTUNITREADY(sd,
                      EXPECT_STATUS_GOOD);
}
