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
test_extendedcopy_param(void)
{
        int tgt_desc_len = 0, seg_desc_len = 0, offset = XCOPY_DESC_OFFSET;
        struct iscsi_data data;
        unsigned char *xcopybuf;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test EXTENDED COPY parameter list length");

        CHECK_FOR_DATALOSS;

        data.size = XCOPY_DESC_OFFSET +
                get_desc_len(IDENT_DESCR_TGT_DESCR) +
                get_desc_len(BLK_TO_BLK_SEG_DESCR);
        data.data = alloca(data.size);
        xcopybuf = data.data;
        memset(xcopybuf, 0, data.size);

        offset += populate_tgt_desc(xcopybuf+offset, IDENT_DESCR_TGT_DESCR,
                        LU_ID_TYPE_LUN, 0, 0, 0, 0, sd);
        tgt_desc_len = offset - XCOPY_DESC_OFFSET;

        offset += populate_seg_desc_b2b(xcopybuf+offset, 0, 0, 0, 0,
                        2048, 0, num_blocks - 2048);
        seg_desc_len = offset - XCOPY_DESC_OFFSET - tgt_desc_len;

        populate_param_header(xcopybuf, 1, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);

        logging(LOG_VERBOSE,
                        "Test parameter list length truncating target descriptor");
        data.size = XCOPY_DESC_OFFSET +
                get_desc_len(IDENT_DESCR_TGT_DESCR) - 1;
        EXTENDEDCOPY(sd, &data, EXPECT_PARAM_LIST_LEN_ERR);

        logging(LOG_VERBOSE,
                        "Test parameter list length truncating segment descriptor");
        data.size = XCOPY_DESC_OFFSET +
                get_desc_len(IDENT_DESCR_TGT_DESCR) +
                get_desc_len(BLK_TO_BLK_SEG_DESCR) - 1;
        EXTENDEDCOPY(sd, &data, EXPECT_PARAM_LIST_LEN_ERR);

        logging(LOG_VERBOSE, "Test parameter list length = 0");
        data.size = 0;
        EXTENDEDCOPY(sd, &data, EXPECT_STATUS_GOOD);
}
