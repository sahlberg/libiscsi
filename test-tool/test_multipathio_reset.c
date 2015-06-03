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
	/* SAM3 6.2 */
	int lu_reset_acsq[2]
		= {SCSI_SENSE_ASCQ_BUS_DEVICE_RESET_FUNCTION_OCCURED, /* SAM3 */
		   SCSI_SENSE_ASCQ_BUS_RESET}; /* TGT */

	CHECK_FOR_DATALOSS;
	CHECK_FOR_SBC;
	MPATH_SKIP_IF_UNAVAILABLE(mp_sds, mp_num_sds);

	logging(LOG_VERBOSE, LOG_BLANK_LINE);

	for (reset_path = 0; reset_path < mp_num_sds; reset_path++) {
		int ret;
		int tur_path;
		struct scsi_device *reset_sd = mp_sds[reset_path];

		ret = testunitready(reset_sd, EXPECT_STATUS_GOOD);
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
				"attention sense via TUR on path %d", tur_path);
			ret = testunitready(mp_sds[tur_path],
					    SCSI_STATUS_CHECK_CONDITION,
					    SCSI_SENSE_UNIT_ATTENTION,
					    lu_reset_acsq, sizeof(lu_reset_acsq));
			CU_ASSERT_EQUAL(ret, 0);
		}
	}
}
