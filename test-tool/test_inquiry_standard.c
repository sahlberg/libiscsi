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
test_inquiry_standard(void)
{
        int ret, i;
        struct scsi_inquiry_standard *std_inq;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of the standard INQUIRY page");

        logging(LOG_VERBOSE, "Verify we can read standard INQUIRY page");
        /* 260 bytes is the maximum possible size of the standard vpd */
        ret = inquiry(sd, &task, 0, 0, 260,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);

        logging(LOG_VERBOSE, "Verify we got at least 36 bytes of data");
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

        logging(LOG_VERBOSE, "Verify version field is either 0x4, 0x5 or 0x6");
        switch (std_inq->version) {
        case 0x0:
                logging(LOG_NORMAL, "[WARNING] Standard INQUIRY data claims "
                        "conformance to no standard. Version==0. "
                        "Bad sport.");

                break;
        case 0x4:
        case 0x5:
        case 0x6:
                break;
        default:
                logging(LOG_NORMAL, "[FAILED] Invalid version in standard "
                        "INQUIRY data. Version %d found but only versions "
                        "0x4,0x4,0x6 are valid.", std_inq->version);
                CU_FAIL("Invalid version in INQUIRY data");
        }

        logging(LOG_VERBOSE, "Verify response-data-format is 2 "
                "(SPC-2 or later)");
        if (std_inq->response_data_format != 2) {
                logging(LOG_NORMAL, "[FAILED] Response data format is "
                        "invalid. Must be 2 but device returned %d",
                        std_inq->response_data_format);
        }
        CU_ASSERT_EQUAL(std_inq->response_data_format, 2);

        logging(LOG_VERBOSE, "Verify additional-length is correct");
        if (std_inq->additional_length > task->datain.size - 5) {
                logging(LOG_NORMAL, "[FAILED] Bad additional length "
                        "returned. Should be %d but device returned %d.",
                        task->datain.size - 5,
                        std_inq->additional_length);
                logging(LOG_NORMAL, "[FAILED] Additional length points "
                        "beyond end of data");
                CU_FAIL("Additional length points beyond end of data");
        }
        if (std_inq->additional_length < task->datain.size - 5) {
                logging(LOG_NORMAL, "[WARNING] Bad additional length "
                        "returned. Should be %d but device returned %d. ",
                        task->datain.size - 5,
                        std_inq->additional_length);
                logging(LOG_VERBOSE, "Verify that all padding data is 0");
                for (i = std_inq->additional_length + 6; i < task->datain.size; i++) {
                        if (!task->datain.data[i])
                                continue;
                        logging(LOG_NORMAL, "[FAILED] Padding data is not zero."
                                           " Are we leaking data?");
                        CU_FAIL("Padding data is not zero. Leaking data?");
                }
        }

        logging(LOG_VERBOSE, "Verify VENDOR_IDENTIFICATION is in ASCII");
        for (i = 8; i < 16; i++) {
                /* SPC-4 4.4.1 only characters 0x00 and 0x20-0x7E allowed */
                if (task->datain.data[i] == 0) {
                        continue;
                }
                if (task->datain.data[i] >= 0x20 && task->datain.data[i] <= 0x7e) {
                        continue;
                }

                logging(LOG_NORMAL, "[FAILED] VENDOR_IDENTIFICATION contains "
                        "non-ASCII characters");
                CU_FAIL("Invalid characters in VENDOR_IDENTIFICATION");
                break;
        }

        logging(LOG_VERBOSE, "Verify PRODUCT_IDENTIFICATION is in ASCII");
        for (i = 16; i < 32; i++) {
                /* SPC-4 4.4.1 only characters 0x00 and 0x20-0x7E allowed */
                if (task->datain.data[i] == 0) {
                        continue;
                }
                if (task->datain.data[i] >= 0x20 && task->datain.data[i] <= 0x7e) {
                        continue;
                }

                logging(LOG_NORMAL, "[FAILED] PRODUCT_IDENTIFICATION contains "
                        "non-ASCII characters");
                CU_FAIL("Invalid characters in PRODUCT_IDENTIFICATION");
                break;
        }

        logging(LOG_VERBOSE, "Verify PRODUCT_REVISION_LEVEL is in ASCII");
        for (i = 32; i < 36; i++) {
                /* SPC-4 4.4.1 only characters 0x00 and 0x20-0x7E allowed */
                if (task->datain.data[i] == 0) {
                        continue;
                }
                if (task->datain.data[i] >= 0x20 && task->datain.data[i] <= 0x7e) {
                        continue;
                }

                logging(LOG_NORMAL, "[FAILED] PRODUCT_REVISON_LEVEL contains "
                        "non-ASCII characters");
                CU_FAIL("Invalid characters in PRODUCT_REVISON_LEVEL");
                break;
        }

        logging(LOG_VERBOSE, "Verify AERC is clear in SPC-3 and later");
        if (task->datain.data[3] & 0x80 && std_inq->version >= 5) {
                logging(LOG_NORMAL, "[FAILED] AERC is set but this device "
                        "reports SPC-3 or later.");
                CU_FAIL("AERC is set but SPC-3+ is claimed");
        }

        logging(LOG_VERBOSE, "Verify TRMTSK is clear in SPC-2 and later");
        if (task->datain.data[3] & 0x40 && std_inq->version >= 4) {
                logging(LOG_NORMAL, "[FAILED] TRMTSK is set but this device "
                        "reports SPC-2 or later.");
                CU_FAIL("TRMTSK is set but SPC-2+ is claimed");
        }

        if (task != NULL) {
                scsi_free_scsi_task(task);
                task = NULL;
        }
}
