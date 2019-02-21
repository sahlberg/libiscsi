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
test_writesame10_invalid_dataout_size(void)
{
        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_LBPWS10;
        CHECK_FOR_LBPPB_GT_1;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that WRITESAME10 fails for invalid "
                "(too small/too large) DataOut sizes.");
        memset(scratch, 0xa6, block_size);

        logging(LOG_VERBOSE, "Check too small DataOut");
        logging(LOG_VERBOSE, "Unmap with DataOut==%zd (block_size==%zd)",
                block_size / 2, block_size);
        WRITESAME10(sd, 0, block_size / 2, 1, 0, 1, 0, 0, scratch,
                    EXPECT_STATUS_GENERIC_BAD);

        logging(LOG_VERBOSE, "Check too large DataOut");
        logging(LOG_VERBOSE, "Unmap with DataOut==%zd (block_size==%zd)",
                block_size * 2, block_size);
        WRITESAME10(sd, 0, block_size * 2, 1, 0, 1, 0, 0, scratch,
                    EXPECT_STATUS_GENERIC_BAD);
}
