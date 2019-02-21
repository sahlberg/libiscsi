/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronneisahlberg@gmail.com>
   
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
test_writesame16_beyond_eol(void)
{ 
        int i;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITESAME16 1-256 blocks one block beyond the end");
        memset(scratch, 0, block_size);
        for (i = 1; i <= 256; i++) {
                WRITESAME16(sd, num_blocks - i + 1,
                            block_size, i, 0, 0, 0, 0, scratch,
                            EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test WRITESAME16 1-256 blocks at LBA==2^63");
        for (i = 1; i <= 256; i++) {
                WRITESAME16(sd, 0x8000000000000000ULL,
                            block_size, i, 0, 0, 0, 0, scratch,
                            EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test WRITESAME16 1-256 blocks at LBA==-1");
        for (i = 1; i <= 256; i++) {
                WRITESAME16(sd, -1, block_size, i, 0, 0, 0, 0, scratch,
                            EXPECT_LBA_OOB);
        }

        logging(LOG_VERBOSE, "Test WRITESAME16 2-256 blocks all but one block beyond the end");
        for (i = 2; i <= 256; i++) {
                WRITESAME16(sd, num_blocks - 1,
                            block_size, i, 0, 0, 0, 0, scratch,
                            EXPECT_LBA_OOB);
        }
}
