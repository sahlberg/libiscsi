/*
   Copyright (C) 2013 Ronnie Sahlberg <ronniesahlberg@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-support.h"
#include "iscsi-test-cu.h"
#include "iscsi-multipath.h"

void
test_multipathio_reset(void)
{
        int reset_path;

        CHECK_FOR_DATALOSS;
        CHECK_FOR_SBC;
        MPATH_SKIP_IF_UNAVAILABLE(mp_sds, mp_num_sds);
        MPATH_SKIP_UNLESS_ISCSI(mp_sds, mp_num_sds);

        logging(LOG_VERBOSE, LOG_BLANK_LINE);

        for (reset_path = 0; reset_path < mp_num_sds; reset_path++) {
                int num_uas;
                int ret;
                int tur_path;
                struct scsi_device *reset_sd = mp_sds[reset_path];

                logging(LOG_VERBOSE, "Awaiting good TUR");
                ret = test_iscsi_tur_until_good(reset_sd, &num_uas);
                CU_ASSERT_EQUAL(ret, 0);

                logging(LOG_VERBOSE,
                        "Test multipath LUN Reset using path %d", reset_path);

                ret = iscsi_task_mgmt_lun_reset_sync(reset_sd->iscsi_ctx,
                                                     reset_sd->iscsi_lun);
                if (ret != 0) {
                        logging(LOG_NORMAL, "LUN reset failed. %s",
                                iscsi_get_error(reset_sd->iscsi_ctx));
                }
                CU_ASSERT_EQUAL(ret, 0);

                /* check for and clear LU reset UA on all paths */
                for (tur_path = 0; tur_path < mp_num_sds; tur_path++) {
                        logging(LOG_VERBOSE, "check for LU reset unit "
                                "attention via TUR on path %d", tur_path);
                        ret = test_iscsi_tur_until_good(mp_sds[tur_path], &num_uas);
                        CU_ASSERT_EQUAL(ret, 0);
                        CU_ASSERT_NOT_EQUAL(num_uas, 0);
                }
        }
}
