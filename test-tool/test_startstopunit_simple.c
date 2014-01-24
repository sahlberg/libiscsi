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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_startstopunit_simple(void)
{ 
	int ret;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test basic STARTSTOPUNIT");


	logging(LOG_VERBOSE, "Test we can eject removable the media with IMMED==1");
	if (inq->rmb) {
		logging(LOG_VERBOSE, "Media is removable. STARTSTOPUNIT should work");
	} else {
		logging(LOG_VERBOSE, "Media is not removable. STARTSTOPUNIT should fail");
	}

	ret = startstopunit(iscsic, tgt_lun,
			    1, 0, 0, 0, 1, 0);
	if (!inq->rmb) {
		CU_ASSERT_NOT_EQUAL(ret, 0);
		return;
	}
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test TESTUNITREADY that medium is ejected.");
	ret = testunitready_nomedium(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test we can load the removable the media with IMMED==1");
	ret = startstopunit(iscsic, tgt_lun,
			    1, 0, 0, 0, 1, 1);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify we can read from the media.");
	ret = testunitready(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);



	logging(LOG_VERBOSE, "Test we can eject removable the media with IMMED==1");
	ret = startstopunit(iscsic, tgt_lun,
			    0, 0, 0, 0, 1, 0);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test TESTUNITREADY that medium is ejected.");
	ret = testunitready_nomedium(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);


	logging(LOG_VERBOSE, "Test we can load the removable the media with IMMED==1");
	ret = startstopunit(iscsic, tgt_lun,
			    0, 0, 0, 0, 1, 1);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Verify we can access the media again.");
	ret = testunitready(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);
}
