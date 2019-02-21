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
test_prin_read_keys_simple(void)
{
        int ret = 0;
        int al;


        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test Persistent Reserve IN READ_KEYS works.");

        ret = prin_read_keys(sd, &task, NULL, 16384);
        if (ret == -2) {
                CU_PASS("PERSISTENT RESERVE IN is not implemented.");
                return;
        }        
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Test DATA-IN is at least 8 bytes.");
        if (task->datain.size < 8) {
                logging(LOG_NORMAL,
                    "[FAILED] DATA-IN returned less than 8 bytes");
                return;
        }

        logging(LOG_VERBOSE, "Test ADDITIONAL_LENGTH matches DATA_IN size.");
        al = scsi_get_uint32(&task->datain.data[4]);
        if (al != task->datain.size - 8) {
                logging(LOG_NORMAL,
                    "[FAILED] ADDITIONAL_LENGTH was %d bytes but %d was expected.",
                        al, task->datain.size - 8);
                return;
        }
}
