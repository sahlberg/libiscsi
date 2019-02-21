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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

void
test_unmap_simple(void)
{
        int i;
        int max_nr_bdc = 256;
        struct unmap_list list[257];

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test basic UNMAP");

        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_SBC;


        logging(LOG_VERBOSE, "Test UNMAP of 1-256 blocks at the start of the "
                "LUN as a single descriptor");

        logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
        memset(scratch, 'a', 256 * block_size);
        WRITE10(sd, 0, 256 * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        for (i = 1; i <= 256; i++) {
                logging(LOG_VERBOSE, "UNMAP blocks 0-%d", i);
                list[0].lba = 0;
                list[0].num = i;
                UNMAP(sd, 0, list, 1,
                      EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read blocks 0-%d", i);
                READ10(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                if (rc16 && rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ==1 All UNMAPPED blocks "
                                "should read back as 0");
                        ALL_ZERO(scratch, i * block_size);
                }
        }

        if (inq_bl->max_unmap_bdc > 0 && max_nr_bdc > (int)inq_bl->max_unmap_bdc) {
          max_nr_bdc = (int)inq_bl->max_unmap_bdc;
        }
        if (max_nr_bdc < 0 || max_nr_bdc > 256) {
                logging(LOG_VERBOSE, "Clamp max unmapped blocks to 256");
                max_nr_bdc = 256;
        }

        logging(LOG_VERBOSE, "Test UNMAP of 1-%d blocks at the start of the "
                "LUN with one descriptor per block", max_nr_bdc);

        logging(LOG_VERBOSE, "Write 'a' to the first %d LBAs", max_nr_bdc);
        memset(scratch, 'a', max_nr_bdc * block_size);
        WRITE10(sd, 0, max_nr_bdc * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        for (i = 0; i < max_nr_bdc; i++) {
                list[i].lba = i;
                list[i].num = 1;
                UNMAP(sd, 0, list, i + 1,
                      EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read blocks 0-%d", i);
                READ10(sd, NULL, 0, (i + 1) * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                if (rc16 && rc16->lbprz) {
                        logging(LOG_VERBOSE, "LBPRZ==1 All UNMAPPED blocks "
                                "should read back as 0");
                        ALL_ZERO(scratch, i * block_size);
                }
        }
}
