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
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_writeatomic16_wrprotect(void)
{
        int i, gran;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        if (!inq_bl) {
                CU_PASS("BlockLimits VPD is not available. Skipping test.\n");
                return;
        }

        logging(LOG_VERBOSE, LOG_BLANK_LINE);

        gran = inq_bl->atomic_gran ? inq_bl->atomic_gran : 1;
        memset(scratch, 0, block_size);
        WRITEATOMIC16(sd, 0, block_size * gran, block_size, 0, 0, 0, 0, scratch,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITEATOMIC16 with non-zero WRPROTECT");
        memset(scratch, 0xa6, block_size);
        if (!inq->protect || (rc16 != NULL && !rc16->prot_en)) {
                logging(LOG_VERBOSE, "Device does not support/use protection information. All commands should fail.");
                for (i = 1; i < 8; i++) {
                        WRITEATOMIC16(sd, 0, gran * block_size, block_size,
                                      i, 0, 0, 0, scratch,
                                      EXPECT_INVALID_FIELD_IN_CDB);
                }
                return;
        }

        logging(LOG_NORMAL, "No tests for devices that support protection information yet.");
}
