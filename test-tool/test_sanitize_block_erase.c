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
#include <inttypes.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

static void
check_wabereq(void)
{
        struct scsi_task *task_ret = NULL;

        logging(LOG_VERBOSE, "Read one block from LBA 0");
        READ10(sd, &task_ret, 0, block_size, block_size, 0, 0, 0, 0, 0, NULL,
               EXPECT_STATUS_GOOD);
        CU_ASSERT_PTR_NOT_NULL_FATAL(task_ret);
        if (task_ret == NULL) {
                return;
        }
        CU_ASSERT_NOT_EQUAL(task_ret->status, SCSI_STATUS_CANCELLED);

        switch (inq_bdc->wabereq) {
        case 0:
                logging(LOG_NORMAL, "[FAILED] SANITIZE BLOCK ERASE "
                        "opcode is supported but WABEREQ is 0");
                CU_FAIL("[FAILED] SANITIZE BLOCK ERASE "
                        "opcode is supported but WABEREQ is 0");
                break;
        case 1:
                logging(LOG_VERBOSE, "WABEREQ==1. Reads from the "
                        "device should be successful.");
                if (task_ret->status == SCSI_STATUS_GOOD) {
                        logging(LOG_VERBOSE, "[SUCCESS] Read was "
                                "successful after SANITIZE");
                        break;
                }
                logging(LOG_NORMAL, "[FAILED] Read after "
                        "SANITIZE failed but WABEREQ is 1");
                CU_FAIL("[FAILED] Read after SANITIZE failed "
                        "but WABEREQ is 1");
                break;
        case 2:
                logging(LOG_VERBOSE, "WABEREQ==2. Reads from the "
                        "device should fail.");
                if (task_ret->status        == SCSI_STATUS_CHECK_CONDITION
                    && task_ret->sense.key  == SCSI_SENSE_MEDIUM_ERROR
                    && task_ret->sense.ascq != SCSI_SENSE_ASCQ_WRITE_AFTER_SANITIZE_REQUIRED) {
                        logging(LOG_VERBOSE, "[SUCCESS] Read failed "
                                "with CHECK_CONDITION/MEDIUM_ERROR/"
                                "!WRITE_AFTER_SANITIZE_REQUIRED");
                        break;
                }
                logging(LOG_VERBOSE, "[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "!WRITE_AFTER_SANITIZE_REQUIRED");
                CU_FAIL("[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "!WRITE_AFTER_SANITIZE_REQUIRED");
                break;
        case 3:
                logging(LOG_VERBOSE, "WABEREQ==3. Reads from the "
                        "device should fail.");
                if (task_ret->status        == SCSI_STATUS_CHECK_CONDITION
                    && task_ret->sense.key  == SCSI_SENSE_MEDIUM_ERROR
                    && task_ret->sense.ascq == SCSI_SENSE_ASCQ_WRITE_AFTER_SANITIZE_REQUIRED) {
                        logging(LOG_VERBOSE, "[SUCCESS] Read failed "
                                "with CHECK_CONDITION/MEDIUM_ERROR/"
                                "WRITE_AFTER_SANITIZE_REQUIRED");
                        break;
                }
                logging(LOG_VERBOSE, "[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "WRITE_AFTER_SANITIZE_REQUIRED");
                CU_FAIL("[FAILED] Read should have failed "
                        "with CHECK_CONDITION/MEDIUM_ERROR/"
                        "WRITE_AFTER_SANITIZE_REQUIRED");
                break;
        }

        scsi_free_scsi_task(task_ret);
}

static void
check_unmap(void)
{
        int i;
        struct scsi_task *task_ret = NULL;
        struct scsi_get_lba_status *lbas;
        uint64_t lba;

        logging(LOG_VERBOSE, "Read LBA mapping from the target");
        GETLBASTATUS(sd, &task_ret, 0, 256,
                     EXPECT_STATUS_GOOD);
        if (task_ret == NULL) {
                logging(LOG_VERBOSE, "[FAILED] Failed to read LBA mapping "
                        "from the target.");
                CU_FAIL("[FAILED] Failed to read LBA mapping "
                        "from the target.");
                return;
        }
        if (task_ret->status != SCSI_STATUS_GOOD) {
                logging(LOG_VERBOSE, "[FAILED] Failed to read LBA mapping "
                        "from the target. Sense: %s",
                        sd->error_str);
                CU_FAIL("[FAILED] Failed to read LBA mapping "
                        "from the target.");
                scsi_free_scsi_task(task_ret);
                return;
        }

        logging(LOG_VERBOSE, "Unmarshall LBA mapping datain buffer");
        lbas = scsi_datain_unmarshall(task_ret);
        if (lbas == NULL) {
                logging(LOG_VERBOSE, "[FAILED] Failed to unmarshall LBA "
                        "mapping");
                CU_FAIL("[FAILED] Failed to read unmarshall LBA mapping");
                scsi_free_scsi_task(task_ret);
                return;
        }

        logging(LOG_VERBOSE, "Verify we got at least one status descriptor "
                "from the target");
        if (lbas->num_descriptors < 1) {
                logging(LOG_VERBOSE, "[FAILED] Wrong number of LBA status "
                        "descriptors. Expected >=1 but got %d descriptors",
                        lbas->num_descriptors);
                CU_FAIL("[FAILED] Wrong number of LBA status descriptors.");
                scsi_free_scsi_task(task_ret);
                return;
        }

        logging(LOG_VERBOSE, "Verify that all descriptors are either "
                "DEALLOCATED or ANCHORED.");
        for (i = 0; i < (int)lbas->num_descriptors; i++) {
                logging(LOG_VERBOSE, "Check descriptor %d LBA:%" PRIu64 "-%"
                        PRIu64 " that it is not MAPPED",
                        i,
                        lbas->descriptors[i].lba,
                        lbas->descriptors[i].lba + lbas->descriptors[i].num_blocks);
                if (lbas->descriptors[i].provisioning == SCSI_PROVISIONING_TYPE_MAPPED) {
                          logging(LOG_VERBOSE, "[FAILED] Descriptor %d is MAPPED."
                                "All descriptors shoudl be either DEALLOCATED "
                                "or ANCHORED after SANITIZE", i);
                          CU_FAIL("[FAILED] LBA status descriptor is MAPPED.");
                }
        }

        logging(LOG_VERBOSE, "Verify that the descriptors cover the whole LUN");
        lba = 0;
        for (i = 0; i < (int)lbas->num_descriptors; i++) {
                logging(LOG_VERBOSE, "Check descriptor %d LBA:%" PRIu64 "-%"
                        PRIu64 " that it is in order",
                        i,
                        lbas->descriptors[i].lba,
                        lbas->descriptors[i].lba + lbas->descriptors[i].num_blocks);
                if (lba != lbas->descriptors[i].lba) {
                          logging(LOG_VERBOSE, "[FAILED] LBA status descriptors "
                                "are not in order.");
                          CU_FAIL("[FAILED] LBA status descriptors not in order");
                }
                lba += lbas->descriptors[i].num_blocks;
        }
        if (lba != num_blocks) {
                  logging(LOG_VERBOSE, "[FAILED] The LUN is not fully"
                        "DEALLOCATED/ANCHORED");
                  CU_FAIL("[FAILED] The LUN is not fully"
                        "DEALLOCATED/ANCHORED");
        }

        scsi_free_scsi_task(task_ret);
}

static void
check_lun_is_wiped(unsigned char *buf, uint64_t lba)
{
        unsigned char *rbuf = alloca(256 * block_size);

        READ16(sd, NULL, lba, 256 * block_size, block_size, 0, 0, 0, 0, 0, rbuf,
               EXPECT_STATUS_GOOD);
        if (rc16 == NULL) {
                return;
        }

        if (rc16->lbprz) {
                logging(LOG_VERBOSE, "LBPRZ==1 All blocks "
                        "should read back as 0");
                if (all_zero(rbuf, 256 * block_size) == 0) {
                        logging(LOG_NORMAL, "[FAILED] Blocks did not "
                                "read back as zero");
                        CU_FAIL("[FAILED] Blocks did not read back "
                                "as zero");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] Blocks read "
                                "back as zero");
                }
        } else {
                logging(LOG_VERBOSE, "LBPRZ==0 Blocks should not read back as "
                        "all 'a' any more");
                if (!memcmp(buf, rbuf, 256 * block_size)) {
                        logging(LOG_NORMAL, "[FAILED] Blocks were not wiped");
                        CU_FAIL("[FAILED] Blocks were not wiped");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] Blocks were wiped");
                }
        }
}

void
test_sanitize_block_erase(void)
{ 
        struct iscsi_data data;
        struct scsi_command_descriptor *cd;
        unsigned char *buf = alloca(256 * block_size);

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test SANITIZE BLOCK ERASE");

        CHECK_FOR_SANITIZE;
        CHECK_FOR_DATALOSS;

        logging(LOG_VERBOSE, "Check that SANITIZE BLOCK_ERASE is supported "
                "in REPORT_SUPPORTED_OPCODES");
        cd = get_command_descriptor(SCSI_OPCODE_SANITIZE,
                                    SCSI_SANITIZE_BLOCK_ERASE);
        if (cd == NULL) {
                logging(LOG_VERBOSE, "Opcode is not supported. Verify that "
                        "WABEREQ is zero.");
                if (inq_bdc && inq_bdc->wabereq) {
                        logging(LOG_NORMAL, "[FAILED] WABEREQ is not 0 but "
                                "SANITIZE BLOCK ERASE opcode is not supported");
                        CU_FAIL("[FAILED] WABEREQ is not 0 but BLOCK ERASE "
                                "is not supported.");
                }

                logging(LOG_NORMAL, "[SKIPPED] SANITIZE BLOCK_ERASE is not "
                        "implemented according to REPORT_SUPPORTED_OPCODES.");
                CU_PASS("SANITIZE is not implemented.");
                return;
        }

        logging(LOG_VERBOSE, "Verify that we have BlockDeviceCharacteristics "
                "VPD page.");
        if (inq_bdc == NULL) {
                logging(LOG_NORMAL, "[FAILED] SANITIZE BLOCK ERASE opcode is "
                        "supported but BlockDeviceCharacteristics VPD page is "
                        "missing");
                CU_FAIL("[FAILED] BlockDeviceCharacteristics VPD "
                        "page is missing");
        }

        logging(LOG_VERBOSE, "Verify that we have READCAPACITY16");
        if (!rc16) {
                logging(LOG_NORMAL, "[FAILED] SANITIZE BLOCK ERASE opcode is "
                        "supported but READCAPACITY16 is missing.");
                CU_FAIL("[FAILED] READCAPACITY16 is missing");
        }

        logging(LOG_VERBOSE, "Verify that logical block provisioning (LBPME) "
                "is available.");
        if (!rc16 || !(rc16->lbpme)) {
                logging(LOG_NORMAL, "[FAILED] SANITIZE BLOCK ERASE opcode is "
                        "supported but LBPME==0.");
                CU_FAIL("[FAILED] SANITIZE BLOCK ERASE opcode is "
                        "supported but LBPME==0.");
        }

        logging(LOG_VERBOSE, "Check MediumRotationRate whether this is a HDD "
                "or a SSD device.");
        if (inq_bdc && inq_bdc->medium_rotation_rate != 0) {
                logging(LOG_NORMAL, "This is a HDD device");
                logging(LOG_NORMAL, "[WARNING] SANITIZE BLOCK ERASE opcode is "
                        "supported but MediumRotationRate is not 0 "
                        "indicating that this is a HDD. Only SSDs should "
                        "implement BLOCK ERASE");
        } else {
                logging(LOG_NORMAL, "This is a HDD device");
        }


        logging(LOG_VERBOSE, "Write 'a' to the first 256 LBAs");
        memset(scratch, 'a', 256 * block_size);
        WRITE16(sd, 0, 256 * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);
        logging(LOG_VERBOSE, "Write 'a' to the last 256 LBAs");
        WRITE16(sd, num_blocks - 256, 256 * block_size,
                block_size, 0, 0, 0, 0, 0, scratch,
                EXPECT_STATUS_GOOD);


        logging(LOG_VERBOSE, "Test we can perform basic BLOCK ERASE SANITIZE");
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_BLOCK_ERASE, 0, NULL,
                 EXPECT_STATUS_GOOD);

        logging(LOG_VERBOSE, "Check that the first 256 LBAs are wiped.");
        check_lun_is_wiped(buf, 0);
        logging(LOG_VERBOSE, "Check that the last 256 LBAs are wiped.");
        check_lun_is_wiped(buf, num_blocks - 256);

        data.size = 8;
        data.data = alloca(data.size);
        memset(data.data, 0, data.size);

        logging(LOG_VERBOSE, "BLOCK_ERASE parameter list length must be 0");
        logging(LOG_VERBOSE, "Test that non-zero param length is an error for "
                "BLOCK ERASE");
        SANITIZE(sd, 0, 0, SCSI_SANITIZE_BLOCK_ERASE, 8, &data,
                 EXPECT_INVALID_FIELD_IN_CDB);

        if (inq_bdc) {
                logging(LOG_VERBOSE, "Check WABEREQ setting and that READ "
                        "after SANITIZE works correctly.");
                check_wabereq();
        }

        logging(LOG_VERBOSE, "Verify that all blocks are unmapped after "
                "SANITIZE BLOCK_ERASE");
        check_unmap();
}
