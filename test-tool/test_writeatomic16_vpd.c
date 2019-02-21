/*
   Copyright (C) 2015 by Ronnie Sahlberg <sahlberg@gmail.com>

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
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_writeatomic16_vpd(void)
{
        int ret;
        struct scsi_inquiry_block_limits *bl;
        struct scsi_task *bl_task = NULL;
        int gran;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITEATOMIC16 VPD data");

        CHECK_FOR_SBC;
        CHECK_FOR_DATALOSS;


        logging(LOG_VERBOSE, "Block device. Verify that we can read Block "
                "Limits VPD");
        ret = inquiry(sd, &bl_task,
                      1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, 255,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret != 0) {
                logging(LOG_NORMAL, "[FAILURE] failed to read Block Limits VDP");
                CU_FAIL("[FAILURE] failed to read Block Limits VDP");
                goto finished;
        }
        bl = scsi_datain_unmarshall(bl_task);
        if (bl == NULL) {
                logging(LOG_NORMAL, "[FAILURE] failed to unmarshall Block Limits VDP");
                CU_FAIL("[FAILURE] failed to unmarshall Block Limits VDP");
                goto finished;
        }


        logging(LOG_VERBOSE, "Check if WRITEATOMIC16 is supported");
        gran = inq_bl->atomic_gran ? inq_bl->atomic_gran : 1;
        memset(scratch   , 0x00, block_size * gran);
        ret = writeatomic16(sd, 0, block_size * gran,
                            block_size, 0, 0, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);
        if (ret == -2) {
                logging(LOG_VERBOSE, "WRITEATOMIC16 is NOT supported by the target.");

                logging(LOG_VERBOSE, "Verify that MAXIMUM_ATOMIC_TRANSFER_LENGTH is zero");
                if (bl->max_atomic_xfer_len) {
                        logging(LOG_VERBOSE, "MAXIMUM_ATOMIC_TRANSFER_LENGTH is non-zero but target does not support ATOMICWRITE16");
                        CU_FAIL("MAXIMUM_ATOMIC_TRANSFER_LENGTH is non-zero but target does not support ATOMICWRITE16");
                }

                logging(LOG_VERBOSE, "Verify that ATOMIC_ALIGNMENT is zero");
                if (bl->atomic_align) {
                        logging(LOG_VERBOSE, "ATOMIC_ALIGNMENT is non-zero but target does not support ATOMICWRITE16");
                        CU_FAIL("ATOMIC_ALIGNMENT is non-zero but target does not support ATOMICWRITE16");
                }

                logging(LOG_VERBOSE, "Verify that ATOMIC_GRANULARITY is zero");
                if (bl->atomic_gran) {
                        logging(LOG_VERBOSE, "ATOMIC_GRANULARITY is non-zero but target does not support ATOMICWRITE16");
                        CU_FAIL("ATOMIC_GRANULARITY is non-zero but target does not support ATOMICWRITE16");
                }
                goto finished;
        }

        logging(LOG_VERBOSE, "WRITEATOMIC16 IS supported by the target.");
        logging(LOG_VERBOSE, "Verify that MAXIMUM_ATOMIC_TRANSFER_LENGTH is non-zero");
        if (!bl->max_atomic_xfer_len) {
                logging(LOG_VERBOSE, "[WARNING] MAXIMUM_ATOMIC_TRANSFER_LENGTH is zero but target supports ATOMICWRITE16");
                CU_FAIL("[WARNING] MAXIMUM_ATOMIC_TRANSFER_LENGTH is zero but target supports ATOMICWRITE16");
        }

        logging(LOG_VERBOSE, "Verify that MAXIMUM_ATOMIC_TRANSFER_LENGTH is less than or equal to MAXIMUM_TRANSFER_LENGTH");
        if (bl->max_atomic_xfer_len > bl->max_xfer_len) {
                logging(LOG_VERBOSE, "[FAILED] MAXIMUM_ATOMIC_TRANSFER_LENGTH is greater than MAXIMUM_TRANSFER_LENGTH");
                CU_FAIL("[FAILED] MAXIMUM_ATOMIC_TRANSFER_LENGTH is greater than MAXIMUM_TRANSFER_LENGTH");
        }

        logging(LOG_VERBOSE, "Check handling on misaligned writes");
        if (bl->atomic_align < 2) {
                logging(LOG_VERBOSE, "[SKIPPED] No alignment restrictions on this LUN");
        } else {
                logging(LOG_VERBOSE, "Atomic Write at LBA 1 should fail due to misalignment");
                ret = writeatomic16(sd, 1, block_size * gran,
                                    block_size, 0, 0, 0, 0, scratch,
                                    EXPECT_INVALID_FIELD_IN_CDB);
                if (ret) {
                        logging(LOG_VERBOSE, "[FAILED] Misaligned write did NOT fail with INVALID_FIELD_IN_CDB");
                CU_FAIL("[FAILED] Misaligned write did NOT fail with INVALID_FIELD_IN_CDB");
                }
        }

        logging(LOG_VERBOSE, "Check handling on invalid granularity");
        if (bl->atomic_gran < 2) {
                logging(LOG_VERBOSE, "[SKIPPED] No granularity restrictions on this LUN");
        } else {
                logging(LOG_VERBOSE, "Atomic Write of 1 block should fail due to invalid granularity");
                ret = writeatomic16(sd, 0, block_size,
                                    block_size, 0, 0, 0, 0, scratch,
                                    EXPECT_INVALID_FIELD_IN_CDB);
                if (ret) {
                        logging(LOG_VERBOSE, "[FAILED] Misgranularity write did NOT fail with INVALID_FIELD_IN_CDB");
                CU_FAIL("[FAILED] Misgranularity write did NOT fail with INVALID_FIELD_IN_CDB");
                }
        }


finished:
        scsi_free_scsi_task(bl_task);
}
