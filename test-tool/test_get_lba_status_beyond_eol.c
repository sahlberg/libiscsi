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
test_get_lba_status_beyond_eol(void)
{
        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test GETLBASTATUS one block beyond the end of the LUN");

        GETLBASTATUS(sd, NULL, num_blocks + 1, 24,
                     EXPECT_LBA_OOB);

        logging(LOG_VERBOSE, "Test GETLBASTATUS at LBA 2^63");
        GETLBASTATUS(sd, NULL, 0x8000000000000000ULL, 24,
                     EXPECT_LBA_OOB);

        logging(LOG_VERBOSE, "Test GETLBASTATUS at LBA -1");
        GETLBASTATUS(sd, NULL, 0xffffffffffffffffULL, 24,
                     EXPECT_LBA_OOB);
}
