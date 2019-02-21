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
test_inquiry_version_descriptors(void)
{
        int i, claimed_ok;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test of the INQUIRY version descriptors");

        switch (inq->device_type) {
        case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS:
                logging(LOG_VERBOSE, "Device is a block device");

                logging(LOG_VERBOSE, "Verify it claim some version of SPC");
                claimed_ok = 0;
                for (i = 0; i < 8; i++) {
                        switch(inq->version_descriptor[i]) {
                        case SCSI_VERSION_DESCRIPTOR_SPC:
                        case SCSI_VERSION_DESCRIPTOR_SPC_ANSI_INCITS_301_1997:
                        case SCSI_VERSION_DESCRIPTOR_SPC_T10_0995_D_R11A:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2_ISO_IEC_14776_452:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2_ANSI_INCITS_351_2001:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R20:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R12:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R18:
                        case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R19:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3_ISO_IEC_14776_453:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3_ANSI_INCITS_408_2005:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R7:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R21:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R22:
                        case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R23:
                        case SCSI_VERSION_DESCRIPTOR_SPC_4:
                        case SCSI_VERSION_DESCRIPTOR_SPC_4_T10_1731_D_R16:
                        case SCSI_VERSION_DESCRIPTOR_SPC_4_T10_1731_D_R18:
                        case SCSI_VERSION_DESCRIPTOR_SPC_4_T10_1731_D_R23:
                                claimed_ok = 1;
                                break;
                        }
                }
                if (claimed_ok == 0) {
                        logging(LOG_NORMAL, "[WARNING] Block device "
                                "did not claim any version of SPC");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] Block device "
                                "claimed a version of SPC");
                }

                logging(LOG_VERBOSE, "Verify it claim some version of SBC");
                claimed_ok = 0;
                for (i = 0; i < 8; i++) {
                        switch(inq->version_descriptor[i]) {
                        case SCSI_VERSION_DESCRIPTOR_SBC:
                        case SCSI_VERSION_DESCRIPTOR_SBC_ANSI_INCITS_306_1998:
                        case SCSI_VERSION_DESCRIPTOR_SBC_T10_0996_D_R08C:
                        case SCSI_VERSION_DESCRIPTOR_SBC_2:
                        case SCSI_VERSION_DESCRIPTOR_SBC_2_ISO_IEC_14776_322:
                        case SCSI_VERSION_DESCRIPTOR_SBC_2_ANSI_INCITS_405_2005:
                        case SCSI_VERSION_DESCRIPTOR_SBC_2_T10_1417_D_R16:
                        case SCSI_VERSION_DESCRIPTOR_SBC_2_T10_1417_D_R5A:
                        case SCSI_VERSION_DESCRIPTOR_SBC_2_T10_1417_D_R15:
                        case SCSI_VERSION_DESCRIPTOR_SBC_3:
                                claimed_ok = 1;
                                break;
                        }
                }
                if (claimed_ok == 0) {
                        logging(LOG_NORMAL, "[WARNING] Block device "
                                "did not claim any version of SBC");
                } else {
                        logging(LOG_VERBOSE, "[SUCCESS] Block device "
                                "claimed a version of SBC");
                }
                break;
        default:
                logging(LOG_VERBOSE, "No version descriptor tests for device"
                                     " type %d yet.", inq->device_type);
        }
}
