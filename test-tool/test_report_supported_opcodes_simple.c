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
test_report_supported_opcodes_simple(void)
{
	int ret;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test basic READ_SUPPORTED_OPCODES");

	ret = report_supported_opcodes(iscsic, tgt_lun,
		0, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0,
		1024, NULL);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] READ_SUPPORTED_OPCODES is not "
			"implemented.");
		CU_PASS("READ_SUPPORTED_OPCODES is not implemented.");
		return;
	}
	CU_ASSERT_EQUAL(ret, 0);
}
