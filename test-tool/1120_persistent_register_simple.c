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
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"


static inline long rand_key(void)
{
	time_t t;
	pid_t p;
	unsigned int s;
	long l;

	(void)time(&t);
	p = getpid();
	s = ((int)p * (t & 0xffff));
	srandom(s);
	l = random();
	return l;
}

int register_and_ignore(struct iscsi_context *iscsi, int lun,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	/* register our reservation key with the target */
	printf("Send PROUT/REGISTER_AND_IGNORE to register ... ");
	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_OUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION &&
	    task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST &&
	    task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PERSISTENT_RESERVE_OUT Not Supported\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_OUT command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}


int register_key(struct iscsi_context *iscsi, int lun,
    unsigned long long sark, unsigned long long rk)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	/* register our reservation key with the target */
	printf("Send PROUT/REGISTER to register ... ");
	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	poc.reservation_key = rk;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_OUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_OUT command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}

static int verify_key_presence(struct iscsi_context *iscsi, int lun,
    unsigned long long key, int present)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	int i;
	int key_found;
	struct scsi_persistent_reserve_in_read_keys *rk = NULL;


	printf("Send PRIN/READ_KEYS to verify key %s ... ",
	    present ? "present" : "absent");
	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_KEYS, buf_sz);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_IN command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_IN command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	rk = scsi_datain_unmarshall(task);
	if (rk == NULL) {
		printf("failed to unmarshall PERSISTENT_RESERVE_IN/READ_KEYS data. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);

	key_found = 0;
	for (i = 0; i < rk->num_keys; i++) {
		if (rk->keys[i] == key) {
			key_found = 1;
		}
	}

	if ((present && key_found) ||
	    (!present && !key_found)) {
		printf("[OK]\n");
		return 0;
	} else {
	        printf("[FAILED]\n");
		if (present) {
			printf("Key found when none expected\n");
		} else {
			printf("Key not found when expected\n");
		}
		return -1;
	}
}


int reregister_fails(struct iscsi_context *iscsi, int lun,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	printf("Send PROUT/REGISTER to ensure reregister fails ... ");
	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
	    &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_OUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	
	if (task->status != SCSI_STATUS_CHECK_CONDITION ||
	    task->sense.key != SCSI_SENSE_ILLEGAL_REQUEST ||
	    task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[FAILED]\n");
		printf("PRIN/REGISTER when already registered should fail\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PROUT/REGISTER command: succeeded when it should not have!\n");
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}


int T1120_persistent_register_simple(const char *initiator, const char *url,
    int data_loss _U_, int show_info)
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
		printf("4, Remove the registraion using REGISTER\n");
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
	ret = register_and_ignore(iscsi, lun, key);
	if (ret != 0) {
		goto finished;
	}

	/* verify we can read the registration */
	ret = verify_key_presence(iscsi, lun, key, 1);
	if (ret != 0) {
		goto finished;
	}

	/* try to reregister, which should fail */
	ret = reregister_fails(iscsi, lun, key+1);
	if (ret != 0) {
		goto finished;
	}

	/* release from the target */
	ret = register_key(iscsi, lun, 0, key);
	if (ret != 0) {
		goto finished;
	}

	/* Verify the registration is gone */
	/* verify we can read the registration */
	ret = verify_key_presence(iscsi, lun, key, 0);
	if (ret != 0) {
		goto finished;
	}

finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
