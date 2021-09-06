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
        int tgt_desc_len = 0, seg_desc_len = 0, offset = XCOPY_DESC_OFFSET, len;
        struct iscsi_data data;
        unsigned char *xcopybuf;
        unsigned int copied_blocks;
        unsigned char *buf1;
        unsigned char *buf2;
        uint64_t cp_dst_lba;

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

        cp_dst_lba = num_blocks - copied_blocks;
        logging(LOG_VERBOSE, "Zero %u blocks at the end of the LUN (LBA:%llu)",
                copied_blocks, (unsigned long long)cp_dst_lba);
        memset(buf1, '\0', copied_blocks * block_size);
        WRITE16(sd, cp_dst_lba, copied_blocks * block_size,
                block_size, 0, 0, 0, 0, 0, buf1, EXPECT_STATUS_GOOD);
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
        len = populate_tgt_desc(xcopybuf+offset, IDENT_DESCR_TGT_DESCR,
                        LU_ID_TYPE_LUN, 0, 0, 0, 0, sd);
        if (len < 0) {
                CU_FAIL("Populating target descriptor failed");
                goto free;
        }
        offset += len;
        tgt_desc_len = offset - XCOPY_DESC_OFFSET;

        /* Initialize segment descriptor list with one segment descriptor */
        offset += populate_seg_desc_b2b(xcopybuf+offset, 0, 0, 0, 0,
                        copied_blocks, 0, cp_dst_lba);
        seg_desc_len = offset - XCOPY_DESC_OFFSET - tgt_desc_len;

        /* Initialize the parameter list header */
        populate_param_header(xcopybuf, 1, 0, LIST_ID_USAGE_DISCARD, 0,
                        tgt_desc_len, seg_desc_len, 0);

        EXTENDEDCOPY(sd, &data, EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Read %u blocks from end of the LUN",
                copied_blocks);
        READ16(sd, NULL, cp_dst_lba, copied_blocks * block_size,
               block_size, 0, 0, 0, 0, 0, buf2,
               EXPECT_STATUS_GOOD);

        if (memcmp(buf1, buf2, copied_blocks * block_size)) {
                CU_FAIL("Blocks were not copied correctly");
        }

free:
        free(buf1);
        free(buf2);
}

/* to save time, avoid copying more than 256M */
#define TEST_XCOPY_LARGE_CP_LEN_MAX (256 * 1024 * 1024)
void
test_extendedcopy_large(void)
{
        struct scsi_task *edl_task = NULL;
        struct scsi_copy_results_op_params *opp = NULL;
        uint32_t cp_len_bytes = 0;
        int i, tgt_desc_len = 0, seg_desc_len = 0, offset = XCOPY_DESC_OFFSET;
        int len;
        struct iscsi_data data;
        unsigned char *xcopybuf;
        unsigned int write_blocks;
        unsigned char *buf1;
        unsigned char *buf2;
        uint64_t cp_dst_lba;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE,
                "Test large EXTENDED COPY with type 0x0A segment descriptor");

        CHECK_FOR_DATALOSS;

        /* check for 0x0A type SD support, which offers 32-bit lengths */
        RECEIVE_COPY_RESULTS(&edl_task, sd, SCSI_COPY_RESULTS_OP_PARAMS, 0,
                             (void **)&opp, EXPECT_STATUS_GOOD);
        CU_ASSERT_NOT_EQUAL(opp, NULL);

        for (i = 0; i < opp->impl_desc_list_length; i++) {
                if (opp->imp_desc_type_codes[i] == BLK_TO_BLK_OFF_SEG_DESCR) {
                        cp_len_bytes = opp->max_segment_length;
                        break;
                }
        }
        scsi_free_scsi_task(edl_task);

        if (cp_len_bytes == 0) {
                logging(LOG_NORMAL,
                        "[SKIPPED] BLK_TO_BLK_OFF_SEG_DESCR not supported");
                CU_PASS("[SKIPPED] BLK_TO_BLK_OFF_SEG_DESCR not supported");
                return;
        }
        if (cp_len_bytes > TEST_XCOPY_LARGE_CP_LEN_MAX) {
                cp_len_bytes = TEST_XCOPY_LARGE_CP_LEN_MAX;
        }
        if (num_blocks < (cp_len_bytes / block_size) * 2) {
                logging(LOG_NORMAL,
                        "[SKIPPED] device too small to handle maxlen XCOPY");
                CU_PASS("[SKIPPED] device too small to handle maxlen XCOPY");
                return;
        }

        write_blocks = num_blocks / 2;
        if (write_blocks > 2048)
                write_blocks = 2048;
        buf1 = malloc(write_blocks * block_size);
        buf2 = malloc(write_blocks * block_size);

        cp_dst_lba = num_blocks - (cp_len_bytes / block_size);
        logging(LOG_VERBOSE, "Zero %u blocks at the end of the LUN (LBA:%llu)",
                write_blocks, (unsigned long long)cp_dst_lba);
        memset(buf1, '\0', write_blocks * block_size);
        WRITE16(sd, cp_dst_lba, write_blocks * block_size,
                block_size, 0, 0, 0, 0, 0, buf1, EXPECT_STATUS_GOOD);
        logging(LOG_VERBOSE, "Write %u blocks of 'B' at LBA:0", write_blocks);
        memset(buf1, 'B', write_blocks * block_size);
        WRITE16(sd, 0, write_blocks * block_size, block_size, 0, 0, 0, 0, 0,
                buf1, EXPECT_STATUS_GOOD);

        data.size = XCOPY_DESC_OFFSET +
                get_desc_len(IDENT_DESCR_TGT_DESCR) +
                get_desc_len(BLK_TO_BLK_OFF_SEG_DESCR);
        data.data = alloca(data.size);
        xcopybuf = data.data;
        memset(xcopybuf, 0, data.size);

        /* Initialize target descriptor list with one target descriptor */
        len = populate_tgt_desc(xcopybuf+offset, IDENT_DESCR_TGT_DESCR,
                        LU_ID_TYPE_LUN, 0, 0, 0, 0, sd);
        if (len < 0) {
                CU_FAIL("Populating target descriptor failed");
                goto free;
        }
        offset += len;
        tgt_desc_len = offset - XCOPY_DESC_OFFSET;

        /* Initialize segment descriptor list with one segment descriptor */
        offset += populate_seg_desc_b2b_off(xcopybuf+offset, 0, 0, 0,
                                  cp_len_bytes, 0, cp_dst_lba, 0, 0);
        seg_desc_len = offset - XCOPY_DESC_OFFSET - tgt_desc_len;

        /* Initialize the parameter list header */
        populate_param_header(xcopybuf, 1, 0, LIST_ID_USAGE_DISCARD, 0,
                        tgt_desc_len, seg_desc_len, 0);

        EXTENDEDCOPY(sd, &data, EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Read %u blocks from end of the LUN",
                write_blocks);
        READ16(sd, NULL, cp_dst_lba, write_blocks * block_size,
               block_size, 0, 0, 0, 0, 0, buf2,
               EXPECT_STATUS_GOOD);

        if (memcmp(buf1, buf2, write_blocks * block_size)) {
                CU_FAIL("Blocks were not copied correctly");
        }

free:
        free(buf1);
        free(buf2);
}
