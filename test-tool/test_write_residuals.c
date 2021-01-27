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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"
#include "test_write_residuals.h"

bool command_is_implemented = true;

void
write_residuals_test(const struct residuals_test_data *tdata)
{
        struct iscsi_data data;
        struct scsi_task *task_ret;
        int ok;
        int scsi_status;
        unsigned int xfer_len_byte = 8;
        unsigned int i;
        unsigned int scsi_opcode_write = SCSI_OPCODE_WRITE10;
        const char *residual = tdata->residuals_kind == SCSI_RESIDUAL_OVERFLOW ? "overflow" : "underflow";

        switch (tdata->cdb_size) {
        case 10:
                scsi_opcode_write = SCSI_OPCODE_WRITE10;
                xfer_len_byte = 8;
                break;
        case 12:
                scsi_opcode_write = SCSI_OPCODE_WRITE12;
                xfer_len_byte = 9;
                break;
        case 16:
                scsi_opcode_write = SCSI_OPCODE_WRITE16;
                xfer_len_byte = 13;
                break;
        }

        if (tdata->check_overwrite) {
                logging(LOG_VERBOSE, "Write two blocks of 'a'");
                memset(scratch, 'a', (2 * block_size));

                switch (tdata->cdb_size) {
                case 10:
                        WRITE10(sd, 0, 2 * block_size, block_size, 0, 0, 0, 0, 0, scratch, EXPECT_STATUS_GOOD);
                        break;
                case 12:
                        WRITE12(sd, 0, 2 * block_size, block_size, 0, 0, 0, 0, 0, scratch, EXPECT_STATUS_GOOD);
                        break;
                case 16:
                        WRITE16(sd, 0, 2 * block_size, block_size, 0, 0, 0, 0, 0, scratch, EXPECT_STATUS_GOOD);
                        break;
                }

                logging(LOG_VERBOSE, "Write %u block(s) of 'b' but set iSCSI EDTL to %u block(s).",
                        tdata->xfer_len,
                        tdata->xfer_len % 2 + 1);
        }

        task = malloc(sizeof(*task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);
        memset(task, 0, sizeof(*task));

        if (tdata->check_overwrite) {
                memset(scratch, 'b', tdata->buf_len);
        } else {
                memset(scratch, 0xa6, tdata->buf_len);
        }

        task->cdb[0] = scsi_opcode_write;
        task->cdb[xfer_len_byte] = tdata->xfer_len;
        task->cdb_size = tdata->cdb_size;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = tdata->buf_len;
        data.size = task->expxferlen;
        data.data = &scratch[0];

        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun,
                                           task, tdata->buf_len == 0 ? NULL : &data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);
        CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_CANCELLED);

        if (task->status     == SCSI_STATUS_CHECK_CONDITION &&
            task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST &&
            task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
                logging(LOG_NORMAL, "[SKIPPED] WRITE%zu is not implemented.", tdata->cdb_size);
                command_is_implemented = false;
                scsi_free_scsi_task(task);
                task = NULL;
                return;
        }

        logging(LOG_VERBOSE, "Verify that target returns SUCCESS or INVALID "
                "FIELD IN INFORMATION UNIT");
        scsi_status = task->status;
        ok = task->status     == SCSI_STATUS_GOOD ||
            (task->status     == SCSI_STATUS_CHECK_CONDITION &&
             task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST &&
             task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_FIELD_IN_INFORMATION_UNIT);
        if (!ok) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT(ok);

        logging(LOG_VERBOSE, "Verify residual %s flag is set", residual);
        if (task->residual_status != tdata->residuals_kind) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "%s flag", residual);
        }
        CU_ASSERT_EQUAL(task->residual_status, tdata->residuals_kind);

        logging(LOG_VERBOSE, "Verify we got %zu bytes of residual %s",
                tdata->residuals_amount, residual);
        if (task->residual != tdata->residuals_amount) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        tdata->residuals_amount, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, tdata->residuals_amount);
        scsi_free_scsi_task(task);
        task = NULL;

        if (!tdata->check_overwrite) {
                return;
        }

        logging(LOG_VERBOSE, "Read the two blocks");
        switch (tdata->cdb_size) {
        case 10:
                READ10(sd, NULL, 0, 2* block_size, block_size, 0, 0, 0, 0, 0,
                       scratch, EXPECT_STATUS_GOOD);
                break;
        case 12:
                READ12(sd, NULL, 0, 2* block_size, block_size, 0, 0, 0, 0, 0,
                       scratch, EXPECT_STATUS_GOOD);
                break;
        case 16:
                READ16(sd, NULL, 0, 2* block_size, block_size, 0, 0, 0, 0, 0,
                       scratch, EXPECT_STATUS_GOOD);
                break;
        }

        /* According to FCP target could transfer no data and return 
           CHECK CONDITION status with the sense key set to 
           ILLEGAL REQUEST and the additional sense code set to 
           INVALID FIELD IN COMMAND INFORMATION UNIT; this check prevent
           false assert*/

        if (scsi_status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "Verify that blocks were NOT "
                        "overwritten and still contain 'a'");
                for (i = 0; i < 2 * block_size; i++) {
                        if (scratch[i] != 'a') {
                                logging(LOG_NORMAL, "Blocks were overwritten "
                                        "and no longer contain 'a'");
                                CU_FAIL("Blocks were incorrectly overwritten");
                                break;
                        }
                }
                return;
        }

        logging(LOG_VERBOSE, "Verify that the first block was changed to 'b'");
        for (i = 0; i < block_size; i++) {
                if (scratch[i] != 'b') {
                        logging(LOG_NORMAL, "First block did not contain "
                                "expected 'b'");
                        CU_FAIL("Block was not written correctly");
                        break;
                }
        }

        logging(LOG_VERBOSE, "Verify that the second block was NOT "
                "overwritten and still contains 'a'");
        for (i = block_size; i < 2 * block_size; i++) {
                if (scratch[i] != 'a') {
                        logging(LOG_NORMAL, "Second block was overwritten "
                                "and no longer contain 'a'");
                        CU_FAIL("Second block was incorrectly overwritten");
                        break;
                }
        }
        return;
}
