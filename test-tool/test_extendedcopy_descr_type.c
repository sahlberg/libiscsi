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

int init_xcopybuf(unsigned char *buf, int tgt_desc_type, int seg_desc_type,
                int *tgt_desc_len, int *seg_desc_len)
{
        int offset = XCOPY_DESC_OFFSET;

        offset += populate_tgt_desc(buf+offset, tgt_desc_type, LU_ID_TYPE_LUN,
                        0, 0, 0, 0, sd);
        *tgt_desc_len = offset - XCOPY_DESC_OFFSET;
        if (seg_desc_type == BLK_TO_BLK_SEG_DESCR)
                offset += populate_seg_desc_b2b(buf+offset, 0, 0, 0, 0, 2048, 0,
                                num_blocks - 2048);
        else
                offset += populate_seg_desc_hdr(buf+offset, seg_desc_type,
                                0, 0, 0, 0);
        *seg_desc_len = offset - XCOPY_DESC_OFFSET - *tgt_desc_len;

        return offset;
}

void
test_extendedcopy_descr_type(void)
{
        int tgt_desc_len = 0, seg_desc_len = 0, alloc_len;
        struct iscsi_data data;
        unsigned char *xcopybuf;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE,
                        "Test EXTENDED COPY unsupported descriptor types");

        CHECK_FOR_DATALOSS;

        alloc_len = XCOPY_DESC_OFFSET +
                get_desc_len(IDENT_DESCR_TGT_DESCR) +
                get_desc_len(BLK_TO_BLK_SEG_DESCR);
        data.data = alloca(alloc_len);
        xcopybuf = data.data;
        memset(xcopybuf, 0, alloc_len);

        logging(LOG_VERBOSE,
                        "Send Fibre Channel N_Port_Name target descriptor");
        data.size = init_xcopybuf(xcopybuf, 0xE0, BLK_TO_BLK_SEG_DESCR,
                        &tgt_desc_len, &seg_desc_len);
        populate_param_header(xcopybuf, 1, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);

        EXTENDEDCOPY(sd, &data, EXPECT_UNSUPP_DESCR_CODE);

        logging(LOG_VERBOSE, "Send Stream-to-Stream Copy segment descriptor");
        memset(xcopybuf, 0, alloc_len);
        data.size = init_xcopybuf(xcopybuf, IDENT_DESCR_TGT_DESCR,
                        STRM_TO_STRM_SEG_DESCR,
                        &tgt_desc_len, &seg_desc_len);
        populate_param_header(xcopybuf, 1, 0, 0, 0,
                        tgt_desc_len, seg_desc_len, 0);

        EXTENDEDCOPY(sd, &data, EXPECT_UNSUPP_DESCR_CODE);
}
