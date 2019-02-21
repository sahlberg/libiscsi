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
test_modesense6_control_d_sense(void)
{
        struct scsi_task *ms_task = NULL;
        struct scsi_task *r16_task = NULL;
        struct scsi_mode_sense *ms;
        struct scsi_mode_page *page;
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of MODESENSE6 CONTROL D_SENSE flag");

        logging(LOG_VERBOSE, "Read the CONTROL page from the device");
        ret = modesense6(sd, &ms_task, 0, SCSI_MODESENSE_PC_CURRENT,
                         SCSI_MODEPAGE_CONTROL, 0, 255,
                         EXPECT_STATUS_GOOD);
        if (ret != 0) {
                logging(LOG_NORMAL,"[WARNING] Could not read "
                        "BlockDeviceCharacteristics.");
                goto finished;
        }
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
                CU_PASS("[SKIPPED] CONTROL page not reported");
                goto finished;
        }
        logging(LOG_VERBOSE, "Send a READ16 that will fail so we can check "
                "the type of sense data returned");
        READ16(sd, &r16_task, 0xffffffffffffffffLL, block_size, block_size, 0,
               0, 0, 0, 0, NULL,
               EXPECT_LBA_OOB);

        if (page->control.d_sense) {
                logging(LOG_VERBOSE, "D_SENSE is set, verify that sense format "
                        "is descriptor format");
        } else {
                logging(LOG_VERBOSE, "D_SENSE is clear, verify that sense format "
                        "is fixed format");
        }
        switch (r16_task->sense.error_type) {
        case SCSI_SENSE_DESCRIPTOR_CURRENT:
        case SCSI_SENSE_DESCRIPTOR_DEFERRED_ERRORS:
                if (!page->control.d_sense) {
                        logging(LOG_NORMAL, "[FAILED] D_SENSE is set but "
                                "returned sense is not descriptor format");
                        CU_FAIL("[FAILED] Wrong type of sense format returned");
                        goto finished;
                }
                break;
        case SCSI_SENSE_FIXED_CURRENT:
        case SCSI_SENSE_FIXED_DEFERRED_ERRORS:
                if (page->control.d_sense) {
                        logging(LOG_NORMAL, "[FAILED] D_SENSE is cleat but "
                                "returned sense is not fixed format");
                        CU_FAIL("[FAILED] Wrong type of sense format returned");
                        goto finished;
                }
                break;
        }


 finished:
        if (ms_task != NULL) {
                scsi_free_scsi_task(ms_task);
        }
        if (r16_task != NULL) {
                scsi_free_scsi_task(r16_task);
        }
}
