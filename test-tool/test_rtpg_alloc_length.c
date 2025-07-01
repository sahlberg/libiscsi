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
test_rtpg_alloc_length(void)
{
        int ret, full_size, size, group;
        struct scsi_inquiry_standard *std_inq;
        struct scsi_report_target_port_groups *report;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of the RTPG command with insufficient buffers");

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

        /* The test assumes that groups are reported in the same order for any buffer size. */
        size = 4; /* offset of the 1st target port group descriptor */
        for (group = 0; group < report->num_groups; ++group) {
                const int group_descriptor_size = 8;
                const int port_descriptor_size = 4;
                struct scsi_report_target_port_groups *report_partial = NULL;
                struct scsi_task *task_partial = NULL;
                int i;

                logging(LOG_VERBOSE, "Buffer boundary cuts descriptor of group %d in half", group);
                size += group_descriptor_size / 2;
                ret = rtpg(sd, &task_partial, size, EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
                report_partial = scsi_datain_unmarshall(task_partial);
                CU_ASSERT_NOT_EQUAL(report_partial, NULL);
                /* cut group not unmarshalled */
                CU_ASSERT_EQUAL(group, report_partial->num_groups);
                /* previous groups unmarshalled along with their ports */
                for (i = 0; i < group; ++i) {
                        CU_ASSERT_EQUAL(report_partial->groups[i].retrieved_port_count,
                                        report_partial->groups[i].port_count);
                        CU_ASSERT_EQUAL(report_partial->groups[i].retrieved_port_count,
                                        report->groups[i].retrieved_port_count);
                }
                scsi_free_scsi_task(task_partial);
                task_partial = NULL;

                logging(LOG_VERBOSE, "Buffer boundary at the end of descriptor of group %d", group);
                size += group_descriptor_size / 2;
                ret = rtpg(sd, &task_partial, size, EXPECT_STATUS_GOOD);
                CU_ASSERT_EQUAL(ret, 0);
                report_partial = scsi_datain_unmarshall(task_partial);
                CU_ASSERT_NOT_EQUAL(report_partial, NULL);
                /* group unmarshalled */
                CU_ASSERT_EQUAL(group + 1, report_partial->num_groups);
                /* previous groups unmarshalled along with their ports */
                for (i = 0; i < group; ++i) {
                        CU_ASSERT_EQUAL(report_partial->groups[i].retrieved_port_count,
                                        report_partial->groups[i].port_count);
                        CU_ASSERT_EQUAL(report_partial->groups[i].retrieved_port_count,
                                        report->groups[i].retrieved_port_count);
                }
                /* no retrieved ports for the current group */
                CU_ASSERT_EQUAL(report_partial->groups[group].retrieved_port_count, 0);
                CU_ASSERT_EQUAL(report_partial->groups[group].port_count,
                                report->groups[group].port_count);
                scsi_free_scsi_task(task_partial);
                task_partial = NULL;

                if (report->groups[group].port_count == 0) {
                        continue;
                }
                size += port_descriptor_size;
                if (report->groups[group].port_count > 1) {
                        logging(LOG_VERBOSE, "Just one port of group %d fits the buffer", group);
                        ret = rtpg(sd, &task_partial, size, EXPECT_STATUS_GOOD);
                        CU_ASSERT_EQUAL(ret, 0);
                        report_partial = scsi_datain_unmarshall(task_partial);
                        CU_ASSERT_NOT_EQUAL(report_partial, NULL);
                        /* group unmarshalled */
                        CU_ASSERT_EQUAL(group + 1, report_partial->num_groups);
                        /* previous groups unmarshalled along with their ports */
                        for (i = 0; i < group; ++i) {
                                CU_ASSERT_EQUAL(report_partial->groups[i].retrieved_port_count,
                                                report_partial->groups[i].port_count);
                                CU_ASSERT_EQUAL(report_partial->groups[i].retrieved_port_count,
                                                report->groups[i].retrieved_port_count);
                        }
                        /* 1 retrieved port for the current group */
                        CU_ASSERT_EQUAL(report_partial->groups[group].retrieved_port_count, 1);
                        CU_ASSERT_EQUAL(report_partial->groups[group].port_count,
                                        report->groups[group].port_count);
                        scsi_free_scsi_task(task_partial);
                        task_partial = NULL;
                        size += port_descriptor_size * (report->groups[group].port_count - 1);
                }
        }

        scsi_free_scsi_task(task);
        task = NULL;
};
