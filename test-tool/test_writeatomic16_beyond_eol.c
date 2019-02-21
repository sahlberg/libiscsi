/*
   Copyright (C) 2015 Ronnie Sahlberg <ronneisahlberg@gmail.com>

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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"


void
test_writeatomic16_beyond_eol(void)
{
        int align, i, gran;
        const size_t bufsz = 256 * 2 * block_size;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        if (!inq_bl) {
                CU_PASS("BlockLimits VPD is not available. Skipping test.\n");
                return;
        }

        logging(LOG_VERBOSE, LOG_BLANK_LINE);

        memset(scratch, 0xa6, bufsz);
        align = inq_bl->atomic_align ? inq_bl->atomic_align : 1;
        gran = inq_bl->atomic_gran ? inq_bl->atomic_gran : 1;
        WRITEATOMIC16(sd, 0, block_size * gran, block_size, 0, 0, 0, 0, scratch,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 1-256 blocks <granularity> blocks beyond the end");
        for (i = gran; i <= 256; i += gran) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITEATOMIC16(sd, num_blocks - i,
                              2 * i * block_size, block_size,
                              0, 0, 0, 0, scratch,
                              EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 1-256 blocks at LBA==2^63");
        for (i = gran; i <= 256; i += gran) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITEATOMIC16(sd, 0x8000000000000000ULL,
                              i * block_size, block_size,
                              0, 0, 0, 0, scratch,
                              EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 1-256 blocks at LBA==-<alignment>");
        for (i = gran; i <= 256; i += gran) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITEATOMIC16(sd, -align, i * block_size,
                              block_size, 0, 0, 0, 0, scratch,
                              EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 2-256 blocks all but one block beyond the end");
        for (i = 2 * gran; i <= 256; i += gran) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                WRITEATOMIC16(sd, num_blocks - gran,
                              i * block_size, block_size,
                              0, 0, 0, 0, scratch,
                              EXPECT_LBA_OOB);
        }
}
