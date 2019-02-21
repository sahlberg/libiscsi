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
#include "iscsi-multipath.h"

void
test_sanitize_readonly(void)
{
        int ret;
        struct iscsi_data data;
        struct scsi_command_descriptor *cd;
        struct scsi_device *sd2;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE with READONLY devices");

        CHECK_FOR_SANITIZE;
        CHECK_FOR_DATALOSS;

        if (sd->iscsi_ctx == NULL) {
                const char *err = "[SKIPPED] This SANITIZE test is only "
                        "supported for iSCSI backends";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

        logging(LOG_VERBOSE, "Create a second connection to the target");
        ret = mpath_sd2_get_or_clone(sd, &sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret < 0)
                return;

        logging(LOG_VERBOSE, "Set Software Write Protect on the second connection");
        ret = set_swp(sd2);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret != 0) {
                return;
        }

        logging(LOG_VERBOSE, "Use TESTUNITREADY to clear unit attention on "
                "first connection");
        while (testunitready_clear_ua(sd)) {
                sleep(1);
        }

        logging(LOG_VERBOSE, "Check if SANITIZE OVERWRITE is supported "
                "in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_OVERWRITE);
        if (cd == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] SANITIZE OVERWRITE is not "
                        "implemented according to REPORT_SUPPORTED_OPCODES.");
        } else {
                logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with "
                        "initialization pattern of one full block");
                data.size = block_size + 4;
                data.data = alloca(data.size);
                memset(&data.data[4], 0xaa, block_size);

                data.data[0] = 0x01;
                data.data[1] = 0x00;
                data.data[2] = block_size >> 8;
                data.data[3] = block_size & 0xff;
                SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                         EXPECT_WRITE_PROTECTED);
        }


        logging(LOG_VERBOSE, "Check if SANITIZE BLOCK_ERASE is supported "
                "in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_BLOCK_ERASE);
        if (cd == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] SANITIZE BLOCK_ERASE is not "
                        "implemented according to REPORT_SUPPORTED_OPCODES.");
        } else {
                logging(LOG_VERBOSE, "Test SANITIZE BLOCK_ERASE");
                SANITIZE(sd, 0, 0, SCSI_SANITIZE_BLOCK_ERASE, 0, NULL,
                         EXPECT_WRITE_PROTECTED);
        }

        logging(LOG_VERBOSE, "Check if SANITIZE CRYPTO_ERASE is supported "
                "in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_CRYPTO_ERASE);
        if (cd == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] SANITIZE CRYPTO_ERASE is not "
                        "implemented according to REPORT_SUPPORTED_OPCODES.");
        } else {
                logging(LOG_VERBOSE, "Test SANITIZE CRYPTO_ERASE");
                SANITIZE(sd, 0, 0, SCSI_SANITIZE_CRYPTO_ERASE, 0, NULL,
                         EXPECT_WRITE_PROTECTED);
        }


        logging(LOG_VERBOSE, "Clear Software Write Protect on the second connection");
        ret = clear_swp(sd2);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Use TESTUNITREADY to clear unit attention on "
                "first connection");
        while (testunitready_clear_ua(sd)) {
                sleep(1);
        }

        mpath_sd2_put(sd2);
}
