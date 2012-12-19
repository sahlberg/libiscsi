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
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-test.h"

int T1120_persistent_register_simple(const char *initiator, const char *url, int data_loss _U_, int show_info)
{ 
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_persistent_reserve_in_read_keys *rk;
	int ret, lun, i;

	printf("1120_persistent_register_simple:\n");
	printf("============================================\n");
	if (show_info) {
		printf("Test basic PERSISTENT_RESERVE_OUT/REGISTER functionality.\n");
		printf("1, REGISTER with the target.\n");
		printf("2, Check READ_KEYS that the registration exists.\n");
		printf("3, Remove the registraion using REGISTER_AND_IGNORE_EXISTING_KEY\n");
		printf("4, Check READ_KEYS that the registration is gone.\n");
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

	/* Register at the target */
	printf("Send PERSISTENT_RESERVE_OUT/REGISTER ... ");
	poc.reservation_key			= 0;
	poc.service_action_reservation_key	= 0x6C69626953435349LL;
	poc.spec_i_pt				= 0;
	poc.all_tg_pt				= 0;
	poc.aptpl				= 0;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
			SCSI_PERSISTENT_RESERVE_REGISTER,
			SCSI_PERSISTENT_RESERVE_SCOPE_LU,
			SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
			&poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_OUT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PERSISTENT_RESERVE_OUT Not Supported\n");
		ret = -2;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_OUT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



	/* Verify we can read the registration */
	printf("Check READ_KEYS that the registration exists ... ");
	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
			SCSI_PERSISTENT_RESERVE_READ_KEYS,
			16384);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_IN command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_IN command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rk = scsi_datain_unmarshall(task);
	if (rk == NULL) {
		printf("failed to unmarshall PERSISTENT_RESERVE_IN/READ_KEYS data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	/* verify we can see the key */
	for (i = 0; i < rk->num_keys; i++) {
		if (rk->keys[i] == 0x6C69626953435349LL) {
			break;
		}
	}
	if (i == rk->num_keys) {
		printf("[FAILED]\n");
		printf("Did not find registration in READ_KEYS data\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



	/* Release from the target */
	printf("Remove the registration using REGISTER_AND_IGNORE_EXISTING_KEY ... ");
	poc.reservation_key			= 0;
	poc.service_action_reservation_key	= 0;
	poc.spec_i_pt				= 0;
	poc.all_tg_pt				= 0;
	poc.aptpl				= 0;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
			SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY,
			SCSI_PERSISTENT_RESERVE_SCOPE_LU,
			SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
			&poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_OUT command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PERSISTENT_RESERVE_OUT Not Supported\n");
		ret = -2;
		scsi_free_scsi_task(task);
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_OUT command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



	/* Verify the registration is gone */
	printf("Check READ_KEYS that the registration is gone... ");
	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
			SCSI_PERSISTENT_RESERVE_READ_KEYS,
			16384);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PERSISTENT_RESERVE_IN command: %s\n", iscsi_get_error(iscsi));
		ret = -1;
		goto finished;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PERSISTENT_RESERVE_IN command: failed with sense. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	rk = scsi_datain_unmarshall(task);
	if (rk == NULL) {
		printf("failed to unmarshall PERSISTENT_RESERVE_IN/READ_KEYS data. %s\n", iscsi_get_error(iscsi));
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	/* verify we can see the key */
	for (i = 0; i < rk->num_keys; i++) {
		if (rk->keys[i] == 0x6C69626953435349LL) {
			break;
		}
	}
	if (i != rk->num_keys) {
		printf("[FAILED]\n");
		printf("Registration still remains in the READ_KEYS data\n");
		ret = -1;
		scsi_free_scsi_task(task);
		goto finished;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");



finished:
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}
