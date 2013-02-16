/* 
   Copyright (C) 2013 by Lee Duncan <leeman.duncan@gmail.com>

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
#include <arpa/inet.h>
#include <string.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"


/*
 * list of persistent reservation types to test, in order
 */
static enum scsi_persistent_out_type pr_types_to_test[] = {
	SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
	SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS,
	SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY,
	SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY,
	SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS,
	SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS,
	0
};

int T1130_persistent_reserve_simple(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret;
	int lun;
	const unsigned long long key = rand_key();
	int i;

	printf("1130_persistent_reserve_simple:\n");
	printf("=========================================\n");
	if (show_info) {
		int idx = 1;

		printf("Test that we can use each type of Persistent Reservation,\n");
		printf(" and that we can release each type, as well\n");
		printf("%d, We can register a key\n", idx++);
		for (i = 0; pr_types_to_test[i] != 0; i++) {
			printf("%d, Can reserve %s\n", idx++,
			    scsi_pr_type_str(pr_types_to_test[i]));
			printf("%d, Can read reservation\n", idx++);
			printf("%d, Can release reservation\n", idx++);
		}
		printf("%d, Can unregister\n", idx++);
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	
	ret = 0;

	/* register our reservation key with the target */
	ret = prout_register_and_ignore(iscsi, lun, key);
	if (ret != 0)
		goto finished;

	/* test each reservatoin type */
	for (i = 0; pr_types_to_test[i] != 0; i++) {
		enum scsi_persistent_out_type pr_type = pr_types_to_test[i];

		/* reserve the target */
		ret = prout_reserve(iscsi, lun, key, pr_type);
		if (ret != 0)
			goto finished;

		/* verify target reservation */
		ret = prin_verify_reserved_as(iscsi, lun,
		    pr_type_is_all_registrants(pr_type) ? 0 : key,
		    pr_type);
		if (ret != 0)
			goto finished;

		/* release our reservation */
		ret = prout_release(iscsi, lun, key, pr_type);
		if (ret != 0)
			goto finished;
	}

	/* remove our key from the target */
	ret = prout_register_key(iscsi, lun, 0, key);
	if (ret != 0)
		goto finished;

finished:
	/* XXX should we clean up key if needed? */
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
