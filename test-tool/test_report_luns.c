/*
   Copyright (C) SUSE LLC 2020

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

#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

void
test_report_luns_simple(void)
{
	struct scsi_task *rl_task;
	struct scsi_reportluns_list *rl_list;
	int full_report_size;
	uint32_t i;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test REPORT LUNS");

	rl_task = iscsi_reportluns_sync(sd->iscsi_ctx, 0, 512);
	CU_ASSERT_PTR_NOT_NULL_FATAL(rl_task);

	full_report_size = scsi_datain_getfullsize(rl_task);
	if (full_report_size > rl_task->datain.size) {
		logging(LOG_NORMAL,
			"[SKIPPED] REPORT LUNS response truncated.");
		CU_PASS("REPORT LUNS response truncated.");
	}

	rl_list = scsi_datain_unmarshall(rl_task);
	if (rl_list == NULL) {
		fprintf(stderr, "failed to unmarshall reportluns datain blob\n");
		exit(10);
	}

	for (i = 0; i < rl_list->num; i++) {
		uint32_t j;
		logging(LOG_VERBOSE, "LUN[%u]: 0x%02hx", (unsigned)i,
			(unsigned short)rl_list->luns[i]);
		/* inefficiently check for duplicates */
		for (j = i + 1; j < rl_list->num; j++)
			CU_ASSERT_NOT_EQUAL(rl_list->luns[i], rl_list->luns[j]);
	}

	scsi_free_scsi_task(rl_task);
}
