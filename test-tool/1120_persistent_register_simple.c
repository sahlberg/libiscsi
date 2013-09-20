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
#include <arpa/inet.h>
#include <string.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"



int T1120_persistent_register_simple(const char *initiator, const char *url)
{ 
	struct iscsi_context *iscsi;
	int ret, lun;
	const unsigned long long key = rand_key();


	printf("1120_persistent_register_simple:\n");
	printf("============================================\n");
	if (show_info) {
		printf("Test basic PERSISTENT_RESERVE_OUT/REGISTER functionality.\n");
		printf("1, Register with a target using REGISTER_AND_IGNORE.\n");
		printf("2, Make sure READ_KEYS sees the registration.\n");
		printf("3, Make sure we cannot REGISTER again\n");
		printf("4, Remove the registration using REGISTER\n");
		printf("5, Make sure READ_KEYS shows the registration is gone.\n");
		printf("\n");
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
	if (ret != 0) {
		goto finished;
	}

	/* verify we can read the registration */
	ret = prin_verify_key_presence(iscsi, lun, key, 1);
	if (ret != 0) {
		goto finished;
	}

	/* try to reregister, which should fail */
	ret = prout_reregister_key_fails(iscsi, lun, key+1);
	if (ret != 0) {
		goto finished;
	}

	/* release from the target */
	ret = prout_register_key(iscsi, lun, 0, key);
	if (ret != 0) {
		goto finished;
	}

	/* Verify the registration is gone */
	/* verify we can read the registration */
	ret = prin_verify_key_presence(iscsi, lun, key, 0);
	if (ret != 0) {
		goto finished;
	}

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
