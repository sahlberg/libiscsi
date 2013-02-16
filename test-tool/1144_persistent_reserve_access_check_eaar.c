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


int T1144_persistent_reserve_access_check_eaar(const char *initiator,
    const char *url)
{ 
	struct iscsi_context *iscsi = NULL, *iscsi2 = NULL;
	int ret;
	int lun, lun2;
	const unsigned long long key = rand_key();
	const unsigned long long key2 = rand_key();
	const enum scsi_persistent_out_type pr_type = 
		SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS;
	const char *pr_type_str = scsi_pr_type_str(pr_type);
	unsigned char *buf = NULL;


	printf("1144_persistent_reserve_access_check_eaar:\n");
	printf("=========================================\n");
	if (show_info) {
		int idx = 1;

		printf("Test that access constrols are correct for %s Persistent Reservations\n",
		    pr_type_str);
		printf("%d, %s Reservation Holder Read Access\n",
		    idx++, pr_type_str);
		printf("%d, %s Reservation Holder Write Access\n",
		    idx++, pr_type_str);
		printf("%d, non-%s Reservation Holder does not have read access\n",
		    idx++, pr_type_str);
		printf("%d, non-%s Reservation Holder does not have write access\n",
		    idx++, pr_type_str);
		return 0;
	}

	iscsi = iscsi_context_login(initiator, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		ret = -1;
		goto finished;
	}
	iscsi2 = iscsi_context_login(initiatorname2, url, &lun2);
	if (iscsi2 == NULL) {
		printf("Failed to login to target (2nd initiator)\n");
		ret = -1;
		goto finished;
	}

	if (!data_loss) {
		printf("--dataloss flag is not set. Skipping test\n");
		ret = -2;
		goto finished;
	}
	
	/* register our reservation key with the target */
	ret = prout_register_and_ignore(iscsi, lun, key);
	if (ret != 0)
		goto finished;
	ret = prout_register_and_ignore(iscsi2, lun2, key2);
	if (ret != 0)
		goto finished;

	/* reserve the target through initiator 1 */
	ret = prout_reserve(iscsi, lun, key, pr_type);
	if (ret != 0)
		goto finished;

	/* verify target reservation */
	ret = prin_verify_reserved_as(iscsi, lun,
	    pr_type_is_all_registrants(pr_type) ? 0 : key,
	    pr_type);
	if (ret != 0)
		goto finished;

	buf = malloc(512);			/* allocate a buffer */
	if (buf == NULL) {
		printf("failed to allocate 512 byes of memory\n");
		ret = -1;
		goto finished;
	}

	/* make sure init1 can read */
	ret = verify_read_works(iscsi, lun, buf);
	if (ret != 0)
		goto finished;

	/* make sure init1 can write */
	ret = verify_write_works(iscsi, lun, buf);
	if (ret != 0)
		goto finished;

	/* make sure init2 does have read access */
	ret = verify_read_works(iscsi2, lun2, buf);
	if (ret != 0)
		goto finished;

	/* make sure init2 does have write access */
	ret = verify_write_works(iscsi2, lun2, buf);
	if (ret != 0)
		goto finished;

	/* unregister init2 */
	ret = prout_register_key(iscsi2, lun2, 0, key);
	if (ret != 0) {
		goto finished;
	}

	/* make sure init2 does not have read access */
	ret = verify_read_fails(iscsi2, lun2, buf);
	if (ret != 0)
		goto finished;

	/* make sure init2 does not have write access */
	ret = verify_write_fails(iscsi2, lun2, buf);
	if (ret != 0)
		goto finished;

	/* release our reservation */
	ret = prout_release(iscsi, lun, key, pr_type);
	if (ret != 0)
		goto finished;

	/* remove our key from the target */
	ret = prout_register_key(iscsi, lun, 0, key);
	if (ret != 0)
		goto finished;

finished:
	/* XXX should we clean up key if needed? */
	if (iscsi) {
		iscsi_logout_sync(iscsi);
		iscsi_destroy_context(iscsi);
	}
	if (iscsi2) {
		iscsi_logout_sync(iscsi2);
		iscsi_destroy_context(iscsi2);
	}
	if (buf)
		free(buf);
	return ret;
}
