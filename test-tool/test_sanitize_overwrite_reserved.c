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
#include <stdlib.h>
#include <arpa/inet.h>
#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

static int change_num;

static int my_iscsi_queue_pdu(struct iscsi_context *iscsi _U_, struct iscsi_pdu *pdu)
{
        switch (change_num) {
        case 1:
                /* Set reserved bit 0x40 in byte 1 of the CDB */
                pdu->outdata.data[33] |= 0x40;
                break;
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
                /* Set reserved byte in the CDB */
                pdu->outdata.data[32 + change_num] = change_num;
                break;
        }

        change_num = 0;        
        return 0;
}

void test_sanitize_overwrite_reserved(void)
{ 
        int i;
        struct iscsi_data data;

        data.size = block_size + 4;
        data.data = alloca(data.size);
        memset(&data.data[4], 0xaa, block_size);

        data.data[0] = 0x01;
        data.data[1] = 0x00;
        data.data[2] = block_size >> 8;
        data.data[3] = block_size & 0xff;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE OVERWRITE Reserved bits/bytes");

        CHECK_FOR_SANITIZE;
        CHECK_FOR_DATALOSS;

        local_iscsi_queue_pdu = my_iscsi_queue_pdu;

        logging(LOG_VERBOSE, "Send SANITIZE command with the reserved "
                "bit in byte 1 set to 1");
        change_num = 1;
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);

        for (i = 2; i < 7; i++) {
                logging(LOG_VERBOSE, "Send SANITIZE command with the reserved "
                        "byte %d set to non-zero", i);
                change_num = i;

                SANITIZE(sd, 0, 0, SCSI_SANITIZE_OVERWRITE, data.size, &data,
                         EXPECT_INVALID_FIELD_IN_CDB);
        }
}
