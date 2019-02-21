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
#include <stdlib.h>
#include <string.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"


void
test_read16_residuals(void)
{
        struct scsi_task *task_ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test READ16 commands with residuals");
        logging(LOG_VERBOSE, "Block size is %zu", block_size);

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This READ16 test is only "
                        "supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_READ16;
        task->cdb[13] = 1;
        task->cdb_size = 16;
        task->xfer_dir = SCSI_XFER_READ;
        task->expxferlen = 0;

        /*
         * we don't want autoreconnect since some targets will drop the session
         * on this condition.
         */
        iscsi_set_noautoreconnect(sd->iscsi_ctx, 1);


        logging(LOG_VERBOSE, "Try reading one block but with iSCSI expected transfer length==0");

        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, NULL);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);
        CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_CANCELLED); /* XXX redundant? */

        if (task->status        == SCSI_STATUS_CHECK_CONDITION
            && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
            && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
                logging(LOG_NORMAL, "[SKIPPED] READ16 is not implemented on this target and it does not claim SBC-3 support.");
                CU_PASS("READ16 is not implemented and no SBC-3 support claimed.");
                return;
        }        
        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        if (task->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify residual overflow flag is set");
        if (task->residual_status != SCSI_RESIDUAL_OVERFLOW) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "overflow flag");
        }
        CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);

        logging(LOG_VERBOSE, "Verify we got %zu bytes of residual overflow",
                block_size);
        if (task->residual != block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        block_size, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, block_size);
        scsi_free_scsi_task(task);
        task = NULL;

        /* in case the previous test failed the session */
        iscsi_set_noautoreconnect(sd->iscsi_ctx, 0);


        logging(LOG_VERBOSE, "Try reading one block but with iSCSI expected transfer length==10000");
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_READ16;
        task->cdb[13] = 1;
        task->cdb_size = 16;
        task->xfer_dir = SCSI_XFER_READ;
        task->expxferlen = 10000;

        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, NULL);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        if (task->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify we got one whole block back from the target");
        if (task->datain.size != (int)block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target returned %u bytes "
                        "of data but should have returned %zu bytes.",
                        task->datain.size,block_size);
        }
        CU_ASSERT_EQUAL(task->datain.size, (int)block_size);

        logging(LOG_VERBOSE, "Verify residual underflow flag is set");
        if (task->residual_status != SCSI_RESIDUAL_UNDERFLOW) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "underflow flag");
        }
        CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_UNDERFLOW);

        logging(LOG_VERBOSE, "Verify we got %zu bytes of residual underflow",
                10000 - block_size);
        if (task->residual != 10000 - block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        10000 - block_size, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, 10000 - block_size);
        scsi_free_scsi_task(task);
        task = NULL;


        logging(LOG_VERBOSE, "Try reading one block but with iSCSI expected transfer length==200");
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_READ16;
        task->cdb[13] = 1;
        task->cdb_size = 16;
        task->xfer_dir = SCSI_XFER_READ;
        task->expxferlen = 200;

        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, NULL);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        if (task->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify we got 200 bytes back from the target");
        CU_ASSERT_EQUAL(task->datain.size, 200);

        logging(LOG_VERBOSE, "Verify residual overflow flag is set");
        if (task->residual_status != SCSI_RESIDUAL_OVERFLOW) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "overflow flag");
        }
        CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);

        logging(LOG_VERBOSE, "Verify we got %zu bytes of residual overflow",
                block_size - 200);
        if (task->residual != block_size - 200) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        block_size - 200, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, block_size - 200);

        scsi_free_scsi_task(task);
        task = NULL;



        logging(LOG_VERBOSE, "Try reading two blocks but iSCSI expected "
                "transfer length==%zu (==one block)", block_size);
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_READ16;
        task->cdb[13] = 2;
        task->cdb_size = 16;
        task->xfer_dir = SCSI_XFER_READ;
        task->expxferlen = block_size;

        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, NULL);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        if (task->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify we got one whole block back from the target");
        CU_ASSERT_EQUAL(task->datain.size, (int)block_size);

        logging(LOG_VERBOSE, "Verify residual overflow flag is set");
        if (task->residual_status != SCSI_RESIDUAL_OVERFLOW) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "overflow flag");
        }
        CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_OVERFLOW);

        logging(LOG_VERBOSE, "Verify we got one block of residual overflow");
        if (task->residual != block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        block_size, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, block_size);

        scsi_free_scsi_task(task);
        task = NULL;
}
