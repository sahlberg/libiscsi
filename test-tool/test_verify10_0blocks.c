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
#include "iscsi-test-cu.h"

void
test_verify10_0blocks(void)
{
        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test VERIFY10 0-blocks at LBA==0");

        logging(LOG_VERBOSE, "Test VERIFY10 0-blocks one block past end-of-LUN");
        VERIFY10(sd, num_blocks + 1, 0, block_size, 0, 0, 1, NULL,
                 EXPECT_LBA_OOB);

        if (num_blocks - 1 < 0x80000000) {
          logging(LOG_VERBOSE, "Test VERIFY10 0-blocks at LBA==2^31");
          VERIFY10(sd, 0x80000000, 0, block_size, 0, 0, 1, NULL,
                   EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test VERIFY10 0-blocks at LBA==-1");
        VERIFY10(sd, -1, 0, block_size, 0, 0, 1, NULL,
                 EXPECT_LBA_OOB);
}
