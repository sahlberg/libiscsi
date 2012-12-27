/*
   Copyright (C) 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T0251_prefetch16_flags(const char *initiator, const char *url)
{
	struct iscsi_context *iscsi;
	int ret, i, lun;

	printf("0251_prefetch16_flags:\n");
	printf("===================\n");
	if (show_info) {
		printf("Test PREFETCH16 flags.\n");
		printf("1, Test the IMMED flag.\n");
		printf("2, Test different GROUPNUMBERS.\n");
		printf("\n");
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}


	ret = 0;

	/* prefetch with IMMED==1 */
	printf("Check PREFETCH16 with IMMED==1.\n");
	ret = prefetch16(iscsi, lun, 0, 1, 1, 0);
	if (ret != 0) {
		goto finished;
	}


	/* Prefetch with GROUPNUMBER==0..31 */
	printf("Check PREFETCH16 with GROUPNEMBER 0-31.\n");
	for (i = 0; i < 32; i++) {
		ret = prefetch16(iscsi, lun, 0, 1, 0, i);
		if (ret != 0) {
			goto finished;
		}
	}


finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
