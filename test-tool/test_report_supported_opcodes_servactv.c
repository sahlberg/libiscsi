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


void
test_report_supported_opcodes_servactv(void)
{
        int i;
        struct scsi_task *rso_task;
        struct scsi_report_supported_op_codes *rsoc;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test READ_SUPPORTED_OPCODES SERVACTV flag");

        REPORT_SUPPORTED_OPCODES(sd, &rso_task,
                                 0, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
                                 65535,
                                 EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Unmarshall the DATA-IN buffer");
        rsoc = scsi_datain_unmarshall(rso_task);
        CU_ASSERT_PTR_NOT_NULL_FATAL(rsoc);

        logging(LOG_VERBOSE, "Verify that when SERVACTV is clear then "
                "ServiceAction must be zero.");
        for (i = 0; i < rsoc->num_descriptors; i++) {
                if (!rsoc->descriptors[i].servactv && rsoc->descriptors[i].sa) {
                        logging(LOG_NORMAL, "[FAILED] ServiceAction is "
                                "non-zero but SERVACTV is clear");
                        CU_FAIL("[FAILED] ServiceAction is "
                                "non-zero but SERVACTV is clear");
                }
        }

        scsi_free_scsi_task(rso_task);
}
