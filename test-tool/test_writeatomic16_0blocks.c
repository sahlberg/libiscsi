/*
   Copyright (C) 2015 Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_writeatomic16_0blocks(void)
{
        int align;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        if (!inq_bl) {
                CU_PASS("BlockLimits VPD is not available. Skipping test.\n");
                return;
        }

        logging(LOG_VERBOSE, LOG_BLANK_LINE);

        align = inq_bl->atomic_align ? inq_bl->atomic_align : 1;
        logging(LOG_VERBOSE, "Test WRITEATOMIC16 0-blocks at LBA==0");
        WRITEATOMIC16(sd, 0, 0, block_size, 0, 0, 0, 0, NULL,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 0-blocks one alignment past end-of-LUN");
        WRITEATOMIC16(sd, num_blocks + align, 0, block_size, 0, 0, 0, 0, NULL,
                      EXPECT_LBA_OOB);

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 0-blocks at LBA==2^63");
        WRITEATOMIC16(sd, 0x8000000000000000ULL, 0,
                      block_size, 0, 0, 0, 0, NULL,
                      EXPECT_LBA_OOB);

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 0-blocks at LBA==-<alignment>");
        WRITEATOMIC16(sd, -align, 0, block_size, 0, 0, 0, 0, NULL,
                      EXPECT_LBA_OOB);
}
