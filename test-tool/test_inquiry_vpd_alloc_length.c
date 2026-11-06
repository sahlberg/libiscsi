/*
   Copyright (C) 2026 by Saransh Bansal <saransh1906@gmail.com>

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
#include <string.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

/*
 * SPC: the ALLOCATION LENGTH only truncates the parameter data; it must not
 * change its content. Initiators (e.g. Linux) commonly read just the 4-byte
 * VPD page header first to learn the PAGE LENGTH, so a 4-byte read must
 * return the same header as a full read.
 */
void
test_inquiry_vpd_alloc_length(void)
{
        int ret, i;
        struct scsi_inquiry_supported_pages *sup_inq;
        struct scsi_task *full_task = NULL, *short_task = NULL;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test INQUIRY VPD pages with ALLOCATION LENGTH 4");

        ret = inquiry(sd, &task,
                      1, SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES, 255,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret != 0 || task == NULL) {
                logging(LOG_NORMAL, "[FAILED] INQUIRY command failed");
                goto out;
        }

        sup_inq = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(sup_inq, NULL);
        if (sup_inq == NULL) {
                logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN "
                        "buffer");
                goto out;
        }

        for (i = 0; i < sup_inq->num_pages; i++) {
                int page = sup_inq->pages[i];

                logging(LOG_VERBOSE, "Read page 0x%02x with ALLOCATION "
                        "LENGTH 255", page);
                ret = inquiry(sd, &full_task, 1, page, 255,
                              EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
                if (ret != 0 || full_task == NULL ||
                    full_task->datain.size < 4) {
                        logging(LOG_NORMAL, "[FAILED] Could not read page "
                                "0x%02x", page);
                        goto next;
                }

                logging(LOG_VERBOSE, "Read page 0x%02x with ALLOCATION "
                        "LENGTH 4", page);
                ret = inquiry(sd, &short_task, 1, page, 4,
                              EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
                if (ret != 0 || short_task == NULL) {
                        goto next;
                }

                logging(LOG_VERBOSE, "Verify exactly 4 bytes are returned");
                CU_ASSERT_EQUAL(short_task->datain.size, 4);
                if (short_task->datain.size != 4) {
                        logging(LOG_NORMAL, "[FAILED] Page 0x%02x returned "
                                "%d bytes for ALLOCATION LENGTH 4", page,
                                short_task->datain.size);
                        goto next;
                }

                logging(LOG_VERBOSE, "Verify the header matches the full "
                        "page");
                if (memcmp(short_task->datain.data, full_task->datain.data,
                           4)) {
                        logging(LOG_NORMAL, "[FAILED] Page 0x%02x header "
                                "%02x %02x %02x %02x, expected "
                                "%02x %02x %02x %02x", page,
                                short_task->datain.data[0],
                                short_task->datain.data[1],
                                short_task->datain.data[2],
                                short_task->datain.data[3],
                                full_task->datain.data[0],
                                full_task->datain.data[1],
                                full_task->datain.data[2],
                                full_task->datain.data[3]);
                        CU_FAIL("VPD page header differs for ALLOCATION "
                                "LENGTH 4");
                }

        next:
                if (full_task != NULL) {
                        scsi_free_scsi_task(full_task);
                        full_task = NULL;
                }
                if (short_task != NULL) {
                        scsi_free_scsi_task(short_task);
                        short_task = NULL;
                }
        }

out:
        if (task != NULL) {
                scsi_free_scsi_task(task);
                task = NULL;
        }
}
