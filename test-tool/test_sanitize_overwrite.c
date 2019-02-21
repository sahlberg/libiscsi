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
check_lun_is_wiped(uint64_t lba, unsigned char c)
{
        unsigned char *rbuf = alloca(256 * block_size);
        unsigned char *zbuf = alloca(256 * block_size);

        READ16(sd, NULL, lba, 256 * block_size,
               block_size, 0, 0, 0, 0, 0, rbuf,
               EXPECT_STATUS_GOOD);

        memset(zbuf, c, 256 * block_size);

        if (memcmp(zbuf, rbuf, 256 * block_size)) {
                logging(LOG_NORMAL, "[FAILED] Blocks did not read back as %#x",
                        c);
                CU_FAIL("[FAILED] Blocks did not read back as expected");
        } else {
                logging(LOG_VERBOSE, "[SUCCESS] Blocks read back as %#x", c);
        }
}

void
test_sanitize_overwrite(void)
{ 
        int i;
        struct iscsi_data data;
        struct scsi_command_descriptor *cd;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE");

        CHECK_FOR_SANITIZE;
        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Check that SANITIZE OVERWRITE is supported "
                "in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_OVERWRITE);
        if (cd == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] SANITIZE OVERWRITE is not "
                        "implemented according to REPORT_SUPPORTED_OPCODES.");
                CU_PASS("SANITIZE is not implemented.");
                return;
        }

        logging(LOG_VERBOSE, "Verify that we have BlockDeviceCharacteristics "
                "VPD page.");
        if (inq_bdc == NULL) {
                logging(LOG_NORMAL, "[FAILED] SANITIZE OVERWRITE opcode is "
                        "supported but BlockDeviceCharacteristics VPD page is "
                        "missing");
                CU_FAIL("[FAILED] BlockDeviceCharacteristics VPD "
                        "page is missing");
        }

        logging(LOG_VERBOSE, "Check MediumRotationRate whether this is a HDD "
                "or a SSD device.");
        if (inq_bdc && inq_bdc->medium_rotation_rate == 0) {
                logging(LOG_NORMAL, "This is a HDD device");
        } else {
                logging(LOG_NORMAL, "This is a SSD device");
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

        logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with initialization pattern of one full block");
        data.size = block_size + 4;
        data.data = alloca(data.size);
        memset(&data.data[4], 0xaa, block_size);

        data.data[0] = 0x01;
        data.data[1] = 0x00;
        data.data[2] = block_size >> 8;
        data.data[3] = block_size & 0xff;
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Check that the first 256 LBAs are wiped.");
        check_lun_is_wiped(0, 0xaa);
        logging(LOG_VERBOSE, "Check that the last 256 LBAs are wiped.");
        check_lun_is_wiped(num_blocks - 256, 0xaa);


        logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with initialization pattern of one half block");
        data.size = block_size / 2 + 4;

        data.data[2] = (block_size / 2) >> 8;
        data.data[3] = (block_size / 2 ) & 0xff;

        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE with initialization pattern of 4 bytes");
        data.size = 4 + 4;

        data.data[2] = 0;
        data.data[3] = 4;

        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "OVERWRITE parameter list length must "
                        "be > 4 and < blocksize+5");
        for (i = 0; i < 5; i++) {
                logging(LOG_VERBOSE, "Test OVERWRITE with ParamLen:%d is an "
                        "error.", i);

                SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, i, &data,
                         EXPECT_INVALID_FIELD_IN_CDB);
        }


        logging(LOG_VERBOSE, "Test OVERWRITE with ParamLen:%zd (blocksize+5) "
                "is an error.", block_size + 5);

        data.size = block_size + 8;
        data.data = alloca(block_size + 8); /* so we can send IP > blocksize */
        memset(data.data, 0, data.size);
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, block_size + 5, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);

        logging(LOG_VERBOSE, "Test OVERWRITE COUNT == 0 is an error");
        data.size = block_size + 4;

        data.data[0] = 0x00;
        data.data[1] = 0x00;
        data.data[2] = block_size >> 8;
        data.data[3] = block_size & 0xff;
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);

        logging(LOG_VERBOSE, "Test INITIALIZATION PATTERN LENGTH == 0 is an "
                "error");
        data.size = block_size + 4;

        data.data[0] = 0x00;
        data.data[1] = 0x00;
        data.data[2] = 0x00;
        data.data[3] = 0x00;
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);

        logging(LOG_VERBOSE, "Test INITIALIZATION PATTERN LENGTH == %zd  > %zd "
                "(blocksize) is an error", block_size + 4, block_size);

        data.size = block_size + 4;

        data.data[0] = 0x00;
        data.data[1] = 0x00;
        data.data[2] = (block_size + 4) >> 8;
        data.data[3] = (block_size + 4) & 0xff;
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);
}
