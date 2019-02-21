/* 
   Copyright (C) 2013 by Lee Duncan <lee@gonzoleeman.net>
   
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
#include <arpa/inet.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


void
test_prin_serviceaction_range(void)
{
        int ret = 0;
        int i;


        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve IN Serviceaction range.");

        /* verify PRIN/READ_KEYS works -- XXX redundant -- remove this? */
        ret = prin_read_keys(sd, &task, NULL, 16384);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE IN is not implemented.");
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);

        /* verify that PRIN/SA={0,1,2,3} works ... */
        for (i = 0; i < 4; i++) {
                ret = prin_task(sd, i, 1);
                CU_ASSERT_EQUAL(ret, 0);
        }

        /*  verify that PRIN/SA={4..0x20} fails ... */
        for (i = 4; i < 0x20; i++) {
                ret = prin_task(sd, i, 0);
                CU_ASSERT_EQUAL(ret, 0);
        }
}
