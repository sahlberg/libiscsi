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
test_compareandwrite_miscompare(void)
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
                maxbl = 255;
        }

        n = 255;
        if (n + 0U > num_blocks)
                n = num_blocks;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1-%d blocks at the "
                "start of the LUN. One Byte miscompare in the final block.",
                n);
        for (i = 1; i <= n; i++) {
                logging(LOG_VERBOSE, "Write %d blocks of 'A' at LBA:0", i);
                memset(scratch, 'A', 2 * i * block_size);
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITE16(sd, 0, i * block_size,
                        block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);
                
                logging(LOG_VERBOSE, "Change byte 27 from the end to 'C' so that it does not match.");
                scratch[i * block_size - 27] = 'C';

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
                                EXPECT_MISCOMPARE);

                logging(LOG_VERBOSE, "Read %d blocks at LBA:0 and verify "
                        "they are still unchanged as 'A'", i);
                READ16(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                for (j = 0; j < i * block_size; j++) {
                        if (scratch[j] != 'A') {
                                logging(LOG_VERBOSE, "[FAILED] Data changed "
                                        "eventhough there was a miscompare");
                                CU_FAIL("Block was written to");
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

                logging(LOG_VERBOSE, "Change byte 27 from the end to 'C' so that it does not match.");
                scratch[i * block_size - 27] = 'C';


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
                        "at LBA:%" PRIu64 " (if they all contain 'A')",
                        i, num_blocks - i);
                COMPAREANDWRITE(sd, num_blocks - i,
                                scratch, 2 * i * block_size, block_size,
                                0, 0, 0, 0,
                                EXPECT_MISCOMPARE);

                logging(LOG_VERBOSE, "Read %d blocks at LBA:%" PRIu64 
                        "they are still unchanged as 'A'",
                        i, num_blocks - i);
                READ16(sd, NULL, num_blocks - i, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                for (j = 0; j < i * block_size; j++) {
                        if (scratch[j] != 'A') {
                                logging(LOG_VERBOSE, "[FAILED] Data changed "
                                        "eventhough there was a miscompare");
                                CU_FAIL("Block was written to");
                                return;
                        }
                }
        }
}

void
test_compareandwrite_miscompare_sense(void)
{
        unsigned i;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;
        CHECK_FOR_ISCSI(sd);

        if (inq_bl->max_cmp < 1) {
                logging(LOG_NORMAL, "[SKIPPED] COMPAREANDWRITE "
                        "max_cmp less than 1.");
                CU_PASS("[SKIPPED] single block COMPAREANDWRITE not supported "
                        "Skipping test");
                return;
        }

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test COMPARE_AND_WRITE of 1 block at the "
                "start of the LUN");

        logging(LOG_VERBOSE, "Write 1 block of 'A' at LBA:0");
        memset(scratch, 'A', 2 * block_size);

        WRITE16(sd, 0, block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        memset(scratch + block_size, 'B', block_size);

        logging(LOG_VERBOSE, "Overwrite blocks with 'B' "
                "at LBA:0 (if they all contain 'A')");
        COMPAREANDWRITE(sd, 0,
                        scratch, 2 * block_size, block_size,
                        0, 0, 0, 0,
                        EXPECT_STATUS_GOOD);
        /* we've confirmed that c&w is supported, time for the proper test... */

        logging(LOG_VERBOSE, "Vary location of miscompare in %zd bytes and check"
                "sense", block_size);
        memset(scratch + block_size, 'C', block_size);

        for (i = 0; i < block_size; i++) {
                struct scsi_task *tsk;
                struct scsi_iovec iov;

                logging(LOG_VERBOSE, "Fill buffer with 'B' except for %d "
                        "offset", i);
                memset(scratch, 'B', block_size);
                scratch[i] = 'Z';

                tsk = scsi_cdb_compareandwrite(0, 2 * block_size, block_size,
                                                0, 0, 0, 0, 0);
                CU_ASSERT(tsk != NULL);

                iov.iov_base = scratch;
                iov.iov_len  = 2 * block_size;
                scsi_task_set_iov_out(tsk, &iov, 1);

                tsk = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun,
                                               tsk, NULL);
                CU_ASSERT_FATAL(tsk != NULL);
                CU_ASSERT(tsk->status == SCSI_STATUS_CHECK_CONDITION);
                CU_ASSERT(tsk->sense.key == SCSI_SENSE_MISCOMPARE);
                CU_ASSERT(tsk->sense.ascq
                                == SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY);
                if (tsk->sense.info_valid) {
                        logging(LOG_VERBOSE, "Check Information field provided"
                                " with miscompare sense response");
                        CU_ASSERT_EQUAL(tsk->sense.information, i);
                }

                scsi_free_scsi_task(tsk);
        }
}
