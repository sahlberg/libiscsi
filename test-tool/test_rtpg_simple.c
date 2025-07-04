/* 
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

#include <stdio.h>

void
test_rtpg_simple(void)
{
        int ret, full_size, i, io_ready_groups;
        struct scsi_inquiry_standard *std_inq;
        struct scsi_report_target_port_groups *report;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of the RTPG command");

        logging(LOG_VERBOSE, "Checking if the target supports RTPG");
        ret = inquiry(sd, &task, 0, 0, 260, EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        std_inq = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(std_inq, NULL);
        if (std_inq->tpgs == 0) {
                logging(LOG_VERBOSE, "The target does not support RTPG. Skipping RTPG tests.");
                scsi_free_scsi_task(task);
                task = NULL;
                return;
        }
        scsi_free_scsi_task(task);
        task = NULL;

        logging(LOG_VERBOSE, "Retrieving 4 bytes of RTPG data");
        ret = rtpg(sd, &task, 4, EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        full_size = scsi_datain_getfullsize(task);
        scsi_free_scsi_task(task);
        task = NULL;
        logging(LOG_VERBOSE, "Retrieving all RTPG data (%d bytes)", full_size);
        ret = rtpg(sd, &task, full_size, EXPECT_STATUS_GOOD);
        CU_ASSERT_EQUAL(ret, 0);
        report = scsi_datain_unmarshall(task);
        CU_ASSERT_NOT_EQUAL(report, NULL);
        /* data size stays the same */
        CU_ASSERT_EQUAL(full_size, scsi_datain_getfullsize(task));

        logging(LOG_VERBOSE, "Validating %d target port groups", report->num_groups);
        io_ready_groups = 0;
        for (i = 0; i < report->num_groups; ++i) {
                /* Valid ALUA state */
                CU_ASSERT(report->groups[i].alua_state == SCSI_ALUA_ACTIVE_OPTIMIZED ||
                          report->groups[i].alua_state == SCSI_ALUA_ACTIVE_NONOPTIMIZED ||
                          report->groups[i].alua_state == SCSI_ALUA_STANDBY ||
                          report->groups[i].alua_state == SCSI_ALUA_UNAVAILABLE ||
                          report->groups[i].alua_state == SCSI_ALUA_LOGICAL_BLOCK_DEPENDENT ||
                          report->groups[i].alua_state == SCSI_ALUA_OFFLINE ||
                          report->groups[i].alua_state == SCSI_ALUA_TRANSITIONING);
                if (report->groups[i].alua_state == SCSI_ALUA_ACTIVE_OPTIMIZED ||
                    report->groups[i].alua_state == SCSI_ALUA_ACTIVE_NONOPTIMIZED) {
                        ++io_ready_groups;
                }
                /* Since we retrieved full size, we get all port ids */
                CU_ASSERT_EQUAL(report->groups[i].port_count, report->groups[i].retrieved_port_count);
        }
        CU_ASSERT(io_ready_groups > 0);

        scsi_free_scsi_task(task);
        task = NULL;
};
