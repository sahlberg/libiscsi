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
test_verify16_mismatch(void)
{
        int i;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test VERIFY16 for blocks 1-255");
        for (i = 1; i <= 256; i++) {
                int offset = random() % (i * block_size);

                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                READ16(sd, NULL, 0, i * block_size,
                       block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                /* flip a random byte in the data */
                scratch[offset] ^= 'X';
                logging(LOG_VERBOSE, "Flip some bits in the data");

                VERIFY16(sd, 0, i * block_size, block_size, 0, 0, 1, scratch,
                         EXPECT_MISCOMPARE);
        }

        logging(LOG_VERBOSE, "Test VERIFY16 of 1-256 blocks at the end of the LUN");
        for (i = 1; i <= 256; i++) {
                int offset = random() % (i * block_size);

                if (maximum_transfer_length && maximum_transfer_length < i) {
                        break;
                }
                READ16(sd, NULL, num_blocks - i,
                       i * block_size, block_size, 0, 0, 0, 0, 0, scratch,
                       EXPECT_STATUS_GOOD);

                /* flip a random byte in the data */
                scratch[offset] ^= 'X';
                logging(LOG_VERBOSE, "Flip some bits in the data");

                VERIFY16(sd, num_blocks - i,
                         i * block_size, block_size, 0, 0, 1, scratch,
                         EXPECT_MISCOMPARE);
        }
}
