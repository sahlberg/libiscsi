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
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_write16_flags(void)
{ 
        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITE16 flags");

        logging(LOG_VERBOSE, "Test WRITE16 with DPO==1");
        memset(scratch, 0xa6, block_size);
        WRITE16(sd, 0, block_size, block_size, 0, 1, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITE16 with FUA==1 FUA_NV==0");
        WRITE16(sd, 0, block_size, block_size, 0, 0, 1, 0, 0, scratch,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITE16 with FUA==1 FUA_NV==1");
        WRITE16(sd, 0, block_size, block_size, 0, 0, 1, 1, 0, scratch,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITE16 with FUA==0 FUA_NV==1");
        WRITE16(sd, 0, block_size, block_size, 0, 0, 0, 1, 0, scratch,
                EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test WRITE16 with DPO==1 FUA==1 FUA_NV==1");
        WRITE16(sd, 0, block_size, block_size, 0, 1, 1, 1, 0, scratch,
                EXPECT_STATUS_GOOD);
}
