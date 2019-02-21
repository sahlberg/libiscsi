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
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_orwrite_verify(void)
{
        int i, ret;
        unsigned char *buf     = &scratch[0];
        unsigned char *readbuf = &scratch[256 * block_size];


        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test ORWRITE of 1-256 blocks at the start of the LUN");
        for (i = 1; i <= 256; i++) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }

                logging(LOG_VERBOSE, "Write %d blocks of all-zero", i);
                memset(buf, 0, block_size * i);
                ret = write10(sd, 0, i * block_size,
                              block_size, 0, 0, 0, 0, 0, buf,
                              EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);

                logging(LOG_VERBOSE, "OrWrite %d blocks with 0xa5", i);
                memset(buf, 0xa5, block_size * i);
                ORWRITE(sd, 0, i * block_size,
                        block_size, 0, 0, 0, 0, 0, buf,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read %d blocks back", i);
                READ10(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, readbuf,
                       EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Verify that the blocks are all 0xa5");
                ret = memcmp(buf, readbuf, block_size * i);
                CU_ASSERT_EQUAL(ret, 0);

                logging(LOG_VERBOSE, "OrWrite %d blocks with 0x5a", i);
                memset(buf, 0x5a, block_size * i);
                ORWRITE(sd, 0, i * block_size,
                        block_size, 0, 0, 0, 0, 0, buf,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read %d blocks back", i);
                READ10(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, readbuf,
                       EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Verify that the blocks are all 0xff");
                memset(buf, 0xff, block_size * i);
                ret = memcmp(buf, readbuf, block_size * i);
                CU_ASSERT_EQUAL(ret, 0);
        }

        logging(LOG_VERBOSE, "Test ORWRITE of 1-256 blocks at the end of the LUN");
        for (i = 1; i <= 256; i++) {
                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }

                logging(LOG_VERBOSE, "Write %d blocks of all-zero", i);
                memset(buf, 0, block_size * i);
                WRITE16(sd, num_blocks - i, i * block_size,
                        block_size, 0, 0, 0, 0, 0, buf,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "OrWrite %d blocks with 0xa5", i);
                memset(buf, 0xa5, block_size * i);
                ORWRITE(sd, num_blocks - i, i * block_size,
                        block_size, 0, 0, 0, 0, 0, buf,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read %d blocks back", i);
                READ16(sd, NULL, num_blocks - i, i * block_size,
                       block_size, 0, 0, 0, 0, 0, readbuf,
                       EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Verify that the blocks are all 0xa5");
                ret = memcmp(buf, readbuf, block_size * i);
                CU_ASSERT_EQUAL(ret, 0);

                logging(LOG_VERBOSE, "OrWrite %d blocks with 0x5a", i);
                memset(buf, 0x5a, block_size * i);
                ORWRITE(sd, num_blocks - i, i * block_size,
                        block_size, 0, 0, 0, 0, 0, buf,
                        EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Read %d blocks back", i);
                READ16(sd, NULL, num_blocks - i, i * block_size,
                       block_size, 0, 0, 0, 0, 0, readbuf,
                       EXPECT_STATUS_GOOD);

                logging(LOG_VERBOSE, "Verify that the blocks are all 0xff");
                memset(buf, 0xff, block_size * i);
                ret = memcmp(buf, readbuf, block_size * i);
                CU_ASSERT_EQUAL(ret, 0);
        }
}
