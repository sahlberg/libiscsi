/* 
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
test_readcapacity16_support(void)
{
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that READCAPACITY16 is supported on SBC3");

        ret = readcapacity16(sd, NULL, 15,
                             EXPECT_STATUS_GOOD);
        if (ret == -2) {
                if (sbc3_support) {
                        logging(LOG_NORMAL, "[FAILED] READCAPACITY16 is not available but the device claims SBC-3 support.");
                        CU_FAIL("READCAPACITY16 failed but the device claims SBC-3 support.");
                } else {
                        logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 is not implemented on this target and it does not claim SBC-3 support.");
                }
        } else {
                CU_ASSERT_EQUAL(ret, 0);
        }
}
