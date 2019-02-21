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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_writesame10_unmap_until_end(void)
{
        unsigned int i;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_LBPWS10;
        CHECK_FOR_SBC;

        if (inq_bl->wsnz) {
                logging(LOG_NORMAL, "WRITESAME10 does not support 0-blocks."
                        "WSNZ == 1");
                memset(scratch, 0, block_size);
                WRITESAME10(sd, 0, block_size, 0, 0, 1, 0, 0, scratch,
                            EXPECT_INVALID_FIELD_IN_CDB);
                return;
        }

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITESAME10 of 1-256 blocks at the end of the LUN by setting number-of-blocks==0");
        for (i = 1; i <= 256; i++) {
                logging(LOG_VERBOSE, "Write %d blocks of 0xFF", i);
                memset(scratch, 0xff, block_size * i);
                WRITE10(sd, num_blocks - i,
                        i * block_size, block_size, 0, 0, 0, 0, 0, scratch,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME10", i);
                WRITESAME10(sd, num_blocks - i,
                            block_size, 0, 0, 1, 0, 0, scratch,
                            EXPECT_STATUS_GOOD);

                if (rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ is set. Read the unmapped "
                                "blocks back and verify they are all zero");

                        logging(LOG_VERBOSE, "Read %d blocks and verify they "
                                "are now zero", i);
                        READ10(sd, NULL, num_blocks - i,
                               i * block_size, block_size,
                               0, 0, 0, 0, 0, scratch,
                               EXPECT_STATUS_GOOD);
                        ALL_ZERO(scratch, i * block_size);
                } else {
                        logging(LOG_VERBOSE, "LBPRZ is clear. Skip the read "
                                "and verify zero test");
                }
        }
}
