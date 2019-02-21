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
test_read6_simple(void)
{
        int i;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test READ6 of 1-255 blocks at the start of the LUN");
        for (i = 1; i <= 255; i++) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                READ6(sd, NULL, 0, i * block_size, block_size, NULL,
                      EXPECT_STATUS_GOOD);
        }

        logging(LOG_VERBOSE, "Test READ6 of 1-255 blocks at the end of the LUN");
        if (num_blocks > 0x200000) {
                CU_PASS("LUN is too big for read-at-eol tests with READ6. Skipping test.\n");
        } else {
                for (i = 1; i <= 255 && i + 0U <= num_blocks; i++) {
                        if (maximum_transfer_length && maximum_transfer_length < i) {
                                break;
                        }
                        READ6(sd, NULL, num_blocks - i,
                              i * block_size, block_size, NULL,
                              EXPECT_STATUS_GOOD);
                }
        }

        /* 256 is converted to 0 when the CDB is marshalled by the helper */
        if (maximum_transfer_length >= 256) {
                logging(LOG_VERBOSE, "Transfer length == 0 means we want to "
                        "transfer 256 blocks");
                READ6(sd, &task, 0,
                      256 * block_size, block_size, NULL,
                      EXPECT_STATUS_GOOD);
                if (task->status != SCSI_STATUS_GOOD) {
                        logging(LOG_NORMAL, "[FAILED] READ6 command: "
                                "failed with sense. %s", sd->error_str );
                }
                CU_ASSERT_EQUAL(task->status, SCSI_STATUS_GOOD);

                logging(LOG_VERBOSE, "Verify that we did get 256 blocks of "
                        "data back");
                if (task->datain.size == (int)(256 * block_size)) {
                        logging(LOG_VERBOSE, "[SUCCESS] Target returned 256 "
                                "blocks of data");
                } else {
                        logging(LOG_NORMAL, "[FAILED] Target did not return "
                                "256 blocks of data");
                }
                CU_ASSERT_EQUAL(task->datain.size, (int)(256 * block_size));
        }
}
