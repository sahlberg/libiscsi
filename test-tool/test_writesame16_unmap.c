/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
test_writesame16_unmap(void)
{
        unsigned int i;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_LBPWS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITESAME16 of 1-256 blocks at the start of the LUN");
        for (i = 1; i <= 256; i++) {
                logging(LOG_VERBOSE, "Write %d blocks of 0xFF", i);
                memset(scratch, 0xff, i * block_size);
                WRITE16(sd, 0, i * block_size, block_size,
                        0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16", i);
                memset(scratch, 0, block_size);
                WRITESAME16(sd, 0, block_size, i, 0, 1, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);

                if (rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ is set. Read the unmapped "
                                "blocks back and verify they are all zero");
                        logging(LOG_VERBOSE, "Read %d blocks and verify they "
                                "are now zero", i);
                        READ16(sd, NULL, 0, i * block_size, block_size,
                               0, 0, 0, 0, 0, scratch,
                               EXPECT_STATUS_GOOD);
                        ALL_ZERO(scratch, i * block_size);
                } else {
                        logging(LOG_VERBOSE, "LBPRZ is clear. Skip the read "
                                "and verify zero test");
                }
        }


        logging(LOG_VERBOSE, "Test WRITESAME16 of 1-256 blocks at the end of the LUN");
        for (i = 1; i <= 256; i++) {
                logging(LOG_VERBOSE, "Write %d blocks of 0xFF", i);
                memset(scratch, 0xff, i * block_size);
                WRITE16(sd, num_blocks - i,
                        i * block_size, block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16", i);
                memset(scratch, 0, block_size);
                WRITESAME16(sd, num_blocks - i,
                            block_size, i, 0, 1, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);

                if (rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ is set. Read the unmapped "
                                "blocks back and verify they are all zero");
                        logging(LOG_VERBOSE, "Read %d blocks and verify they "
                                "are now zero", i);
                        READ16(sd, NULL, num_blocks - i,
                               i * block_size, block_size,
                               0, 0, 0, 0, 0, scratch,
                               EXPECT_STATUS_GOOD);
                        ALL_ZERO(scratch, i * block_size);
                } else {
                        logging(LOG_VERBOSE, "LBPRZ is clear. Skip the read "
                                "and verify zero test");
                }
        }

        logging(LOG_VERBOSE, "Verify that WRITESAME16 ANCHOR==1 + UNMAP==0 is invalid");
        WRITESAME16(sd, 0, block_size, 1, 1, 0, 0, 0, scratch,
                    EXPECT_INVALID_FIELD_IN_CDB);

        if (inq_lbp->anc_sup) {
                logging(LOG_VERBOSE, "Test WRITESAME16 ANCHOR==1 + UNMAP==0");
                memset(scratch, 0, block_size);
                WRITESAME16(sd, 0, block_size, 1, 1, 1, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);
        } else {
                logging(LOG_VERBOSE, "Test WRITESAME16 ANCHOR==1 + UNMAP==0 no ANC_SUP so expecting to fail");
                WRITESAME16(sd, 0, block_size, 1, 1, 1, 0, 0, scratch,
                            EXPECT_INVALID_FIELD_IN_CDB);
        }

        if (inq_bl == NULL) {
                logging(LOG_VERBOSE, "[FAILED] WRITESAME16 works but "
                        "BlockLimits VPD is missing.");
                CU_FAIL("[FAILED] WRITESAME16 works but "
                        "BlockLimits VPD is missing.");
                return;
        }

        i = 256;
        if (i <= num_blocks
            && (inq_bl->max_ws_len == 0 || inq_bl->max_ws_len >= i)) {
                logging(LOG_VERBOSE, "Block Limits VPD page reports MAX_WS_LEN "
                        "as either 0 (==no limit) or >= %d. Test Unmapping "
                        "%d blocks to verify that it can handle 2-byte "
                        "lengths", i, i);

                logging(LOG_VERBOSE, "Write %d blocks of 0xFF", i);
                memset(scratch, 0xff, i * block_size);
                WRITE16(sd, 0,
                        i * block_size, block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16", i);
                memset(scratch, 0, block_size);
                WRITESAME16(sd, 0, block_size, i, 0, 1, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);

                if (rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ is set. Read the unmapped "
                                "blocks back and verify they are all zero");

                        logging(LOG_VERBOSE, "Read %d blocks and verify they "
                                "are now zero", i);
                        READ16(sd, NULL, 0, i * block_size, block_size,
                               0, 0, 0, 0, 0, scratch,
                               EXPECT_STATUS_GOOD);
                        ALL_ZERO(scratch, i * block_size);
                } else {
                        logging(LOG_VERBOSE, "LBPRZ is clear. Skip the read "
                                "and verify zero test");
                }
        } else if (i <= num_blocks) {
                logging(LOG_VERBOSE, "Block Limits VPD page reports MAX_WS_LEN "
                        "as <256. Verify that a 256 block unmap fails with "
                        "INVALID_FIELD_IN_CDB.");

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16", i);
                WRITESAME16(sd, 0, block_size, i, 0, 1, 0, 0, scratch,
                            EXPECT_INVALID_FIELD_IN_CDB);
        }


        i = 65536;
        if (i <= num_blocks
            && (inq_bl->max_ws_len == 0 || inq_bl->max_ws_len >= i)) {
                logging(LOG_VERBOSE, "Block Limits VPD page reports MAX_WS_LEN "
                        "as either 0 (==no limit) or >= %d. Test Unmapping "
                        "%d blocks to verify that it can handle 4-byte "
                        "lengths", i, i);

                logging(LOG_VERBOSE, "Write %d blocks of 0xFF", i);
                memset(scratch, 0xff, i * block_size);
                WRITE16(sd, 0,
                        i * block_size, block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16", i);
                memset(scratch, 0, block_size);
                WRITESAME16(sd, 0, block_size, i, 0, 1, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);

                if (rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ is set. Read the unmapped "
                                "blocks back and verify they are all zero");

                        logging(LOG_VERBOSE, "Read %d blocks and verify they "
                                "are now zero", i);
                        READ16(sd, NULL, 0, i * block_size, block_size,
                               0, 0, 0, 0, 0, scratch,
                               EXPECT_STATUS_GOOD);
                        ALL_ZERO(scratch, i * block_size);
                } else {
                        logging(LOG_VERBOSE, "LBPRZ is clear. Skip the read "
                                "and verify zero test");
                }
        } else if (i <= num_blocks) {
                logging(LOG_VERBOSE, "Block Limits VPD page reports MAX_WS_LEN "
                        "as <256. Verify that a 256 block unmap fails with "
                        "INVALID_FIELD_IN_CDB.");

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16", i);
                WRITESAME16(sd, 0, block_size, i, 0, 1, 0, 0, scratch,
                            EXPECT_INVALID_FIELD_IN_CDB);
        }
}
