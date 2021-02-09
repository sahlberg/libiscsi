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

#include <assert.h>
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
        struct task_status status;
        int ok;
        unsigned int expected_write_size;
        unsigned int max_len;
        unsigned int xfer_len_byte = 8;
        unsigned int i;
        unsigned int transfer_length;
        unsigned int scsi_opcode_write = SCSI_OPCODE_WRITE10;
        const char *residual = tdata->residual_type == SCSI_RESIDUAL_OVERFLOW ? "overflow" : "underflow";

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
        default:
                assert(false);
        }

        if (tdata->xfer_len * block_size > tdata->buf_len) /* SPDTL > EDTL */ {
                /* Transfer has to be truncated up to EDTL */
                expected_write_size = tdata->buf_len;
                max_len = (tdata->xfer_len * block_size);
        } else /* SPDTL <= EDTL */ {
                /* Transfer has to be truncated up to SPDTL */
                expected_write_size = (tdata->xfer_len * block_size);
                max_len = tdata->buf_len;
        }
        transfer_length = DIV_ROUND_UP(max_len, block_size) * block_size;

        logging(LOG_VERBOSE, "Write %ld block(s) of 'a'", transfer_length / block_size);
        memset(scratch, 'a', transfer_length);

        switch (tdata->cdb_size) {
        case 10:
                WRITE10(sd, 0, transfer_length, block_size, 0, 0, 0, 0, 0,
                        scratch, EXPECT_STATUS_GOOD);
                break;
        case 12:
                WRITE12(sd, 0, transfer_length, block_size, 0, 0, 0, 0, 0,
                        scratch, EXPECT_STATUS_GOOD);
                break;
        case 16:
                WRITE16(sd, 0, transfer_length, block_size, 0, 0, 0, 0, 0,
                        scratch, EXPECT_STATUS_GOOD);
                break;
        default:
                assert(false);
        }

        task = malloc(sizeof(*task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);
        memset(task, 0, sizeof(*task));

        logging(LOG_VERBOSE, "Write 'b' with the transfer length "
                "being set to %d block(s)", tdata->xfer_len);
        memset(scratch, 'b', tdata->buf_len);

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

        status.status     = task->status;
        status.sense.key  = task->sense.key;
        status.sense.ascq = task->sense.ascq;

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
        if (task->residual_status != tdata->residual_type) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "%s flag", residual);
        }
        CU_ASSERT_EQUAL(task->residual_status, tdata->residual_type);

        logging(LOG_VERBOSE, "Verify we got %zu bytes of residual %s",
                tdata->residual, residual);
        if (task->residual != tdata->residual) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        tdata->residual, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, tdata->residual);
        scsi_free_scsi_task(task);
        task = NULL;

        logging(LOG_VERBOSE, "Read %ld block(s)", transfer_length / block_size);
        switch (tdata->cdb_size) {
        case 10:
                READ10(sd, NULL, 0, transfer_length, block_size,
                       0, 0, 0, 0, 0, scratch, EXPECT_STATUS_GOOD);
                break;
        case 12:
                READ12(sd, NULL, 0, transfer_length, block_size,
                       0, 0, 0, 0, 0, scratch, EXPECT_STATUS_GOOD);
                break;
        case 16:
                READ16(sd, NULL, 0, transfer_length, block_size,
                       0, 0, 0, 0, 0, scratch, EXPECT_STATUS_GOOD);
                break;
        default:
                assert(false);
        }

        /* According to FCP-4:
           If the command requested that data beyond the length specified by 
           the FCP_DL field be transferred, then the device server shall set 
           the FCP_RESID_OVER bit (see 9.5.8) to one in the FCP_RSP IU and:

           a) process the command normally except that data beyond the FCP_DL 
              count shall not be requested or transferred; */

        if (status.status == SCSI_STATUS_GOOD) {

                switch (tdata->residual_type) {
                case SCSI_RESIDUAL_OVERFLOW:
                        logging(LOG_VERBOSE, "Verify that if iSCSI EDTL < SCSI TL "
                                "then we only write iSCSI EDTL amount of data");
                        break;
                case SCSI_RESIDUAL_UNDERFLOW:
                        logging(LOG_VERBOSE, "Verify that if iSCSI EDTL > SCSI TL "
                                "then we only write SCSI TL amount of data");
                        break;
                case SCSI_RESIDUAL_NO_RESIDUAL:
                        assert(false);
                }

                logging(LOG_VERBOSE, "Verify that the first %d bytes were "
                        "changed to 'b'", expected_write_size);
                for (i = 0; i < expected_write_size; i++) {
                        if (scratch[i] != 'b') {
                                logging(LOG_NORMAL, "Blocks did not contain "
                                        "expected 'b'");
                                CU_FAIL("Blocks was not written correctly");
                                break;
                        }
                }

        /* b) transfer no data and return CHECK CONDITION status with the sense 
              key set to ILLEGAL REQUEST and the additional sense code set to 
              INVALID FIELD IN COMMAND INFORMATION UNIT; */

        } else if (status.status     == SCSI_STATUS_CHECK_CONDITION &&
                   status.sense.key  == SCSI_SENSE_ILLEGAL_REQUEST &&
                   status.sense.ascq == SCSI_SENSE_ASCQ_INVALID_FIELD_IN_INFORMATION_UNIT) {

                logging(LOG_VERBOSE, "Verify that first %d bytes were NOT "
                        "overwritten and still contain 'a'", expected_write_size);
                for (i = 0; i < expected_write_size; i++) {
                        if (scratch[i] != 'a') {
                                logging(LOG_NORMAL, "Blocks were overwritten "
                                        "and no longer contain 'a'");
                                CU_FAIL("Blocks were incorrectly overwritten");
                                break;
                        }
                }
        }

        /* c) may transfer data and return CHECK CONDITION status with the 
              sense key set to ABORTED COMMAND and the additional sense code 
              set to INVALID FIELD IN COMMAND INFORMATION UNIT.

             (not implemented yet) */

        /* Regardless of the executed target scenario, data beyond expected 
           truncation point should not be overwritten */

        logging(LOG_VERBOSE, "Verify that the last %ld bytes were NOT "
                "overwritten and still contain 'a'", tdata->residual);
        for (i = expected_write_size; i < max_len; i++) {
                if (scratch[i] != 'a') {
                        logging(LOG_NORMAL, "Data was overwritten "
                                "and no longer contain 'a'");
                        CU_FAIL("Data was incorrectly overwritten");
                        break;
                }
        }
        return;
}
