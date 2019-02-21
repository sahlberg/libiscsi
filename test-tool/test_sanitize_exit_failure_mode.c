/* 
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
test_sanitize_exit_failure_mode(void)
{ 
        struct iscsi_data data;
        struct scsi_command_descriptor *cd;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE EXIT FAILURE MODE");

        CHECK_FOR_SANITIZE;
        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Check that SANITIZE EXIT FAILURE MODE is "
                "supported in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_EXIT_FAILURE_MODE);
        if (cd == NULL) {
                logging(LOG_NORMAL, "[SKIPPED] SANITIZE EXIT FAILURE MODE is "
                        "not implemented according to "
                        "REPORT_SUPPORTED_OPCODES.");
                CU_PASS("SANITIZE is not implemented.");
                return;
        }

        data.size = 8;
        data.data = alloca(data.size);
        memset(data.data, 0, data.size);

        logging(LOG_VERBOSE, "EXIT_FAILURE_MODE parameter list length must "
                "be 0");
        logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
                "EXIT_FAILURE_MODE");
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_EXIT_FAILURE_MODE, 8, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);
}
