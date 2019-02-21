/*
   iscsi test-tool multipath support

   Copyright (C) 2015 David Disseldorp

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

#include "config.h"

#define _GNU_SOURCE
#include <assert.h>
#include <sys/syscall.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <fnmatch.h>
#include <errno.h>

#ifdef HAVE_SG_IO
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#endif

#include "slist.h"
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"
#include "iscsi-support.h"
#include "iscsi-multipath.h"

int mp_num_sds = 0;
struct scsi_device *mp_sds[MPATH_MAX_DEVS];

static void
mpath_des_free(struct scsi_inquiry_device_designator *des)
{
        if (!des) {
                return;
        }

        free(des->designator);
        free(des);
}

static int
mpath_des_copy(struct scsi_inquiry_device_designator *des,
               struct scsi_inquiry_device_designator **_des_cp)
{
        struct scsi_inquiry_device_designator *des_cp;

        if (!_des_cp) {
                return -1;
        }

        des_cp = malloc(sizeof(*des_cp));
        if (des_cp == NULL) {
                return -1;
        }

        des_cp->protocol_identifier = des->protocol_identifier;
        des_cp->code_set = des->code_set;
        des_cp->piv = des->piv;
        des_cp->association = des->association;
        des_cp->designator_type = des->designator_type;
        des_cp->designator_length = des->designator_length;
        des_cp->designator = malloc(des->designator_length);
        if (des_cp->designator == NULL) {
                free(des_cp);
                return -1;
        }
        memcpy(des_cp->designator, des->designator, des->designator_length);
        *_des_cp = des_cp;

        return 0;
}

static int
mpath_des_cmp(struct scsi_inquiry_device_designator *des1,
              struct scsi_inquiry_device_designator *des2)
{
        if (des1->protocol_identifier != des2->protocol_identifier) {
                return -1;
        }

        if (des1->code_set != des2->code_set) {
                return -1;
        }

        if (des1->piv != des2->piv) {
                return -1;
        }

        if (des1->association != des2->association) {
                return -1;
        }

        if (des1->designator_type != des2->designator_type) {
                return -1;
        }

        if (des1->designator_length != des2->designator_length) {
                return -1;
        }

        return memcmp(des1->designator, des2->designator,
                      des1->designator_length);
}

static int
mpath_check_matching_ids_devid_vpd(int num_sds,
                                   struct scsi_device **sds)
{
        int i;
        int num_sds_with_valid_id = 0;
        struct scsi_task *inq_task = NULL;
        struct scsi_inquiry_device_designator *des_saved = NULL;

        for (i = 0; i < num_sds; i++) {
                int ret;
                int full_size;
                struct scsi_inquiry_device_identification *inq_id_data;
                struct scsi_inquiry_device_designator *des;

                /*
                 * dev ID inquiry to confirm that all multipath devices carry
                 * an identical logical unit identifier.
                 */
                inquiry(sds[i], &inq_task, 1,
                        SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION,
                        64,
                        EXPECT_STATUS_GOOD);
                if (inq_task == NULL || inq_task->status != SCSI_STATUS_GOOD) {
                        printf("Inquiry command failed : %s\n",
                               sds[i]->error_str);
                        goto err_cleanup;
                }
                full_size = scsi_datain_getfullsize(inq_task);
                if (full_size > inq_task->datain.size) {
                        /* we need more data */
                        scsi_free_scsi_task(inq_task);
                        inq_task = NULL;
                        inquiry(sds[i], &inq_task, 1,
                                SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION,
                                full_size,
                                EXPECT_STATUS_GOOD);
                        if (inq_task == NULL) {
                                printf("Inquiry command failed : %s\n",
                                       sds[i]->error_str);
                                goto err_cleanup;
                        }
                }

                inq_id_data = scsi_datain_unmarshall(inq_task);
                if (inq_id_data == NULL) {
                        printf("failed to unmarshall inquiry ID datain blob\n");
                        goto err_cleanup;
                }

                if (inq_id_data->qualifier
                               != SCSI_INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED) {
                        printf("error: multipath device not connected\n");
                        goto err_cleanup;
                }

                if (inq_id_data->device_type
                         != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
                        printf("error: multipath devices must be SBC\n");
                        goto err_cleanup;
                }

                /* walk the list of IDs, and find a suitable LU candidate */
                for (des = inq_id_data->designators;
                     des != NULL;
                     des = des->next) {
                        if (des->association != SCSI_ASSOCIATION_LOGICAL_UNIT) {
                                printf("skipping non-LU designator: %d\n",
                                       des->association);
                                continue;
                        }

                        if ((des->designator_type != SCSI_DESIGNATOR_TYPE_EUI_64)
                         && (des->designator_type != SCSI_DESIGNATOR_TYPE_NAA)
                         && (des->designator_type != SCSI_DESIGNATOR_TYPE_MD5_LOGICAL_UNIT_IDENTIFIER)
                         && (des->designator_type != SCSI_DESIGNATOR_TYPE_SCSI_NAME_STRING)) {
                                printf("skipping unsupported des type: %d\n",
                                       des->designator_type);
                                continue;
                        }

                        if (des->designator_length <= 0) {
                                printf("skipping designator with bad len: %d\n",
                                       des->designator_length);
                                continue;
                        }

                        if (des_saved == NULL) {
                                ret = mpath_des_copy(des, &des_saved);
                                if (ret < 0) {
                                        goto err_cleanup;
                                }
                                /*
                                 * we now have a reference to look for in all
                                 * subsequent paths.
                                 */
                                num_sds_with_valid_id++;
                                break;
                        } else if (mpath_des_cmp(des, des_saved) == 0) {
                                /* found match for previous path designator */
                                num_sds_with_valid_id++;
                                break;
                        }
                        /* no match yet, keep checking other designators */
                }

                scsi_free_scsi_task(inq_task);
                inq_task = NULL;
        }
        mpath_des_free(des_saved);

        if (num_sds_with_valid_id != num_sds) {
                printf("failed to find matching LU device ID for all paths\n");
                return -1;
        }

        printf("found matching LU device identifier for all (%d) paths\n",
               num_sds);
        return 0;

err_cleanup:
        mpath_des_free(des_saved);
        scsi_free_scsi_task(inq_task);
        return -1;
}

static int
mpath_check_matching_ids_serial_vpd(int num_sds,
                                    struct scsi_device **sds)
{
        int i;
        int num_sds_with_valid_id = 0;
        struct scsi_task *inq_task = NULL;
        char *usn_saved = NULL;

        for (i = 0; i < num_sds; i++) {
                int full_size;
                struct scsi_inquiry_unit_serial_number *inq_serial;

                /*
                 * inquiry to confirm that all multipath devices carry an
                 * identical unit serial number.
                 */
                inq_task = NULL;
                inquiry(sds[i], &inq_task, 1,
                        SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER, 64,
                        EXPECT_STATUS_GOOD);
                if (inq_task == NULL || inq_task->status != SCSI_STATUS_GOOD) {
                        printf("Inquiry command failed : %s\n",
                               sds[i]->error_str);
                        goto err_cleanup;
                }
                full_size = scsi_datain_getfullsize(inq_task);
                if (full_size > inq_task->datain.size) {
                        scsi_free_scsi_task(inq_task);

                        /* we need more data */
                        inq_task = NULL;
                        inquiry(sds[i], &inq_task, 1,
                                SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER,
                                full_size,
                                EXPECT_STATUS_GOOD);
                        if (inq_task == NULL) {
                                printf("Inquiry command failed : %s\n",
                                       sds[i]->error_str);
                                goto err_cleanup;
                        }
                }

                inq_serial = scsi_datain_unmarshall(inq_task);
                if (inq_serial == NULL) {
                        printf("failed to unmarshall inquiry datain blob\n");
                        goto err_cleanup;
                }

                if (inq_serial->qualifier
                               != SCSI_INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED) {
                        printf("error: multipath device not connected\n");
                        goto err_cleanup;
                }

                if (inq_serial->device_type
                         != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
                        printf("error: multipath devices must be SBC\n");
                        goto err_cleanup;
                }

                if (inq_serial->usn == NULL) {
                        printf("error: empty usn for multipath device\n");
                        goto err_cleanup;
                }

                if (usn_saved == NULL) {
                        usn_saved = strdup(inq_serial->usn);
                        if (usn_saved == NULL) {
                                goto err_cleanup;
                        }
                        num_sds_with_valid_id++;
                } else if (strcmp(usn_saved, inq_serial->usn) == 0) {
                        num_sds_with_valid_id++;
                } else {
                        printf("multipath unit serial mismatch: %s != %s\n",
                               usn_saved, inq_serial->usn);
                }

                scsi_free_scsi_task(inq_task);
                inq_task = NULL;
        }

        if (num_sds_with_valid_id != num_sds) {
                printf("failed to find matching serial number for all paths\n");
                goto err_cleanup;
        }

        printf("found matching serial number for all (%d) paths: %s\n",
               num_sds, usn_saved);
        free(usn_saved);

        return 0;

err_cleanup:
        free(usn_saved);
        scsi_free_scsi_task(inq_task);
        return -1;
}

int
mpath_check_matching_ids(int num_sds,
                         struct scsi_device **sds)
{
        int ret;

        /*
         * first check all devices for a matching LU identifier in the device
         * identification INQUIRY VPD page.
         */
        ret = mpath_check_matching_ids_devid_vpd(num_sds, sds);
        if (ret == 0) {
                return 0;        /* found matching */
        }

        /* fall back to a unit serial number check */
        ret = mpath_check_matching_ids_serial_vpd(num_sds, sds);
        return ret;
}

int
mpath_count_iscsi(int num_sds,
                  struct scsi_device **sds)
{
        int i;
        int found = 0;

        for (i = 0; i < num_sds; i++) {
                if (sds[i]->iscsi_ctx != NULL) {
                        found++;
                }
        }

        return found;
}

/*
 * use an existing multi-path connection, or clone iscsi sd1.
 */
int
mpath_sd2_get_or_clone(struct scsi_device *sd1, struct scsi_device **_sd2)
{
        struct scsi_device *sd2;

        if (mp_num_sds > 1) {
                logging(LOG_VERBOSE, "using multipath dev for second session");
                *_sd2 = mp_sds[1];
                return 0;
        }

        if (sd1->iscsi_ctx == NULL) {
                logging(LOG_NORMAL, "can't clone non-iscsi device");
                return -EINVAL;
        }

        logging(LOG_VERBOSE, "cloning sd1 for second session");
        sd2 = malloc(sizeof(*sd2));
        if (sd2 == NULL) {
                return -ENOMEM;
        }

        memset(sd2, 0, sizeof(*sd2));
        sd2->iscsi_url = sd1->iscsi_url;
        sd2->iscsi_lun = sd1->iscsi_lun;
        sd2->iscsi_ctx = iscsi_context_login(initiatorname2, sd2->iscsi_url,
                                             &sd2->iscsi_lun);
        if (sd2->iscsi_ctx == NULL) {
                logging(LOG_VERBOSE, "Failed to login to target");
                free(sd2);
                return -ENOMEM;
        }
        *_sd2 = sd2;

        return 0;
}

void
mpath_sd2_put(struct scsi_device *sd2)
{
        if (mp_num_sds > 1) {
                if (sd2 != mp_sds[1]) {
                        logging(LOG_NORMAL, "Invalid sd2!");
                }
                return;
        }

        /* sd2 was allocated by mp_get - cleanup */
        iscsi_logout_sync(sd2->iscsi_ctx);
        iscsi_destroy_context(sd2->iscsi_ctx);
        free(sd2);
}
