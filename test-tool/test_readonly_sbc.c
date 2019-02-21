/* 
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

static void
test_write10(void)
{
        logging(LOG_VERBOSE, "Test WRITE10 fails with WRITE_PROTECTED");
        memset(scratch, 0xa6, block_size);
        WRITE10(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_WRITE_PROTECTED);
}

static void
test_write12(void)
{
        logging(LOG_VERBOSE, "Test WRITE12 fails with WRITE_PROTECTED");
        memset(scratch, 0xa6, block_size);
        WRITE12(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_WRITE_PROTECTED);
}

static void
test_write16(void)
{
        logging(LOG_VERBOSE, "Test WRITE16 fails with WRITE_PROTECTED");
        memset(scratch, 0xa6, block_size);
        WRITE16(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_WRITE_PROTECTED);
}

static void
test_writesame10(void)
{
        logging(LOG_VERBOSE, "Test WRITE_SAME10 fails with WRITE_PROTECTED");
        WRITESAME10(sd, 0, block_size, 1, 0, 0, 0, 0, scratch,
                    EXPECT_WRITE_PROTECTED);

        logging(LOG_VERBOSE, "Test WRITE_SAME10 UNMAP fails with "
                "WRITE_PROTECTED");
        WRITESAME10(sd, 0, block_size, 1, 0, 1, 0, 0, NULL,
                    EXPECT_WRITE_PROTECTED);
}

static void
test_writesame16(void)
{
        logging(LOG_VERBOSE, "Test WRITE_SAME16 fails with WRITE_PROTECTED");
        WRITESAME16(sd, 0, block_size, 1, 0, 0, 0, 0, scratch,
                    EXPECT_WRITE_PROTECTED);

        logging(LOG_VERBOSE, "Test WRITE_SAME16 UNMAP fails with "
                "WRITE_PROTECTED");
        WRITESAME16(sd, 0, block_size, 1, 0, 1, 0, 0, NULL,
                    EXPECT_WRITE_PROTECTED);
}

static void
test_writeverify10(void)
{
        logging(LOG_VERBOSE, "Test WRITEVERIFY10 fails with WRITE_PROTECTED");
        WRITEVERIFY10(sd, 0, block_size, block_size, 0, 0, 1, 0, scratch,
                      EXPECT_WRITE_PROTECTED);
}

static void
test_writeverify12(void)
{
        logging(LOG_VERBOSE, "Test WRITEVERIFY12 fails with WRITE_PROTECTED");
        WRITEVERIFY12(sd, 0, block_size, block_size, 0, 0, 1, 0, scratch,
                      EXPECT_WRITE_PROTECTED);
}

static void
test_writeverify16(void)
{
        logging(LOG_VERBOSE, "Test WRITEVERIFY16 fails with WRITE_PROTECTED");
        WRITEVERIFY16(sd, 0, block_size, block_size, 0, 0, 1, 0, scratch,
                      EXPECT_WRITE_PROTECTED);
}

static void
test_orwrite(void)
{
        logging(LOG_VERBOSE, "Test ORWRITE fails with WRITE_PROTECTED");
        ORWRITE(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_WRITE_PROTECTED);
}

static void
test_compareandwrite(void)
{
        logging(LOG_VERBOSE, "Test COMPAREANDWRITE fails with WRITE_PROTECTED");
        COMPAREANDWRITE(sd, 0, scratch, 2 * block_size, block_size, 0, 0, 0, 0,
                        EXPECT_WRITE_PROTECTED);
}

static void
test_unmap(void)
{
        struct unmap_list list[1];

        logging(LOG_VERBOSE, "Test UNMAP of one physical block fails with "
                "WRITE_PROTECTED");
        list[0].lba = 0;
        list[0].num = lbppb;
        UNMAP(sd, 0, list, 1,
              EXPECT_WRITE_PROTECTED);

        logging(LOG_VERBOSE, "Test UNMAP of one logical block fails with "
                "WRITE_PROTECTED");
        list[0].lba = 0;
        list[0].num = 1;
        UNMAP(sd, 0, list, 1,
              EXPECT_WRITE_PROTECTED);
}

void
test_readonly_sbc(void)
{
        CHECK_FOR_DATALOSS;
        CHECK_FOR_READONLY;
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that Medium write commands fail for READ-ONLY SBC devices");

        test_compareandwrite();
        test_orwrite();
        test_unmap();
        test_write10();
        test_write12();
        test_write16();
        test_writesame10();
        test_writesame16();
        test_writeverify10();
        test_writeverify12();
        test_writeverify16();
}
