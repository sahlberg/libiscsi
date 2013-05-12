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
test_nomedia_sbc(void)
{
	int ret;
	unsigned char buf[4096];
	struct unmap_list list[1];

	CHECK_FOR_SBC;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test that Medium commands fail when medium is ejected on SBC devices");

	if (!inq->rmb) {
		logging(LOG_VERBOSE, "[SKIPPED] LUN is not removable. "
			"Skipping test.");
		return;
	}

	logging(LOG_VERBOSE, "Eject the medium.");
	ret = startstopunit(iscsic, tgt_lun, 1, 0, 0, 0, 1, 0);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test TESTUNITREADY when medium is ejected.");
	ret = testunitready_nomedium(iscsic, tgt_lun);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test SYNCHRONIZECACHE10 when medium is ejected.");
	ret = synchronizecache10_nomedium(iscsic, tgt_lun, 0, 1, 1, 1);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"SYNCHRONIZECACHE10");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test SYNCHRONIZECACHE16 when medium is ejected.");
	ret = synchronizecache16_nomedium(iscsic, tgt_lun, 0, 1, 1, 1);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"SYNCHRONIZECACHE16");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test READ10 when medium is ejected.");
	ret = read10_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			      0, 0, 0, 0, 0, NULL);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test READ12 when medium is ejected.");
	ret = read12_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			      0, 0, 0, 0, 0, NULL);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test READ16 when medium is ejected.");
	ret = read16_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			      0, 0, 0, 0, 0, NULL);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test READCAPACITY10 when medium is ejected.");
	ret = readcapacity10_nomedium(iscsic, tgt_lun, 0, 0);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test READCAPACITY16 when medium is ejected.");
	ret = readcapacity16_nomedium(iscsic, tgt_lun, 15);
	if (ret == -2) {
		if (sbc3_support) {
			logging(LOG_NORMAL, "[FAILED] READCAPACITY16 is not available but the device claims SBC-3 support.");
			CU_FAIL("READCAPACITY16 failed but the device claims SBC-3 support.");
		} else {
			logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 is not implemented on this target and it does not claim SBC-3 support.");
		}
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test GET_LBA_STATUS when medium is ejected.");
	ret = get_lba_status_nomedium(iscsic, tgt_lun, 0, 24);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"GET_LBA_STATUS");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test PREFETCH10 when medium is ejected.");
	ret = prefetch10_nomedium(iscsic, tgt_lun, 0, 1, 1, 0);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"PREFETCH10");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test PREFETCH16 when medium is ejected.");
	ret = prefetch16_nomedium(iscsic, tgt_lun, 0, 1, 1, 0);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"PREFETCH16");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test VERIFY10 when medium is ejected.");
	ret = verify10_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
				0, 0, 1, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"VERIFY10");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test VERIFY12 when medium is ejected.");
	ret = verify12_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
				0, 0, 1, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"VERIFY102");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test VERIFY16 when medium is ejected.");
	ret = verify16_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
				0, 0, 1, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"VERIFY16");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	if (!data_loss) {
		logging(LOG_VERBOSE, "[SKIPPING] Dataloss flag not set. Skipping test for WRITE commands");
		goto finished;
	}

	logging(LOG_VERBOSE, "Test WRITE10 when medium is ejected.");
	ret = write10_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test WRITE12 when medium is ejected.");
	ret = write12_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test WRITE16 when medium is ejected.");
	ret = write16_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, 0, buf);
	CU_ASSERT_EQUAL(ret, 0);

	logging(LOG_VERBOSE, "Test WRITEVERIFY10 when medium is ejected.");
	ret = writeverify10_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"WRITEVERIFY10");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test WRITEVERIFY12 when medium is ejected.");
	ret = writeverify12_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"WRITEVERIFY12");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test WRITEVERIFY16 when medium is ejected.");
	ret = writeverify16_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"WRITEVERIFY16");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test ORWRITE when medium is ejected.");
	ret = orwrite_nomedium(iscsic, tgt_lun, 0, block_size, block_size,
			       0, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"ORWRITE");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test COMPAREWRITE when medium is ejected.");
	logging(LOG_VERBOSE, "[SKIPPED] Test not implemented yet");

	logging(LOG_VERBOSE, "Test WRITESAME10 when medium is ejected.");
	ret = writesame10_nomedium(iscsic, tgt_lun, 0, block_size,
				   1, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"WRITESAME10");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test WRITESAME16 when medium is ejected.");
	ret = writesame16_nomedium(iscsic, tgt_lun, 0, block_size,
				   1, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"WRITESAME16");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}

	logging(LOG_VERBOSE, "Test UNMAP when medium is ejected.");
	list[0].lba = 0;
	list[0].num = lbppb;
	ret = unmap_nomedium(iscsic, tgt_lun, 0, list, 1);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] target does not support "
			"UNMAP");
	} else {
		CU_ASSERT_EQUAL(ret, 0);
	}


finished:
	logging(LOG_VERBOSE, "Load the medium again.");
	ret = startstopunit(iscsic, tgt_lun, 1, 0, 0, 0, 1, 1);
	CU_ASSERT_EQUAL(ret, 0);
}
