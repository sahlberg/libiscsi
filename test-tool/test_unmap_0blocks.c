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
test_unmap_0blocks(void)
{
        int i;
        int max_nr_bdc = 256;
        struct unmap_list list[257];

        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test UNMAP of 0 blocks at LBA:0-255 as a single descriptor");
        for (i = 0; i < 256; i++) {
                list[0].lba = i;
                list[0].num = 0;
                UNMAP(sd, 0, list, 1,
                      EXPECT_STATUS_GOOD);
        }

        logging(LOG_VERBOSE, "Test UNMAP of 0 blocks at end-of-LUN");
        list[0].lba = num_blocks;
        list[0].num = 0;
        UNMAP(sd, 0, list, 1,
              EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test UNMAP without any descriptors.");
        UNMAP(sd, 0, list, 0,
              EXPECT_STATUS_GOOD);

        if (inq_bl->max_unmap_bdc <= 1) {
                CU_PASS("[SKIPPING] Test UNMAP of 0 blocks with multiple descriptos not supported");
                return;
        }

        if (inq_bl->max_unmap_bdc > 0 && max_nr_bdc > (int)inq_bl->max_unmap_bdc) {
          max_nr_bdc = (int)inq_bl->max_unmap_bdc;
        }

        logging(LOG_VERBOSE, "Test UNMAP of 0 blocks at LBA:0-%d  with one descriptor per block", max_nr_bdc - 1);
        for (i = 0; i < max_nr_bdc; i++) {
                list[i].lba = i;
                list[i].num = 0;
                UNMAP(sd, 0, list, i + 1,
                      EXPECT_STATUS_GOOD);
        }

        logging(LOG_VERBOSE, "Test UNMAP of 0 blocks at LBA:0-%d  with one descriptor per block, possibly \"overlapping\".", max_nr_bdc - 1);
        for (i = 0; i < max_nr_bdc; i++) {
                list[i].lba = i/2;
                list[i].num = 0;
        }
        UNMAP(sd, 0, list, max_nr_bdc,
              EXPECT_STATUS_GOOD);
}
