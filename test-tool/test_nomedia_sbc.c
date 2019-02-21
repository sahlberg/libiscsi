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
test_synchronizecache10(void)
{
        logging(LOG_VERBOSE, "Test SYNCHRONIZECACHE10 when medium is ejected.");
        SYNCHRONIZECACHE10(sd, 0, 1, 1, 1,
                           EXPECT_NO_MEDIUM);
}

static void
test_synchronizecache16(void)
{
        logging(LOG_VERBOSE, "Test SYNCHRONIZECACHE16 when medium is ejected.");
        SYNCHRONIZECACHE16(sd, 0, 1, 1, 1,
                           EXPECT_NO_MEDIUM);
}

static void
test_read10(void)
{
        logging(LOG_VERBOSE, "Test READ10 when medium is ejected.");
        READ10(sd, NULL, 0, block_size, block_size, 0, 0, 0, 0, 0, NULL,
               EXPECT_NO_MEDIUM);
}

static void
test_read12(void)
{
        logging(LOG_VERBOSE, "Test READ12 when medium is ejected.");
        READ12(sd, NULL, 0, block_size, block_size, 0, 0, 0, 0, 0, NULL,
               EXPECT_NO_MEDIUM);
}

static void
test_read16(void)
{
        logging(LOG_VERBOSE, "Test READ16 when medium is ejected.");
        READ16(sd, NULL, 0, block_size, block_size, 0, 0, 0, 0, 0, NULL,
               EXPECT_NO_MEDIUM);
}

static void
test_write10(void)
{
        logging(LOG_VERBOSE, "Test WRITE10 when medium is ejected.");
        WRITE10(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_NO_MEDIUM);
}

static void
test_write12(void)
{
        logging(LOG_VERBOSE, "Test WRITE12 when medium is ejected.");
        WRITE12(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_NO_MEDIUM);
}

static void
test_write16(void)
{
        logging(LOG_VERBOSE, "Test WRITE16 when medium is ejected.");
        WRITE16(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_NO_MEDIUM);
}

static void
test_writeverify10(void)
{
        logging(LOG_VERBOSE, "Test WRITEVERIFY10 when medium is ejected.");
        WRITEVERIFY10(sd, 0, block_size, block_size, 0, 0, 1, 0, scratch,
                      EXPECT_NO_MEDIUM);
}

static void
test_writeverify12(void)
{
        logging(LOG_VERBOSE, "Test WRITEVERIFY12 when medium is ejected.");
        WRITEVERIFY12(sd, 0, block_size, block_size, 0, 0, 1, 0, scratch,
                      EXPECT_NO_MEDIUM);
}

static void
test_writeverify16(void)
{
        logging(LOG_VERBOSE, "Test WRITEVERIFY16 when medium is ejected.");
        WRITEVERIFY16(sd, 0, block_size, block_size, 0, 0, 1, 0, scratch,
                      EXPECT_NO_MEDIUM);
}

static void
test_verify10(void)
{
        logging(LOG_VERBOSE, "Test VERIFY10 when medium is ejected.");
        VERIFY10(sd, 0, block_size, block_size, 0, 0, 1, scratch,
                 EXPECT_NO_MEDIUM);
}
static void
test_verify12(void)
{
        logging(LOG_VERBOSE, "Test VERIFY12 when medium is ejected.");
        VERIFY12(sd, 0, block_size, block_size, 0, 0, 1, scratch,
                 EXPECT_NO_MEDIUM);
}
static void
test_verify16(void)
{
        logging(LOG_VERBOSE, "Test VERIFY16 when medium is ejected.");
        VERIFY16(sd, 0, block_size, block_size, 0, 0, 1, scratch,
                 EXPECT_NO_MEDIUM);
}

static void
test_getlbastatus(void)
{
        logging(LOG_VERBOSE, "Test GET_LBA_STATUS when medium is ejected.");
        GETLBASTATUS(sd, NULL, 0, 24,
                     EXPECT_NO_MEDIUM);
}

static void
test_prefetch10(void)
{
        logging(LOG_VERBOSE, "Test PREFETCH10 when medium is ejected.");
        PREFETCH10(sd, 0, 1, 1, 0,
                   EXPECT_NO_MEDIUM);
}

static void
test_prefetch16(void)
{
        logging(LOG_VERBOSE, "Test PREFETCH16 when medium is ejected.");
        PREFETCH16(sd, 0, 1, 1, 0,
                   EXPECT_NO_MEDIUM);
}

static void
test_orwrite(void)
{
        logging(LOG_VERBOSE, "Test ORWRITE when medium is ejected.");
        ORWRITE(sd, 0, block_size, block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_NO_MEDIUM);
}

static void
test_compareandwrite(void)
{
        logging(LOG_VERBOSE, "Test COMPAREANDWRITE when medium is ejected.");
        COMPAREANDWRITE(sd, 0, scratch, 2 * block_size, block_size, 0, 0, 0, 0,
                        EXPECT_NO_MEDIUM);
}

static void
test_writesame10(void)
{
        logging(LOG_VERBOSE, "Test WRITESAME10 when medium is ejected.");
        WRITESAME10(sd, 0, block_size, 1, 0, 0, 0, 0, scratch,
                    EXPECT_NO_MEDIUM);
}

static void
test_writesame16(void)
{
        logging(LOG_VERBOSE, "Test WRITESAME16 when medium is ejected.");
        WRITESAME16(sd, 0, block_size, 1, 0, 0, 0, 0, scratch,
                    EXPECT_NO_MEDIUM);
}

static void
test_unmap(void)
{
        struct unmap_list list[1];

        logging(LOG_VERBOSE, "Test UNMAP when medium is ejected.");
        list[0].lba = 0;
        list[0].num = lbppb;
        UNMAP(sd, 0, list, 1,
                    EXPECT_NO_MEDIUM);
}

static void
test_readcapacity10(void)
{
        logging(LOG_VERBOSE, "Test READCAPACITY10 when medium is ejected.");
        READCAPACITY10(sd, NULL, 0, 0,
                       EXPECT_NO_MEDIUM);
}

static void
test_readcapacity16(void)
{
        logging(LOG_VERBOSE, "Test READCAPACITY16 when medium is ejected.");
        READCAPACITY16(sd, NULL, 15,
                       EXPECT_NO_MEDIUM);
}

void
test_nomedia_sbc(void)
{
        CHECK_FOR_SBC;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that Medium commands fail when medium is ejected on SBC devices");

        if (!inq->rmb) {
                logging(LOG_VERBOSE, "[SKIPPED] LUN is not removable. "
                        "Skipping test.");
                return;
        }

        logging(LOG_VERBOSE, "Eject the medium.");
        STARTSTOPUNIT(sd, 1, 0, 0, 0, 1, 0,
                      EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Test TESTUNITREADY when medium is ejected.");
        TESTUNITREADY(sd,
                      EXPECT_NO_MEDIUM);

        test_synchronizecache10();
        test_synchronizecache16();
        test_read10();
        test_read12();
        test_read16();
        test_readcapacity10();
        test_readcapacity16();
        test_verify10();
        test_verify12();
        test_verify16();
        test_getlbastatus();
        test_prefetch10();
        test_prefetch16();

        if (!data_loss) {
                logging(LOG_VERBOSE, "[SKIPPING] Dataloss flag not set. Skipping test for WRITE commands");
                goto finished;
        }

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

finished:
        logging(LOG_VERBOSE, "Load the medium again.");
        STARTSTOPUNIT(sd, 1, 0, 0, 0, 1, 1,
                      EXPECT_STATUS_GOOD);
}
