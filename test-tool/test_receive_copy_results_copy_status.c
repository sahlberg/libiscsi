/*
   Copyright (c) 2015 SanDisk Corp.

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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"



void
test_receive_copy_results_copy_status(void)
{
        struct scsi_task *cs_task = NULL;
        struct scsi_copy_results_copy_status *csp;
        int tgt_desc_len = 0, seg_desc_len = 0;
        int  offset = XCOPY_DESC_OFFSET, list_id = 1;
        struct iscsi_data data;
        unsigned char *xcopybuf;
        struct scsi_inquiry_supported_pages *sup_inq;
        struct scsi_inquiry_third_party_copy *third_party_inq;
        struct third_party_copy_command_support *command;
        bool third_party_page_supported = false;
        bool receive_copy_results_supported = false;
        int ret, i;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test RECEIVE COPY RESULTS, COPY STATUS");

        logging(LOG_VERBOSE, "Get VPD pages supported list.");
        ret = inquiry(sd, &task,
                      1, SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES, 255,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret != 0) {
                logging(LOG_NORMAL, "[FAILED] Failed to get Supported VPD Pages.");
                return;
        }

        logging(LOG_VERBOSE, "Check that RECEIVE COPY RESULTS is supported.");
        sup_inq = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(sup_inq, NULL);
        if (sup_inq == NULL) {
                logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN "
                        "buffer.");
                return;
        }

        for (i = 0; i < sup_inq->num_pages; i++) {
                if (sup_inq->pages[i] == SCSI_INQUIRY_PAGECODE_THIRD_PARTY_COPY) {
                        third_party_page_supported = true;
                        break;
                }
        }
        if (!third_party_page_supported) {
                logging(LOG_NORMAL, "[SKIPPED] Third-party Copy VPD page is not "
                        "implemented.");
                CU_PASS("RECEIVE COPY RESULTS is not implemented.");
                goto finished;
        }
        logging(LOG_VERBOSE, "Third-party Copy VPD page is supported.");

        scsi_free_scsi_task(task);
        task = NULL;

        ret = inquiry(sd, &task,
                      1, SCSI_INQUIRY_PAGECODE_THIRD_PARTY_COPY, 1024,
                      EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        if (ret != 0) {
                logging(LOG_NORMAL, "[FAILED] Failed to get Third-party Copy "
                        "VPD page.");
                return;
        }

        third_party_inq = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(third_party_inq, NULL);
        if (third_party_inq == NULL) {
                logging(LOG_NORMAL, "[FAILED] Failed to unmarshall DATA-IN "
                        "buffer.");
                return;
        }
        CU_ASSERT_NOT_EQUAL(third_party_inq->supported_commands, NULL);
        if (third_party_inq->supported_commands == NULL) {
                logging(LOG_NORMAL, "[FAILED] Supported Commands descriptor is "
                        "is not found.");
                return;
        }

        command = third_party_inq->supported_commands->commands_supported;
        while (command && !receive_copy_results_supported) {
                if (command->operation_code == SCSI_OPCODE_RECEIVE_COPY_RESULTS) {
                        for (i = 0; i < command->service_action_length; i++) {
                                if (command->service_action[i] ==
                                            SCSI_COPY_RESULTS_COPY_STATUS) {
                                        receive_copy_results_supported = true;
                                        break;
                                }
                        }
                }

                command = command->next;
        }
        if (!receive_copy_results_supported) {
                logging(LOG_NORMAL, "[SKIPPED] RECEIVE COPY RESULTS is not "
                        "implemented.");
                CU_PASS("RECEIVE COPY RESULTS is not implemented.");
                goto finished;
        }
        logging(LOG_VERBOSE, "RECEIVE COPY RESULTS is supported.");

        scsi_free_scsi_task(task);
        task = NULL;

        logging(LOG_VERBOSE, "No copy in progress");
        RECEIVE_COPY_RESULTS(&cs_task, sd, SCSI_COPY_RESULTS_COPY_STATUS,
                             list_id, NULL, EXPECT_INVALID_FIELD_IN_CDB);
        scsi_free_scsi_task(cs_task);
        cs_task = NULL;

        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Issue Extended Copy");
        data.size = XCOPY_DESC_OFFSET +
                get_desc_len(IDENT_DESCR_TGT_DESCR) +
                get_desc_len(BLK_TO_BLK_SEG_DESCR);
        data.data = alloca(data.size);
        xcopybuf = data.data;
        memset(xcopybuf, 0, data.size);

        /* Initialize target descriptor list with one target descriptor */
        offset += populate_tgt_desc(xcopybuf+offset, IDENT_DESCR_TGT_DESCR,
                        LU_ID_TYPE_LUN, 0, 0, 0, 0, sd);
        tgt_desc_len = offset - XCOPY_DESC_OFFSET;

        /* Initialize segment descriptor list with one segment descriptor */
        offset += populate_seg_desc_b2b(xcopybuf+offset, 0, 0, 0, 0,
                        2048, 0, num_blocks - 2048);
        seg_desc_len = offset - XCOPY_DESC_OFFSET - tgt_desc_len;

        /* Initialize the parameter list header */
        populate_param_header(xcopybuf, list_id, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);

        EXTENDEDCOPY(sd, &data, EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE,
                        "Copy Status for the above Extended Copy command");
        RECEIVE_COPY_RESULTS(&cs_task, sd, SCSI_COPY_RESULTS_COPY_STATUS,
                             list_id, (void **)&csp, EXPECT_STATUS_GOOD);

finished:
        if (task != NULL) {
                scsi_free_scsi_task(task);
                task = NULL;
        }
        if (cs_task != NULL) {
                scsi_free_scsi_task(cs_task);
                cs_task = NULL;
        }
}
