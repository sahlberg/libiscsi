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
test_inquiry_supported_vpd(void)
{
        int ret, i;
        struct scsi_inquiry_supported_pages *sup_inq;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test INQUIRY supported VPD pages");

        logging(LOG_VERBOSE, "Verify we can read the SUPPORTED VPD page");
        ret = inquiry(sd, &task,
                      1, SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES, 255,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Verify we got at least 4 bytes of data");
        CU_ASSERT(task->datain.size >= 4);

        logging(LOG_VERBOSE, "Verify we can unmarshall the DATA-IN buffer");
        sup_inq = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(sup_inq, NULL);
        if (sup_inq == NULL) {
                logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN "
                        "buffer");
                return;
        }

        logging(LOG_VERBOSE, "Verify we read all the supported pages");
        for (i = 0; i < sup_inq->num_pages; i++) {
                logging(LOG_VERBOSE, "Verify we can read page 0x%02x",
                        sup_inq->pages[i]);

                ret = inquiry(sd, NULL, 1, sup_inq->pages[i], 255,
                              EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
        }

        if (task != NULL) {
                scsi_free_scsi_task(task);
                task = NULL;
        }
}
