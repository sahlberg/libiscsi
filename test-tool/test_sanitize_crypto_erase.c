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
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"


static void
check_wacereq(void)
{
        struct scsi_task *task_ret = NULL;

        logging(LOG_VERBOSE, "Read one block from LBA 0");
        READ10(sd, &task_ret, 0, block_size, block_size, 0, 0, 0, 0, 0, NULL,
               EXPECT_STATUS_GOOD);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);
        if (task_ret == NULL) {
                return;
        }
        CU_ASSERT_NOT_EQUAL(task_ret->status, SCSI_STATUS_CANCELLED);

        switch (inq_bdc->wabereq) {
        case 0:
                logging(LOG_NORMAL, "[FAILED] SANITIZE BLOCK ERASE "
                        "opcode is supported but WACEREQ is 0");
                CU_FAIL("[FAILED] SANITIZE BLOCK ERASE "
                        "opcode is supported but WACEREQ is 0");
                break;
        case 1:
                logging(LOG_VERBOSE, "WACEREQ==1. Reads from the "
                        "device should be successful.");
                if (task_ret->status == SCSI_STATUS_GOOD) {
                        logging(LOG_VERBOSE, "[SUCCESS] Read was "
                                "successful after SANITIZE");
                        break;
                }
                logging(LOG_NORMAL, "[FAILED] Read after "
                        "SANITIZE failed but WACEREQ is 1");
                CU_FAIL("[FAILED] Read after SANITIZE failed "
                        "but WACEREQ is 1");
                break;
        case 2:
                logging(LOG_VERBOSE, "WACEREQ==2. Reads from the "
                        "device should fail.");
                if (task_ret->status        == SCSI_STATUS_CHECK_CONDITION
                    && task_ret->sense.key  == SCSI_SENSE_MEDIUM_ERROR
                    && task_ret->sense.ascq != SCSI_SENSE_ASCQ_WRITE_AFTER_SANITIZE_REQUIRED) {
                        logging(LOG_VERBOSE, "[SUCCESS] Read failed "
                                "with CHECK_CONDITION/MEDIUM_ERROR/"
                                "!WRITE_AFTER_SANITIZE_REQUIRED");
                        break;
                }
                logging(LOG_VERBOSE, "[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "!WRITE_AFTER_SANITIZE_REQUIRED");
                CU_FAIL("[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "!WRITE_AFTER_SANITIZE_REQUIRED");
                break;
        case 3:
                logging(LOG_VERBOSE, "WACEREQ==3. Reads from the "
                        "device should fail.");
                if (task_ret->status        == SCSI_STATUS_CHECK_CONDITION
                    && task_ret->sense.key  == SCSI_SENSE_MEDIUM_ERROR
                    && task_ret->sense.ascq == SCSI_SENSE_ASCQ_WRITE_AFTER_SANITIZE_REQUIRED) {
                        logging(LOG_VERBOSE, "[SUCCESS] Read failed "
                                "with CHECK_CONDITION/MEDIUM_ERROR/"
                                "WRITE_AFTER_SANITIZE_REQUIRED");
                        break;
                }
                logging(LOG_VERBOSE, "[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "WRITE_AFTER_SANITIZE_REQUIRED");
                CU_FAIL("[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "WRITE_AFTER_SANITIZE_REQUIRED");
                break;
        }

        scsi_free_scsi_task(task_ret);
}

static void
check_lun_is_wiped(unsigned char *buf, uint64_t lba)
{
        unsigned char *rbuf = alloca(256 * block_size);

        READ16(sd, NULL, lba, 256 * block_size, block_size, 0, 0, 0, 0, 0, rbuf,
               EXPECT_STATUS_GOOD);
        if (!memcmp(buf, rbuf, 256 * block_size)) {
                logging(LOG_NORMAL, "[FAILED] Blocks were not wiped");
                CU_FAIL("[FAILED] Blocks were not wiped");
        } else {
                logging(LOG_VERBOSE, "[SUCCESS] Blocks were wiped");
        }
}

void
test_sanitize_crypto_erase(void)
{ 
        struct iscsi_data data;
        struct scsi_command_descriptor *cd;
        unsigned char *buf = alloca(256 * block_size);

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE CRYPTO ERASE");

        CHECK_FOR_SANITIZE;
        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Check that SANITIZE CRYPTO_ERASE is supported "
                "in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_CRYPTO_ERASE);
        if (cd == NULL) {
                logging(LOG_VERBOSE, "Opcode is not supported. Verify that "
                        "WACEREQ is zero.");
                if (inq_bdc && inq_bdc->wacereq) {
                        logging(LOG_NORMAL, "[FAILED] WACEREQ is not 0 but "
                                "SANITIZE CRYPTO ERASE opcode is not "
                                "supported");
                        CU_FAIL("[FAILED] WACEREQ is not 0 but CRYPTO ERASE "
                                "is not supported.");
                }

                logging(LOG_NORMAL, "[SKIPPED] SANITIZE CRYPTO_ERASE is not "
                        "implemented according to REPORT_SUPPORTED_OPCODES.");
                CU_PASS("SANITIZE is not implemented.");
                return;
        }

        logging(LOG_VERBOSE, "Verify that we have BlockDeviceCharacteristics "
                "VPD page.");
        if (inq_bdc == NULL) {
                logging(LOG_NORMAL, "[FAILED] SANITIZE CRYPTO ERASE opcode is "
                        "supported but BlockDeviceCharacteristics VPD page is "
                        "missing");
                CU_FAIL("[FAILED] BlockDeviceCharacteristics VPD "
                        "page is missing");
        }


        logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
        memset(scratch, 'a', 256 * block_size);
        WRITE16(sd, 0, 256 * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);
        logging(LOG_VERBOSE, "Write 'a' to the last 256 LBAs");
        WRITE16(sd, num_blocks - 256, 256 * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);


        logging(LOG_VERBOSE, "Test we can perform basic CRYPTO ERASE SANITIZE");
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_CRYPTO_ERASE, 0, NULL,
                 EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Check that the first 256 LBAs are wiped.");
        check_lun_is_wiped(buf, 0);
        logging(LOG_VERBOSE, "Check that the last 256 LBAs are wiped.");
        check_lun_is_wiped(buf, num_blocks - 256);

        data.size = 8;
        data.data = alloca(data.size);
        memset(data.data, 0, data.size);

        logging(LOG_VERBOSE, "CRYPTO_ERASE parameter list length must be 0");
        logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
                "CRYPTO ERASE");
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_CRYPTO_ERASE, 8, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);
        if (inq_bdc) {
                logging(LOG_VERBOSE, "Check WACEREQ setting and that READ "
                        "after SANITIZE works correctly.");
                check_wacereq();
        }
}
