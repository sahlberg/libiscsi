/* 
   Copyright (C) 2016 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"


static int new_tl;

static int my_iscsi_queue_pdu(struct iscsi_context *iscsi _U_, struct iscsi_pdu *pdu _U_)
{
        if (pdu->outdata.data[0] != ISCSI_PDU_SCSI_REQUEST) {
                return 0;
        }
        switch (new_tl) {
        case 1:
                /* change TL to 1 */
                pdu->outdata.data[32 + 13] = 1;
                break;
        case 2:
                /* change TL to 3 */
                pdu->outdata.data[32 + 13] = 3;
                break;
        }
        return 0;
}

void
test_compareandwrite_invalid_dataout_size(void)
{
        CHECK_FOR_DATALOSS;
        CHECK_FOR_THIN_PROVISIONING;
        CHECK_FOR_LBPPB_GT_1;
        CHECK_FOR_SBC;

        local_iscsi_queue_pdu = my_iscsi_queue_pdu;
        
        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test that COMPAREANDWRITE fails for invalid "
                "(too small/too large) DataOut sizes.");
        memset(scratch, 0xa6, 2 * block_size);


        logging(LOG_VERBOSE, "Check too small DataOut");
        logging(LOG_VERBOSE, "COMPAREANDWRITE with DataOut==%zd (4 blocks) "
                "and TL == 1 ", 4 * block_size);

        new_tl = 1;
        COMPAREANDWRITE(sd, 0,
                        scratch, 4 * block_size,
                        block_size, 0, 0, 0, 0,
                        EXPECT_STATUS_GENERIC_BAD);

        logging(LOG_VERBOSE, "Check too large DataOut");
        logging(LOG_VERBOSE, "COMPAREANDWRITE with DataOut==%zd (4 blocks) "
                "and TL == 3 ", 4 * block_size);

        new_tl = 2;
        COMPAREANDWRITE(sd, 0,
                        scratch, 4 * block_size,
                        block_size, 0, 0, 0, 0,
                        EXPECT_STATUS_GENERIC_BAD);

}
