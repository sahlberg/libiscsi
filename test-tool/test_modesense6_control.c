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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"

void
test_modesense6_control(void)
{
        struct scsi_mode_sense *ms;
        struct scsi_mode_page *ap_page;
        struct scsi_mode_page *ct_page;
        struct scsi_task *ap_task = NULL;
        struct scsi_task *ct_task = NULL;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of MODESENSE6 CONTROL page");

        logging(LOG_VERBOSE, "Fetch the CONTROL page via AllPages");
        logging(LOG_VERBOSE, "Send MODESENSE6 command to fetch AllPages");
        MODESENSE6(sd, &ap_task, 0, SCSI_MODESENSE_PC_CURRENT,
                   SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
                   EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "[SUCCESS] All Pages fetched.");


        logging(LOG_VERBOSE, "Try to unmarshall the DATA-IN buffer.");
        ms = scsi_datain_unmarshall(ap_task);
        if (ms == NULL) {
                logging(LOG_NORMAL, "[FAILED] failed to unmarshall mode sense "
                        "datain buffer");
                CU_FAIL("[FAILED] Failed to unmarshall the data-in buffer.");
                goto finished;
        }
        logging(LOG_VERBOSE, "[SUCCESS] Unmarshalling successful.");


        logging(LOG_VERBOSE, "Verify that mode data length is >= 3");
        if (ms->mode_data_length >= 3) {
                logging(LOG_VERBOSE, "[SUCCESS] Mode data length is >= 3");
        } else {
                logging(LOG_NORMAL, "[FAILED] Mode data length is < 3");
        }
        CU_ASSERT_TRUE(ms->mode_data_length >= 3);

        for (ap_page = ms->pages; ap_page; ap_page = ap_page->next) {
                if (ap_page->page_code == SCSI_MODEPAGE_CONTROL &&
                    ap_page->spf == 0) {
                        break;
                }
        }
        if(ap_page == NULL) {
                logging(LOG_NORMAL, "[WARNING] CONTROL page was not returned "
                        "from AllPages. All devices SHOULD implement this "
                        "page.");
        }


        logging(LOG_VERBOSE, "Fetch the CONTROL page directly");
        logging(LOG_VERBOSE, "Send MODESENSE6 command to fetch CONTROL");
        MODESENSE6(sd, &ct_task, 0, SCSI_MODESENSE_PC_CURRENT,
                   SCSI_MODEPAGE_CONTROL, 0, 255,
                   EXPECT_STATUS_GOOD);
        logging(LOG_VERBOSE, "[SUCCESS] CONTROL page fetched.");

        logging(LOG_VERBOSE, "Try to unmarshall the DATA-IN buffer.");
        ms = scsi_datain_unmarshall(ct_task);
        if (ms == NULL) {
                logging(LOG_NORMAL, "[FAILED] failed to unmarshall mode sense "
                        "datain buffer");
                CU_FAIL("[FAILED] Failed to unmarshall the data-in buffer.");
                goto finished;
        }
        logging(LOG_VERBOSE, "[SUCCESS] Unmarshalling successful.");

        logging(LOG_VERBOSE, "Verify that mode data length is >= 3");
        if (ms->mode_data_length >= 3) {
                logging(LOG_VERBOSE, "[SUCCESS] Mode data length is >= 3");
        } else {
                logging(LOG_NORMAL, "[FAILED] Mode data length is < 3");
        }
        CU_ASSERT_TRUE(ms->mode_data_length >= 3);

        for (ct_page = ms->pages; ct_page; ct_page = ct_page->next) {
                if (ct_page->page_code == SCSI_MODEPAGE_CONTROL) {
                        break;
                }
        }
        if(ct_page == NULL) {
                logging(LOG_NORMAL, "[WARNING] CONTROL page was not returned."
                        "All devices SHOULD implement this page.");
        }

        if (ap_page == NULL && ct_page != NULL) {
                logging(LOG_NORMAL, "[FAILED] CONTROL page was not returned "
                        "from AllPages.");
                CU_FAIL("[FAILED] CONTROL page is missing from AllPages");
                goto finished;
        }

        if (ap_page != NULL && ct_page == NULL) {
                logging(LOG_NORMAL, "[FAILED] CONTROL page is only available "
                        "from AllPages but not directly.");
                CU_FAIL("[FAILED] CONTROL page is missing");
                goto finished;
        }

        if (ct_page == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] CONTROL page is not "
                        "implemented.");
                CU_PASS("CONTROL page is not implemented.");
                goto finished;
        }

        logging(LOG_VERBOSE, "Verify that the two pages are identical.");

        logging(LOG_VERBOSE, "Check TST field");
        CU_ASSERT_EQUAL(ct_page->control.tst, ap_page->control.tst);
        logging(LOG_VERBOSE, "Check TMF_ONLY field");
        CU_ASSERT_EQUAL(ct_page->control.tmf_only, ap_page->control.tmf_only);
        logging(LOG_VERBOSE, "Check dpicz field");
        CU_ASSERT_EQUAL(ct_page->control.dpicz, ap_page->control.dpicz);
        logging(LOG_VERBOSE, "Check d_sense field");
        CU_ASSERT_EQUAL(ct_page->control.d_sense, ap_page->control.d_sense);
        logging(LOG_VERBOSE, "Check gltsd field");
        CU_ASSERT_EQUAL(ct_page->control.gltsd, ap_page->control.gltsd);
        logging(LOG_VERBOSE, "Check rlec field");
        CU_ASSERT_EQUAL(ct_page->control.rlec, ap_page->control.rlec);
        logging(LOG_VERBOSE, "Check queue_algorithm_modifier field");
        CU_ASSERT_EQUAL(ct_page->control.queue_algorithm_modifier,
                        ap_page->control.queue_algorithm_modifier);
        logging(LOG_VERBOSE, "Check nuar field");
        CU_ASSERT_EQUAL(ct_page->control.nuar, ap_page->control.nuar);
        logging(LOG_VERBOSE, "Check qerr field");
        CU_ASSERT_EQUAL(ct_page->control.qerr, ap_page->control.qerr);
        logging(LOG_VERBOSE, "Check vs field");
        CU_ASSERT_EQUAL(ct_page->control.vs, ap_page->control.vs);
        logging(LOG_VERBOSE, "Check rac field");
        CU_ASSERT_EQUAL(ct_page->control.rac, ap_page->control.rac);
        logging(LOG_VERBOSE, "Check ua_intlck_ctrl field");
        CU_ASSERT_EQUAL(ct_page->control.ua_intlck_ctrl,
                        ap_page->control.ua_intlck_ctrl);
        logging(LOG_VERBOSE, "Check swp field");
        CU_ASSERT_EQUAL(ct_page->control.swp, ap_page->control.swp);
        logging(LOG_VERBOSE, "Check ato field");
        CU_ASSERT_EQUAL(ct_page->control.ato, ap_page->control.ato);
        logging(LOG_VERBOSE, "Check tas field");
        CU_ASSERT_EQUAL(ct_page->control.tas, ap_page->control.tas);
        logging(LOG_VERBOSE, "Check atmpe field");
        CU_ASSERT_EQUAL(ct_page->control.atmpe, ap_page->control.atmpe);
        logging(LOG_VERBOSE, "Check rwwp field");
        CU_ASSERT_EQUAL(ct_page->control.rwwp, ap_page->control.rwwp);
        logging(LOG_VERBOSE, "Check autoload_mode field");
        CU_ASSERT_EQUAL(ct_page->control.autoload_mode,
                        ap_page->control.autoload_mode);
        logging(LOG_VERBOSE, "Check busy_timeout_period field");
        CU_ASSERT_EQUAL(ct_page->control.busy_timeout_period,
                        ap_page->control.busy_timeout_period);
        logging(LOG_VERBOSE, "Check extended_selftest_completion_time field");
        CU_ASSERT_EQUAL(ct_page->control.extended_selftest_completion_time,
                        ap_page->control.extended_selftest_completion_time);


        logging(LOG_VERBOSE, "Verify that the values are sane.");
        logging(LOG_VERBOSE, "Check that TST is 0 or 1.");
        if (ct_page->control.tst > 1) {
                logging(LOG_NORMAL, "[FAILED] TST value is invalid. Must be "
                        "0, 1 but was %d", ct_page->control.tst);
                CU_FAIL("[FAILED] TST is invalid.");
        }
        logging(LOG_VERBOSE, "Check that QUEUE_ALGORITHM_MODIFIER is "
                "0, 1 or >7");
        if (ct_page->control.queue_algorithm_modifier > 1 &&
            ct_page->control.queue_algorithm_modifier < 8) {
                logging(LOG_NORMAL, "[FAILED] QUEUE_ALGORITHM_MODIFIER value "
                        "is invalid. Must be 0, 1 or >7 but was %d",
                        ct_page->control.queue_algorithm_modifier);
                CU_FAIL("[FAILED] QUEUE_ALGORITHM_MODIFIER is invalid.");
        }

        logging(LOG_VERBOSE, "Check that QERR is not 2");
        if (ct_page->control.qerr == 2) {
                logging(LOG_NORMAL, "[FAILED] QERR value "
                        "is invalid. Can not be 2");
                CU_FAIL("[FAILED] QERR is invalid.");
        }

        logging(LOG_VERBOSE, "Check that UA_INTLCK_CTRL is not 1");
        if (ct_page->control.ua_intlck_ctrl == 1) {
                logging(LOG_NORMAL, "[FAILED] UA_INTLCK_CTRL value "
                        "is invalid. Can not be 1");
                CU_FAIL("[FAILED] UA_INTLCK_CTRL is invalid.");
        }

        logging(LOG_VERBOSE, "Check that AUTOLOAD is 0, 1 or 2");
        if (ct_page->control.autoload_mode > 2) {
                logging(LOG_NORMAL, "[FAILED] AUTOLOAD value "
                        "is invalid. Must be 0, 1 or 2 but was %d",
                        ct_page->control.autoload_mode);
                CU_FAIL("[FAILED] AUTOLOAD is invalid.");
        }

        logging(LOG_VERBOSE, "Check that BUSY_TIMEOUT_PERIOD is specified");
        if (ct_page->control.busy_timeout_period == 0) {
                logging(LOG_NORMAL, "[WARNING] BUSY_TIMEOUT_PERIOD is "
                        "undefined.");
        }



finished:
        if (ap_task != NULL) {
                scsi_free_scsi_task(ap_task);
        }
        if (ct_task != NULL) {
                scsi_free_scsi_task(ct_task);
        }
}
