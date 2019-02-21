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
test_mandatory_sbc(void)
{
        int ret;
        //unsigned char buf[4096];
        //struct unmap_list list[1];

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test support for all mandatory opcodes on SBC devices");

        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, "Test INQUIRY.");
        ret = inquiry(sd, NULL, 0, 0, 255,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Test READCAPACITY10.");
        ret = readcapacity10(sd, NULL, 0, 0,
                             EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        if (sbc3_support) {
                logging(LOG_VERBOSE, "Test READCAPACITY16. The device claims SBC-3 support.");
                ret = readcapacity16(sd, NULL, 15,
                                     EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
        }

        logging(LOG_VERBOSE, "Test READ10.");
        ret = read10(sd, NULL, 0, block_size, block_size,
                     0, 0, 0, 0, 0, NULL,
                     EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        if (sbc3_support) {
                logging(LOG_VERBOSE, "Test READ16. the device claims SBC-3 support.");
                ret = read16(sd, NULL, 0, block_size, block_size,
                             0, 0, 0, 0, 0, NULL,
                             EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
        }

        logging(LOG_VERBOSE, "Test TESTUNITREADY.");
        ret = testunitready(sd,
                            EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
}
