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
test_write12_residuals(void)
{
        /* testing scenarios */
        const struct residuals_test_data write12_residuals[] = {
                {.cdb_size	= 12,
                 .xfer_len	= 1,
                 .buf_len	= 0,
                 .residual_type	= SCSI_RESIDUAL_OVERFLOW,
                 .residual	= block_size,
                 .description	=
                 "Try writing one block but with iSCSI EDTL==0"},

                {.cdb_size	= 12,
                 .xfer_len	= 1,
                 .buf_len	= 2 * block_size,
                 .residual_type	= SCSI_RESIDUAL_UNDERFLOW,
                 .residual	= block_size,
                 .description	=
                 "Try writing one block but set iSCSI EDTL to 2 blocks"},

                {.cdb_size	= 12,
                 .xfer_len	= 2,
                 .buf_len	= block_size,
                 .residual_type	= SCSI_RESIDUAL_OVERFLOW,
                 .residual	= block_size,
                 .description	=
                 "Try writing two blocks but set iSCSI EDTL to 1 block"},

                {.cdb_size	= 12,
                 .xfer_len	= 1,
                 .buf_len	= 10000,
                 .residual_type	= SCSI_RESIDUAL_UNDERFLOW,
                 .residual	= 10000 - block_size,
                 .description	=
                 "Try writing one block but with iSCSI EDTL==10000"},

                {.cdb_size	= 12,
                 .xfer_len	= 1,
                 .buf_len	= 200,
                 .residual_type	= SCSI_RESIDUAL_OVERFLOW,
                 .residual	= block_size - 200,
                 .description	=
                 "Try writing one block but with iSCSI EDTL==200"},
        };

        unsigned int i = 0;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test WRITE12 commands with residuals");
        logging(LOG_VERBOSE, "Block size is %zu", block_size);

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;
        CHECK_FOR_ISCSI(sd);

        /*
         * we don't want autoreconnect since some targets will drop the session
         * on this condition.
         */
        iscsi_set_noautoreconnect(sd->iscsi_ctx, 1);

        for (i = 0; i < ARRAY_SIZE(write12_residuals); i++) {
                logging(LOG_VERBOSE, "\n%s", write12_residuals[i].description);
                write_residuals_test(&write12_residuals[i]);

                if (!command_is_implemented) {
                        CU_PASS("WRITE12 is not implemented.");
                        return;
                }
                /* in case the previous test failed the session */
                iscsi_set_noautoreconnect(sd->iscsi_ctx, 0);
        }
}
