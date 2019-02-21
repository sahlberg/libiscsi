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
#include <inttypes.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_compareandwrite_simple(void)
{
        int i, n;
        unsigned j;
        int maxbl;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        if (inq_bl && inq_bl->max_cmp) {
                maxbl = inq_bl->max_cmp;
        } else {
                /* Assume we are not limited */
                maxbl = 256;
        }

        n = 256;
        if (n + 0U > num_blocks)
                n = num_blocks;
        if (maxbl + 0U > num_blocks)
                maxbl = num_blocks;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1-%d blocks at the "
                "start of the LUN", n);
        for (i = 1; i <= n; i++) {
                logging(LOG_VERBOSE, "Write %d blocks of 'A' at LBA:0", i);
                memset(scratch, 'A', 2 * i * block_size);
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITE16(sd, 0, i * block_size,
                        block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                if (i > maxbl) {
                        logging(LOG_VERBOSE, "Number of blocks %d is greater than "
                                "BlockLimits.MaximumCompareAndWriteLength(%d). "
                                "Command should fail with INVALID_FIELD_IN_CDB",
                                i, maxbl);
                        COMPAREANDWRITE(sd, 0,
                                        scratch, 2 * i * block_size,
                                        block_size, 0, 0, 0, 0,
                                        EXPECT_INVALID_FIELD_IN_CDB);
                        continue;
                }

                memset(scratch + i * block_size, 'B', i * block_size);

                logging(LOG_VERBOSE, "Overwrite %d blocks with 'B' "
                        "at LBA:0 (if they all contain 'A')", i);
                COMPAREANDWRITE(sd, 0,
                                scratch, 2 * i * block_size, block_size,
                                0, 0, 0, 0,
                                EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read %d blocks at LBA:0 and verify "
                        "they are all 'B'", i);
                READ16(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                for (j = 0; j < i * block_size; j++) {
                        if (scratch[j] != 'B') {
                                logging(LOG_VERBOSE, "[FAILED] Data did not "
                                        "read back as 'B' (scratch[%d] = %#02x)",
                                        j, scratch[j]);
                                CU_FAIL("Block was not written correctly");
                                return;
                        }
                }
        }


        logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1-%d blocks at the "
                "end of the LUN", n);
        for (i = 1; i <= n; i++) {
                logging(LOG_VERBOSE, "Write %d blocks of 'A' at LBA:%" PRIu64,
                        i, num_blocks - i);
                memset(scratch, 'A', 2 * i * block_size);
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITE16(sd, num_blocks - i, i * block_size,
                        block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                if (i > maxbl) {
                        logging(LOG_VERBOSE, "Number of blocks %d is greater than "
                                "BlockLimits.MaximumCompareAndWriteLength(%d). "
                                "Command should fail with INVALID_FIELD_IN_CDB",
                                i, maxbl);
                        COMPAREANDWRITE(sd, num_blocks - i,
                                        scratch, 2 * i * block_size,
                                        block_size, 0, 0, 0, 0,
                                        EXPECT_INVALID_FIELD_IN_CDB);
                        continue;
                }
                memset(scratch + i * block_size, 'B', i * block_size);

                logging(LOG_VERBOSE, "Overwrite %d blocks with 'B' "
                        "at LBA:%" PRIu64 " (if they all contain 'A')",
                        i, num_blocks - i);
                COMPAREANDWRITE(sd, num_blocks - i,
                                scratch, 2 * i * block_size, block_size,
                                0, 0, 0, 0,
                                EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read %d blocks at LBA:%" PRIu64 
                        " and verify they are all 'B'",
                        i, num_blocks - i);
                READ16(sd, NULL, num_blocks - i, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                for (j = 0; j < i * block_size; j++) {
                        if (scratch[j] != 'B') {
                                logging(LOG_VERBOSE, "[FAILED] Data did not "
                                        "read back as 'B' (scratch[%d] = %#02x)",
                                        j, scratch[j]);
                                CU_FAIL("Block was not written correctly");
                                return;
                        }
                }
        }
}
