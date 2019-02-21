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
test_writesame16_unmap_unaligned(void)
{
        int i;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_LBPWS;
        CHECK_FOR_LBPPB_GT_1;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that unaligned WRITESAME16 Unmap succeeds. LBPPB==%d", lbppb);

        for (i = 1; i < lbppb; i++) {
                logging(LOG_VERBOSE, "Unmap %d blocks using WRITESAME16 at LBA:%d", lbppb - i, i);
                WRITESAME16(sd, i, 0, lbppb - i, 0, 1, 0, 0, NULL,
                            EXPECT_STATUS_GOOD);
        }
}
