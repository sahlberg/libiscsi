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
test_inquiry_alloc_length(void)
{
        int ret, i;
        struct scsi_inquiry_standard *std_inq;
        struct scsi_task *task2 = NULL;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of the INQUIRY allocation length");

        logging(LOG_VERBOSE, "Verify we can read standard INQUIRY page with alloc length from 5-255");
        for (i = 5; i < 256 ; i++) {
                if (task != NULL) {
                        scsi_free_scsi_task(task);
                        task = NULL;
                }
                ret = inquiry(sd, &task, 0, 0, i,
                              EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
        }
        logging(LOG_VERBOSE, "Verify we got at least 36 bytes of data when reading with alloc length 255");
        CU_ASSERT(task->datain.size >= 36);

        logging(LOG_VERBOSE, "Verify we can unmarshall the DATA-IN buffer");
        std_inq = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(std_inq, NULL);
        if (std_inq == NULL) {
                logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN "
                        "buffer");
                return;
        }

        logging(LOG_VERBOSE, "Verify peripheral-qualifier is 0");
        CU_ASSERT_EQUAL(std_inq->qualifier, 0);



        /* Final test. IF this claims SPC-3 or later then the target 
           supports 16-bit allocation lengths. Try reading INQ data
           specifying 256 bytes as allocation length and make sure the
           target responds properly.
        */
        logging(LOG_VERBOSE, "If version is SPC-3 or later INQUIRY supports 16-bit allocation lengths");
        switch (std_inq->version) {
        case 0x5:
        case 0x6:
                break;
        default:
                logging(LOG_NORMAL, "[SKIPPED] This device does not claim "
                        "SPC-3 or later");
                CU_PASS("[SKIPPED] Not SPC-3 or later");
                goto finished;
        }

        scsi_free_scsi_task(task);
        task = NULL;

        logging(LOG_VERBOSE, "Version is SPC-3 or later. Read INQUIRY data using 16-bit allocation length");
        logging(LOG_VERBOSE, "Read INQUIRY data with allocation length 511 (low order byte is 0xff)");
        ret = inquiry(sd, &task, 0, 0, 511,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Read INQUIRY data with allocation length 512 (low order byte is 0x00)");
        ret = inquiry(sd, &task2, 0, 0, 512,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "INQUIRY data should be the same when allocation length is 511 and 512 bytes");
        ret = task->datain.size != task2->datain.size;
        CU_ASSERT_EQUAL(ret, 0);
        ret = memcmp(task->datain.data, task2->datain.data, task->datain.size);
        CU_ASSERT_EQUAL(ret, 0);
        

finished:
        if (task != NULL) {
                scsi_free_scsi_task(task);
                task = NULL;
        }
        if (task2 != NULL) {
                scsi_free_scsi_task(task2);
                task2 = NULL;
        }
}
