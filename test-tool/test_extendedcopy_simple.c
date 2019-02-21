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
test_extendedcopy_simple(void)
{
        int tgt_desc_len = 0, seg_desc_len = 0, offset = XCOPY_DESC_OFFSET;
        struct iscsi_data data;
        unsigned char *xcopybuf;
        unsigned int copied_blocks;
        unsigned char *buf1;
        unsigned char *buf2;


        copied_blocks = num_blocks / 2;
        if (copied_blocks > 2048)
                copied_blocks = 2048;
        buf1 = malloc(copied_blocks * block_size);
        buf2 = malloc(copied_blocks * block_size);

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE,
                "Test EXTENDED COPY of %u blocks from start of LUN to end of LUN",
                copied_blocks);

        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Write %u blocks of 'A' at LBA:0", copied_blocks);
        memset(buf1, 'A', copied_blocks * block_size);
        WRITE16(sd, 0, copied_blocks * block_size, block_size, 0, 0, 0, 0, 0,
                buf1, EXPECT_STATUS_GOOD);

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

        /* Iniitialize segment descriptor list with one segment descriptor */
        offset += populate_seg_desc_b2b(xcopybuf+offset, 0, 0, 0, 0,
                        copied_blocks, 0, num_blocks - copied_blocks);
        seg_desc_len = offset - XCOPY_DESC_OFFSET - tgt_desc_len;

        /* Initialize the parameter list header */
        populate_param_header(xcopybuf, 1, 0, LIST_ID_USAGE_DISCARD, 0,
                        tgt_desc_len, seg_desc_len, 0);

        EXTENDEDCOPY(sd, &data, EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Read %u blocks from end of the LUN",
                copied_blocks);
        READ16(sd, NULL, num_blocks - copied_blocks, copied_blocks * block_size,
               block_size, 0, 0, 0, 0, 0, buf2,
               EXPECT_STATUS_GOOD);

        if (memcmp(buf1, buf2, copied_blocks * block_size)) {
                CU_FAIL("Blocks were not copied correctly");
        }
        
        free(buf1);
        free(buf2);
}
