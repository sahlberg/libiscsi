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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"


void
test_writeverify12_residuals(void)
{
        struct scsi_task *task_ret;
        unsigned char buf[10000];
        struct iscsi_data data;
        int ok;
        unsigned int i;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITEVERIFY12 commands with residuals");
        logging(LOG_VERBOSE, "Block size is %zu", block_size);

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This WRITEVERIFY12 test is only "
                        "supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        /* check if writeverify12 is supported */
        WRITEVERIFY12(sd, 0, 0, block_size, 0, 0, 0, 0, NULL,
                      EXPECT_STATUS_GOOD);

        /* Try a writeverify12 of 1 block but xferlength == 0 */
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_WRITE_VERIFY12;
        task->cdb[9] = 1;
        task->cdb_size = 12;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = 0;

        /*
         * we don't want autoreconnect since some targets will drop the session
         * on this condition.
         */
        iscsi_set_noautoreconnect(sd->iscsi_ctx, 1);

        logging(LOG_VERBOSE, "Try writing one block but with iSCSI expected transfer length==0");

        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, NULL);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);
        CU_ASSERT_NOT_EQUAL(task->status, SCSI_STATUS_CANCELLED); /* XXX redundant? */

        if (task->status        == SCSI_STATUS_CHECK_CONDITION
            && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
            && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
                logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 is not implemented.");
                CU_PASS("WRITEVERIFY12 is not implemented.");
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


        logging(LOG_VERBOSE, "Try writing one block but with iSCSI expected transfer length==10000");
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_WRITE_VERIFY12;
        task->cdb[9] = 1;
        task->cdb_size = 12;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = 10000;

        memset(buf, 0xa6, sizeof(buf));
        data.size = task->expxferlen;
        data.data = &buf[0];
        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, &data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        if (task->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

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


        logging(LOG_VERBOSE, "Try writing one block but with iSCSI expected transfer length==200");
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_WRITE_VERIFY12;
        task->cdb[9] = 1;
        task->cdb_size = 12;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = 200;

        data.size = task->expxferlen;
        data.data = &buf[0];
        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, &data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        ok = task->status == SCSI_STATUS_GOOD ||
                (task->status == SCSI_STATUS_CHECK_CONDITION &&
                 task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST &&
                 task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_FIELD_IN_INFORMATION_UNIT);
        if (!ok) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT(ok);

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



        logging(LOG_VERBOSE, "Try writing two blocks but iSCSI expected "
                "transfer length==%zu (==one block)", block_size);
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_WRITE_VERIFY12;
        task->cdb[9] = 2;
        task->cdb_size = 12;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = block_size;

        data.size = task->expxferlen;
        data.data = &buf[0];
        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, &data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

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

        logging(LOG_VERBOSE, "Verify we got one block of residual overflow");
        if (task->residual != block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        block_size, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, block_size);

        scsi_free_scsi_task(task);
        task = NULL;




        logging(LOG_VERBOSE, "Verify that if iSCSI EDTL > SCSI TL then we only write SCSI TL amount of data");

        logging(LOG_VERBOSE, "Write two blocks of 'a'");
        memset(buf, 'a', 10000);
        WRITE12(sd, 0, 2 * block_size, block_size, 0, 0, 0, 0, 0, buf,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Write one block of 'b' but set iSCSI EDTL to 2 blocks.");
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(buf, 'b', 10000);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_WRITE_VERIFY12;
        task->cdb[9] = 1;
        task->cdb_size = 12;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = 2 * block_size;

        data.size = task->expxferlen;
        data.data = &buf[0];
        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, &data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

        logging(LOG_VERBOSE, "Verify that the target returned SUCCESS");
        if (task->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Target returned error %s",
                        iscsi_get_error(sd->iscsi_ctx));
        }
        CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify residual underflow flag is set");
        if (task->residual_status != SCSI_RESIDUAL_UNDERFLOW) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set residual "
                        "underflow flag");
        }
        CU_ASSERT_EQUAL(task->residual_status, SCSI_RESIDUAL_UNDERFLOW);

        logging(LOG_VERBOSE, "Verify we got one block of residual underflow");
        if (task->residual != block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        block_size, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, block_size);
        scsi_free_scsi_task(task);
        task = NULL;

        logging(LOG_VERBOSE, "Read the two blocks");
        READ12(sd, NULL, 0, 2* block_size, block_size, 0, 0, 0, 0, 0, buf,
               EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify that the first block was changed to 'b'");
        for (i = 0; i < block_size; i++) {
                if (buf[i] != 'b') {
                        logging(LOG_NORMAL, "First block did not contain expected 'b'");
                        CU_FAIL("Block was not written correctly");
                        break;
                }
        }

        logging(LOG_VERBOSE, "Verify that the second block was NOT overwritten and still contains 'a'");
        for (i = block_size; i < 2 * block_size; i++) {
                if (buf[i] != 'a') {
                        logging(LOG_NORMAL, "Second block was overwritten and no longer contain 'a'");
                        CU_FAIL("Second block was incorrectly overwritten");
                        break;
                }
        }


        logging(LOG_VERBOSE, "Verify that if iSCSI EDTL < SCSI TL then we only write iSCSI EDTL amount of data");

        logging(LOG_VERBOSE, "Write two blocks of 'a'");
        memset(buf, 'a', 10000);
        WRITE12(sd, 0, 2 * block_size, block_size, 0, 0, 0, 0, 0, buf,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Write two blocks of 'b' but set iSCSI EDTL to 1 blocks.");
        task = malloc(sizeof(struct scsi_task));
        CU_ASSERT_PTR_NOT_NULL_FATAL(task);

        memset(buf, 'b', 10000);

        memset(task, 0, sizeof(struct scsi_task));
        task->cdb[0] = SCSI_OPCODE_WRITE_VERIFY12;
        task->cdb[9] = 2;
        task->cdb_size = 12;
        task->xfer_dir = SCSI_XFER_WRITE;
        task->expxferlen = block_size;

        data.size = task->expxferlen;
        data.data = &buf[0];
        task_ret = iscsi_scsi_command_sync(sd->iscsi_ctx, sd->iscsi_lun, task, &data);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);

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

        logging(LOG_VERBOSE, "Verify we got one block of residual overflow");
        if (task->residual != block_size) {
                logging(LOG_VERBOSE, "[FAILED] Target did not set correct "
                        "amount of residual. Expected %zu but got %zu.",
                        block_size, task->residual);
        }
        CU_ASSERT_EQUAL(task->residual, block_size);
        scsi_free_scsi_task(task);
        task = NULL;

        logging(LOG_VERBOSE, "Read the two blocks");
        READ12(sd, NULL, 0, 2* block_size, block_size, 0, 0, 0, 0, 0, buf,
               EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Verify that the first block was changed to 'b'");
        for (i = 0; i < block_size; i++) {
                if (buf[i] != 'b') {
                        logging(LOG_NORMAL, "First block did not contain expected 'b'");
                        CU_FAIL("Block was not written correctly");
                        break;
                }
        }

        logging(LOG_VERBOSE, "Verify that the second block was NOT overwritten and still contains 'a'");
        for (i = block_size; i < 2 * block_size; i++) {
                if (buf[i] != 'a') {
                        logging(LOG_NORMAL, "Second block was overwritten and no longer contain 'a'");
                        CU_FAIL("Second block was incorrectly overwritten");
                        break;
                }
        }
}
