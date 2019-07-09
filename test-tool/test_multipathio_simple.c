/*
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   Copyright (C) 2015 David Disseldorp

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
#include "iscsi-multipath.h"

void
test_multipathio_simple(void)
{
        int write_path;
        unsigned char *write_buf = alloca(256 * block_size);
        unsigned char *read_buf = alloca(256 * block_size);
        int ret;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;
        MPATH_SKIP_IF_UNAVAILABLE(mp_sds, mp_num_sds);

        logging(LOG_VERBOSE, LOG_BLANK_LINE);

        for (write_path = 0; write_path < mp_num_sds; write_path++) {
                int i;
                int read_path;

                memset(write_buf, 0xa6 ^ write_path, 256 * block_size);

                /* read back written data using a different path */
                read_path = (write_path + 1) % mp_num_sds;

                logging(LOG_VERBOSE,
                        "Test multipath WRITE10/READ10 of 1-256 blocks using "
                        "path %d", write_path);

                for (i = 1; i <= 256; i++) {
                        if (maximum_transfer_length
                                        && maximum_transfer_length < i) {
                                break;
                        }
                        ret = WRITE10(mp_sds[write_path], 0, i * block_size,
                                      block_size, 0, 0, 0, 0, 0, write_buf,
                                      EXPECT_STATUS_GOOD);
                        if (ret < 0)
                                continue;
                        ret = READ10(mp_sds[read_path], NULL, 0, i * block_size,
                                     block_size, 0, 0, 0, 0, 0, read_buf,
                                     EXPECT_STATUS_GOOD);
                        if (ret < 0)
                                continue;
                        /* compare written and read data */
                        ret = memcmp(write_buf, read_buf, i * block_size);
                        CU_ASSERT_EQUAL(0, ret);
                }

        }
}
