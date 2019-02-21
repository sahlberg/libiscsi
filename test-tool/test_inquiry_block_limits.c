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

static void check_lbp(int *supports_lbp)
{
        *supports_lbp = 0;

        CHECK_FOR_THIN_PROVISIONING;

        *supports_lbp = 1;
}

void
test_inquiry_block_limits(void)
{
        int supports_lbp, ret;
        struct scsi_inquiry_block_limits *bl;
        struct scsi_task *bl_task = NULL;
        struct scsi_inquiry_logical_block_provisioning *lbp = NULL;
        struct scsi_task *lbp_task = NULL;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of the INQUIRY Block Limits");

        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, "Block device. Verify that we can read Block "
                "Limits VPD");
        ret = inquiry(sd, &bl_task,
                      1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, 255,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret != 0) {
                logging(LOG_NORMAL, "[FAILURE] failed to send inquiry.");
                goto finished;
        }

        bl = scsi_datain_unmarshall(bl_task);
        if (bl == NULL) {
                logging(LOG_NORMAL, "[FAILURE] failed to unmarshall inquiry "
                        "datain blob.");
                CU_FAIL("[FAILURE] failed to unmarshall inquiry "
                        "datain blob.");
                goto finished;
        }

        logging(LOG_VERBOSE, "Verify that the PageLength matches up with the "
                "size of the DATA-IN buffer.");
        CU_ASSERT_EQUAL(bl_task->datain.size, bl_task->datain.data[3] + 4);
        if (bl_task->datain.size != bl_task->datain.data[3] + 4) {
                logging(LOG_NORMAL, "[FAILURE] Invalid PageLength returned. "
                        "Was %d but expected %d",
                        bl_task->datain.data[3], bl_task->datain.size - 4);
        } else {
                logging(LOG_VERBOSE, "[SUCCESS] PageLength matches DataIn "
                        "buffer size");
        }

        logging(LOG_VERBOSE, "Verify that the PageLength matches SCSI-level.");
        /* if it is not SBC3 then we assume it must be SBC2 */
        if (sbc3_support) {
                logging(LOG_VERBOSE, "Device claims SBC-3. Verify that "                                "page size is >= 60");
        } else {
                logging(LOG_VERBOSE, "Device is not SBC-3. Verify that "
                        "PageLength == 8 (but allow >= 60 too. Some SBC-2 "
                        "devices support some SBC-3 features.");
        }

        if (bl_task->datain.data[3] == 8) {
                if (sbc3_support) {
                        logging(LOG_NORMAL, "[FAILURE] Invalid PageLength "
                                "returned. SBC3 claimed but page length "
                                "is 8.");
                        CU_FAIL("[FAILED] Invalid pagelength returned. "
                                "SBC3 claimed but page length is 8.");
                }
        } else if (bl_task->datain.size >= 60) {
                if (!sbc3_support) {
                        logging(LOG_NORMAL, "[WARNING] SBC-3 pagelength "
                                "(>=60) returned but SBC-3 support was not "
                                "claimed in the standard inquiry page.");
                        CU_FAIL("[WARNING] SBC-3 pagelength "
                                "(>=60) returned but SBC-3 support was not "
                                "claimed in the standard inquiry page.");
                }
        }

        if (bl_task->datain.data[3] != 0x3c) {
                goto finished;
        }

        check_lbp(&supports_lbp);
        if (!supports_lbp)
                goto finished;

        /*
         * MAXIMUM UNMAP LBA COUNT
         * MAXIMUM UNMAP BLOCK DESCRIPTOR COUNT
         */
        logging(LOG_VERBOSE, "Try reading the logical block provisioning VPD");
        ret = inquiry(sd, &lbp_task,
                      1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, 255,
                      EXPECT_STATUS_GOOD);
        if (ret == 0) {
                lbp = scsi_datain_unmarshall(lbp_task);
                if (lbp == NULL) {
                        logging(LOG_NORMAL, "[FAILURE] failed to unmarshall "
                        "inquiry datain blob.");
                }
        }

        if (lbp && lbp->lbpu) {
                /* We support UNMAP so MAXIMUM UNMAP LBA COUNT and
                 * MAXIMUM UNMAP BLOCK DESCRIPTOR COUNT.
                 * They must be > 0.
                 * It can be 0xffffffff which means no limit, but if there is
                 * an explicit limit set, then we check that it looks sane.
                 * Sane here means < 1M.
                 */
                logging(LOG_VERBOSE, "Device claims UNMAP support via LBPU");
                logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP LBA COUNT is "
                        "not 0");
                CU_ASSERT_NOT_EQUAL(bl->max_unmap, 0);

                logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP LBA COUNT is "
                        "at least 2^LBPPBE");
                CU_ASSERT_EQUAL(bl->max_unmap >= (1U << rc16->lbppbe), 1);

                if (bl->max_unmap != 0xffffffff) {
                        logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP LBA "
                                "COUNT is not insanely big");
                        CU_ASSERT_TRUE(bl->max_unmap <= 1024*1024);
                }

                logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP BLOCK "
                        "DESCRIPTOR COUNT is not 0");
                CU_ASSERT_NOT_EQUAL(bl->max_unmap_bdc, 0);
                if (bl->max_unmap_bdc != 0xffffffff) {
                        logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP "
                                "BLOCK DESCRIPTOR COUNT is not insanely big");
                        CU_ASSERT_TRUE(bl->max_unmap_bdc <= 1024*1024);
                }
        } else {
                logging(LOG_VERBOSE, "Device does not claim UNMAP support via "
                        "LBPU");
                logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP LBA COUNT is "
                        "0");
                CU_ASSERT_EQUAL(bl->max_unmap, 0);

                logging(LOG_VERBOSE, "Verify that MAXIMUM UNMAP BLOCK "
                        "DESCRIPTOR COUNT is 0");
                CU_ASSERT_EQUAL(bl->max_unmap_bdc, 0);
        }



finished:
        scsi_free_scsi_task(bl_task);
        scsi_free_scsi_task(lbp_task);
}
