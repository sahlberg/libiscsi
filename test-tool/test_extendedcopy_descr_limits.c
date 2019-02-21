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
#include "iscsi-private.h"

int init_xcopy_descr(unsigned char *buf, int offset, int num_tgt_desc,
                int num_seg_desc, int *tgt_desc_len, int *seg_desc_len)
{
        int i;

        /* Initialize target descriptor list with num_tgt_desc
         * target descriptor */
        for (i = 0; i < num_tgt_desc; i++)
                offset += populate_tgt_desc(buf+offset, IDENT_DESCR_TGT_DESCR,
                                LU_ID_TYPE_LUN, 0, 0, 0, 0, sd);
        *tgt_desc_len = offset - XCOPY_DESC_OFFSET;

        /* Initialize segment descriptor list with num_seg_desc
         * segment descriptor */
        for (i = 0; i < num_seg_desc; i++)
                offset += populate_seg_desc_b2b(buf+offset, 0, 0, 0, 0,
                                2048, 0, num_blocks - 2048);
        *seg_desc_len = offset - XCOPY_DESC_OFFSET - *tgt_desc_len;

        return offset;
}

void
test_extendedcopy_descr_limits(void)
{
        struct scsi_task *edl_task;
        struct iscsi_data data;
        unsigned char *xcopybuf;
        struct scsi_copy_results_op_params *opp = NULL;
        int tgt_desc_len = 0, seg_desc_len = 0, seg_desc_count;
        unsigned int alloc_len;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test EXTENDED COPY descriptor limits");

        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Issue RECEIVE COPY RESULTS (OPERATING PARAMS)");
        RECEIVE_COPY_RESULTS(&edl_task, sd, SCSI_COPY_RESULTS_OP_PARAMS, 0,
                             (void **)&opp, EXPECT_STATUS_GOOD);

        CU_ASSERT_NOT_EQUAL(opp, NULL);
        if (!opp)
                return;

        /* Allocate buffer to accommodate (MAX+1) target and
         * segment descriptors */
        alloc_len = XCOPY_DESC_OFFSET +
                (opp->max_target_desc_count+1) *
                get_desc_len(IDENT_DESCR_TGT_DESCR) +
                (opp->max_segment_desc_count+1) *
                get_desc_len(BLK_TO_BLK_SEG_DESCR);
        alloc_len = MAX(alloc_len, (XCOPY_DESC_OFFSET
                                    + opp->max_desc_list_length
                                    + get_desc_len(BLK_TO_BLK_SEG_DESCR)));
        data.data = malloc(alloc_len);
        xcopybuf = data.data;
        memset(xcopybuf, 0, alloc_len);

        logging(LOG_VERBOSE,
                        "Test sending more than supported target descriptors");
        data.size = init_xcopy_descr(xcopybuf, XCOPY_DESC_OFFSET,
                        (opp->max_target_desc_count+1), 1,
                        &tgt_desc_len, &seg_desc_len);
        populate_param_header(xcopybuf, 1, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);
        EXTENDEDCOPY(sd, &data, EXPECT_TOO_MANY_DESCR);

        logging(LOG_VERBOSE,
                        "Test sending more than supported segment descriptors");
        memset(xcopybuf, 0, alloc_len);
        data.size = init_xcopy_descr(xcopybuf, XCOPY_DESC_OFFSET, 1,
                        (opp->max_segment_desc_count+1),
                        &tgt_desc_len, &seg_desc_len);
        populate_param_header(xcopybuf, 2, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);
        EXTENDEDCOPY(sd, &data, EXPECT_TOO_MANY_DESCR);

        logging(LOG_VERBOSE,
                        "Test sending descriptors > Maximum Descriptor List Length");
        memset(xcopybuf, 0, alloc_len);
        tgt_desc_len = (opp->max_target_desc_count + 1)
                                * get_desc_len(IDENT_DESCR_TGT_DESCR);
        /* Overfill remaining max_desc_list_length with segment descriptors */
        seg_desc_len = alloc_len - XCOPY_DESC_OFFSET - tgt_desc_len;
        seg_desc_count = seg_desc_len / get_desc_len(BLK_TO_BLK_SEG_DESCR);
        data.size = init_xcopy_descr(xcopybuf, XCOPY_DESC_OFFSET,
                        (opp->max_target_desc_count+1),
                        seg_desc_count,
                        &tgt_desc_len, &seg_desc_len);
        populate_param_header(xcopybuf, 3, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);
        EXTENDEDCOPY(sd, &data, EXPECT_PARAM_LIST_LEN_ERR);

        scsi_free_scsi_task(edl_task);
        free(xcopybuf);
}
