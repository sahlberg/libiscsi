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
        struct scsi_task *cs_task;
        struct scsi_copy_results_copy_status *csp;
        int tgt_desc_len = 0, seg_desc_len = 0;
        int  offset = XCOPY_DESC_OFFSET, list_id = 1;
        struct iscsi_data data;
        unsigned char *xcopybuf;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test RECEIVE COPY RESULTS, COPY STATUS");

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

        scsi_free_scsi_task(cs_task);
}
