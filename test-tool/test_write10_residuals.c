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
#include <string.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"
#include "test_write_residuals.h"

void
test_write10_residuals(void)
{
        /* testing scenarios */
        const struct residuals_test_data write10_residuals[] = {
        /* cdb_size, xfer_len,        buf_len,          residuals_kind,   residuals_amount, check_overwrite */
                {10,        1,              0,  SCSI_RESIDUAL_OVERFLOW,         block_size, false,
                 "Try writing one block but with iSCSI expected transfer length==0"},

                {10,        1,          10000, SCSI_RESIDUAL_UNDERFLOW, 10000 - block_size, false,
                 "Try writing one block but with iSCSI expected transfer length==10000"},

                {10,        1,            200,  SCSI_RESIDUAL_OVERFLOW,   block_size - 200, false,
                 "Try writing one block but with iSCSI expected transfer length==200"},

                {10,        2,     block_size,  SCSI_RESIDUAL_OVERFLOW,         block_size, false,
                 "Try writing two blocks but iSCSI expected one block transfer length"},

                {10,        1, 2 * block_size, SCSI_RESIDUAL_UNDERFLOW,         block_size,  true,
                 "Verify that if iSCSI EDTL > SCSI TL then we only write SCSI TL amount of data"},

                {10,        2,     block_size,  SCSI_RESIDUAL_OVERFLOW,         block_size,  true,
                 "Verify that if iSCSI EDTL < SCSI TL then we only write iSCSI EDTL amount of data"},
        };

        unsigned int i = 0;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITE10 commands with residuals");
        logging(LOG_VERBOSE, "Block size is %zu", block_size);

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;
        CHECK_FOR_ISCSI(sd);

        /*
         * we don't want autoreconnect since some targets will drop the session
         * on this condition.
         */
        iscsi_set_noautoreconnect(sd->iscsi_ctx, 1);

        for (i = 0; i < ARRAY_SIZE(write10_residuals); i++) {
                logging(LOG_VERBOSE, "\n%s", write10_residuals[i].log_messages);
                write_residuals_test(&write10_residuals[i]);

                if (!command_is_implemented) {
                        CU_PASS("WRITE10 is not implemented.");
                        return;
                }
                /* in case the previous test failed the session */
                iscsi_set_noautoreconnect(sd->iscsi_ctx, 0);
        }
}
