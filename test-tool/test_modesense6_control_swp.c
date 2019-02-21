/* 
   Copyright (C) 2015 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

void
test_modesense6_control_swp(void)
{
        struct scsi_task *ms_task = NULL;
        struct scsi_mode_sense *ms;
        struct scsi_mode_page *page;
        int ret;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of MODESENSE6 CONTROL SWP flag");

        logging(LOG_VERBOSE, "Set SWP to enable write protect");
        ret = set_swp(sd);
        if (ret == -2) {
                CU_PASS("[SKIPPED] Target does not support changing SWP");
                return;
        }
        CU_ASSERT_EQUAL(ret, 0);
        if (ret) {
                goto finished;
        }

        logging(LOG_VERBOSE, "Read the CONTROL page back from the device");
        MODESENSE6(sd, &ms_task, 0, SCSI_MODESENSE_PC_CURRENT,
                   SCSI_MODEPAGE_CONTROL, 0, 255,
                   EXPECT_STATUS_GOOD);
        logging(LOG_VERBOSE, "[SUCCESS] CONTROL page fetched.");

        logging(LOG_VERBOSE, "Try to unmarshall the DATA-IN buffer.");
        ms = scsi_datain_unmarshall(ms_task);
        if (ms == NULL) {
                logging(LOG_NORMAL, "[FAILED] failed to unmarshall mode sense "
                        "datain buffer");
                CU_FAIL("[FAILED] Failed to unmarshall the data-in buffer.");
                goto finished;
        }
        logging(LOG_VERBOSE, "[SUCCESS] Unmarshalling successful.");
        for (page = ms->pages; page; page = page->next) {
                if (page->page_code == SCSI_MODEPAGE_CONTROL) {
                        break;
                }
        }
        if(page == NULL) {
                logging(LOG_NORMAL, "[WARNING] CONTROL page was not returned."
                        "All devices SHOULD implement this page.");
        }

        logging(LOG_VERBOSE, "Verify that the SWP bit is set");
        if (page->control.swp == 0) {
                logging(LOG_NORMAL, "[FAILED] SWP bit is not set");
                CU_FAIL("[FAILED] SWP is not set");
                goto finished;
        }
        logging(LOG_VERBOSE, "[SUCCESS] SWP was set successfully");


        logging(LOG_VERBOSE, "Read a block from the now Read-Only device");
        READ10(sd, NULL, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
               EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Try to write a block to the Read-Only device");
        WRITE10(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_WRITE_PROTECTED);

 finished:
        if (ms_task != NULL) {
                scsi_free_scsi_task(ms_task);
        }
        logging(LOG_VERBOSE, "Clear SWP to disable write protect");
        clear_swp(sd);
}
