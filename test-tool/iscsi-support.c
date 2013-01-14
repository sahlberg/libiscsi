/*
   iscsi-test tool support

   Copyright (C) 2012 by Lee Duncan <leeman.duncan@gmail.com>

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

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <popt.h>
#include <fnmatch.h>
#include "slist.h"
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"
#include "iscsi-support.h"


/*****************************************************************
 * globals
 *****************************************************************/
const char *initiatorname1 =
	"iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test";
const char *initiatorname2 =
	"iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-test-2";

const char *tgt_url;

uint32_t block_size;
uint64_t num_blocks;
int lbpme;
int lbppb;
int lbpme;
int removable;
enum scsi_inquiry_peripheral_device_type device_type;
int sccs;
int encserv;
int data_loss;


int (*real_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

void logging(int level, const char *format, ...)
{
        va_list ap;
	static char message[1024];
	int ret;

	if (loglevel < level) {
		return;
	}

        va_start(ap, format);
	ret = vsnprintf(message, 1024, format, ap);
        va_end(ap);

	if (ret < 0) {
		return;
	}

	printf("    %s\n", message);
}


struct iscsi_context *
iscsi_context_login(const char *initiatorname, const char *url, int *lun)
{
	struct iscsi_context *iscsi;
	struct iscsi_url *iscsi_url;

	iscsi = iscsi_create_context(initiatorname);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		return NULL;
	}

	iscsi_url = iscsi_parse_full_url(iscsi, url);
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n",
			iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return NULL;
	}

	iscsi_set_targetname(iscsi, iscsi_url->target);
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_url->user != NULL) {
		if (iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
			fprintf(stderr, "Failed to set initiator username and password\n");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			return NULL;
		}
	}

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		return NULL;
	}
	if (lun != NULL) {
		*lun = iscsi_url->lun;
	}

	iscsi_destroy_url(iscsi_url);
	return iscsi;
}

void
wait_until_test_finished(struct iscsi_context *iscsi, struct iscsi_async_state *state)
{
	struct pollfd pfd;
	int count = 0;
	int ret;

	while (state->finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		ret = poll(&pfd, 1, 1000);
		if (ret < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (ret == 0) {
			if (count++ > 5) {
				struct iscsi_pdu *pdu;

				state->finished     = 1;
				state->status       = SCSI_STATUS_CANCELLED;
				state->task->status = SCSI_STATUS_CANCELLED;
				/* this may leak memory since we dont free the pdu */
				while ((pdu = iscsi->outqueue)) {
					SLIST_REMOVE(&iscsi->outqueue, pdu);
				}
				while ((pdu = iscsi->waitpdu)) {
					SLIST_REMOVE(&iscsi->waitpdu, pdu);
				}
				return;
			}
			continue;
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			printf("iscsi_service failed with : %s\n", iscsi_get_error(iscsi));
			break;
		}
	}
}

int
iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	if (local_iscsi_queue_pdu != NULL) {
		local_iscsi_queue_pdu(iscsi, pdu);
	}
	return real_iscsi_queue_pdu(iscsi, pdu);
}

int
register_and_ignore(struct iscsi_context *iscsi, int lun,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	/* register our reservation key with the target */
	printf("Send PROUT/REGISTER_AND_IGNORE to register init=%s ... ",
	    iscsi->initiator_name);
	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PROUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION &&
	    task->sense.key == SCSI_SENSE_ILLEGAL_REQUEST &&
	    task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PROUT Not Supported\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PROUT command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}


int
register_key(struct iscsi_context *iscsi, int lun,
    unsigned long long sark, unsigned long long rk)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	/* register our reservation key with the target */
	printf("Send PROUT/REGISTER to %s init=%s... ",
	    sark != 0 ? "register" : "unregister",
	    iscsi->initiator_name);
	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	poc.reservation_key = rk;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU, 0, &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PROUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PROUT command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}


int
verify_key_presence(struct iscsi_context *iscsi, int lun,
    unsigned long long key, int present)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	int i;
	int key_found;
	struct scsi_persistent_reserve_in_read_keys *rk = NULL;


	printf("Send PRIN/READ_KEYS to verify key %s init=%s... ",
	    present ? "present" : "absent",
	    iscsi->initiator_name);
	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_KEYS, buf_sz);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PRIN command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PRIN command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	rk = scsi_datain_unmarshall(task);
	if (rk == NULL) {
		printf("failed to unmarshall PRIN/READ_KEYS data. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);

	key_found = 0;
	for (i = 0; i < rk->num_keys; i++) {
		if (rk->keys[i] == key)
			key_found = 1;
	}

	if ((present && key_found) ||
	    (!present && !key_found)) {
		printf("[OK]\n");
		return 0;
	} else {
	        printf("[FAILED]\n");
		if (present)
			printf("Key found when none expected\n");
		else
			printf("Key not found when expected\n");
		return -1;
	}
}


int
reregister_key_fails(struct iscsi_context *iscsi, int lun,
    unsigned long long sark)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	printf("Send PROUT/REGISTER to ensure reregister fails init=%s... ",
	    iscsi->initiator_name);
	memset(&poc, 0, sizeof (poc));
	poc.service_action_reservation_key = sark;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_REGISTER,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
	    &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PROUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	
	if (task->status != SCSI_STATUS_CHECK_CONDITION ||
	    task->sense.key != SCSI_SENSE_ILLEGAL_REQUEST ||
	    task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[FAILED]\n");
		printf("PROUT/REGISTER when already registered should fail\n");
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


int
reserve(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	/* reserve the target using specified reservation type */
	printf("Send PROUT/RESERVE to reserve, type=%d (%s) init=%s ... ",
	    pr_type, scsi_pr_type_str(pr_type),
	    iscsi->initiator_name);

	memset(&poc, 0, sizeof (poc));
	poc.reservation_key = key;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_RESERVE,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    pr_type, &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PROUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PROUT command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}


int
release(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_persistent_reserve_out_basic poc;
	struct scsi_task *task;


	/* release the target using specified reservation type */
	printf("Send PROUT/RELEASE to release reservation, type=%d init=%s ... ",
	    pr_type, iscsi->initiator_name);

	memset(&poc, 0, sizeof (poc));
	poc.reservation_key = key;
	task = iscsi_persistent_reserve_out_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_RELEASE,
	    SCSI_PERSISTENT_RESERVE_SCOPE_LU,
	    pr_type, &poc);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PROUT command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PROUT command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);
	printf("[OK]\n");

	return 0;
}

int
verify_reserved_as(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type)
{
	struct scsi_task *task;
	const int buf_sz = 16384;
	struct scsi_persistent_reserve_in_read_reservation *rr = NULL;


	printf("Send PRIN/READ_RESERVATION to verify type=%d init=%s... ",
	    pr_type, iscsi->initiator_name);
	task = iscsi_persistent_reserve_in_sync(iscsi, lun,
	    SCSI_PERSISTENT_RESERVE_READ_RESERVATION, buf_sz);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PRIN command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PRIN command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	rr = scsi_datain_unmarshall(task);
	if (rr == NULL) {
		printf("failed to unmarshall PRIN/READ_RESERVATION data. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	scsi_free_scsi_task(task);


	if (!rr->reserved) {
	        printf("[FAILED]\n");
		printf("Failed to find Target reserved as expected.\n");
		return -1;
	}
	if (rr->reservation_key != key) {
	        printf("[FAILED]\n");
		printf("Failed to find reservation key 0x%llx: found 0x%lx.\n",
		    key, rr->reservation_key);
		return -1;
	}
	if (rr->pr_type != pr_type) {
	        printf("[FAILED]\n");
		printf("Failed to find reservation type %d: found %d.\n",
		    pr_type, rr->pr_type);
		return -1;
	}

	printf("[OK]\n");
	return 0;
}


int
verify_read_works(struct iscsi_context *iscsi, int lun, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;

	/*
	 * try to read the second 512-byte block
	 */

	printf("Send READ10 to verify READ works init=%s ... ",
	    iscsi->initiator_name);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blksize, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ10 command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("READ10 command: failed with sense: %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	memcpy(buf, task->datain.data, task->datain.size);
	scsi_free_scsi_task(task);

	printf("[OK]\n");
	return 0;
}

int
verify_write_works(struct iscsi_context *iscsi, int lun, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;

	/*
	 * try to write the second 512-byte block
	 */

	printf("Send WRITE10 to verify WRITE works init=%s ... ",
	    iscsi->initiator_name);

	task = iscsi_write10_sync(iscsi, lun, lba, buf, datalen, blksize, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE10 command: failed with sense: %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);

	printf("[OK]\n");
	return 0;
}


int
verify_read_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;

	/*
	 * try to read the second 512-byte block -- should fail
	 */

	printf("Send READ10 to verify READ does not work init=%s ... ",
	    iscsi->initiator_name);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blksize, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send READ10 command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		memcpy(buf, task->datain.data, task->datain.size);
	        printf("[FAILED]\n");
		printf("READ10 command succeeded when expected to fail\n");
		scsi_free_scsi_task(task);
		return -1;
	}

	/*
	 * XXX should we verify sense data?
	 */

	scsi_free_scsi_task(task);

	printf("[OK]\n");
	return 0;
}

int
verify_write_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf)
{
	struct scsi_task *task;
	const uint32_t lba = 1;
	const int blksize = 512;
	const uint32_t datalen = 1 * blksize;

	/*
	 * try to write the second 512-byte block
	 */

	printf("Send WRITE10 to verify WRITE does not work init=%s ... ",
	    iscsi->initiator_name);

	task = iscsi_write10_sync(iscsi, lun, lba, buf, datalen, blksize, 0, 0, 0, 0, 0);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send WRITE10 command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("WRITE10 command: succeeded when exptec to fail\n");
		scsi_free_scsi_task(task);
		return -1;
	}

	/*
	 * XXX should we verify sense data?
	 */

	scsi_free_scsi_task(task);

	printf("[OK]\n");
	return 0;
}

int
testunitready(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send TESTUNITREADY (Expecting SUCCESS)");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
		logging(LOG_NORMAL,
			"[FAILED] Failed to send TESTUNITREADY command: %s",
			iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL,
			"[FAILED] TESTUNITREADY command: failed with sense. %s",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] TestUnitReady returned SUCCESS.");
	return 0;
}

int
testunitready_nomedium(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	printf("Send TESTUNITREADY (expecting NOT_READY/MEDIUM_NOT_PRESENT) ... ");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("TESTUNITREADY Should have failed with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		scsi_free_scsi_task(task);
		return -1;
	}	
	scsi_free_scsi_task(task);
	printf("[OK]\n");
	return 0;
}

int
testunitready_conflict(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;

	printf("Send TESTUNITREADY (expecting RESERVATION_CONFLICT) ... ");
	task = iscsi_testunitready_sync(iscsi, lun);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send TESTUNITREADY command: %s\n",
		    iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_RESERVATION_CONFLICT) {
		printf("[FAILED]\n");
		printf("Expected RESERVATION CONFLICT\n");
		return -1;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");
	return 0;
}

int
prefetch10(struct iscsi_context *iscsi, int lun, uint32_t lba, int num, int immed, int group)
{
	struct scsi_task *task;

	printf("Send PREFETCH10 LBA:%d Count:%d IMEMD:%d GROUP:%d ... ", lba, num, immed, group);
	task = iscsi_prefetch10_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PREFETCH10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PREFETCH10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");
	return 0;
}

int
prefetch10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
    int num, int immed, int group)
{
	struct scsi_task *task;

	printf("Send PREFETCH10 LBA:%d Count:%d IMEMD:%d GROUP:%d (expecting ILLEGAL_REQUEST/LBA_OUT_OF_RANGE) ... ", lba, num, immed, group);
	task = iscsi_prefetch10_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PREFETCH10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("PREFETCH10 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
prefetch10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba,
    int num, int immed, int group)
{
	struct scsi_task *task;

	printf("Send PREFETCH10 LBA:%d Count:%d IMEMD:%d GROUP:%d (expecting NOT_READY/MEDIUM_NOT_PRESENT) ... ", lba, num, immed, group);
	task = iscsi_prefetch10_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PREFETCH10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("PREFETCH10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		scsi_free_scsi_task(task);
		return -1;
	}	

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
prefetch16(struct iscsi_context *iscsi, int lun, uint64_t lba, int num,
    int immed, int group)
{
	struct scsi_task *task;

	printf("Send PREFETCH16 LBA:%" PRIu64 " Count:%d IMEMD:%d GROUP:%d ... ", lba, num, immed, group);
	task = iscsi_prefetch16_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PREFETCH16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("PREFETCH16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}
	scsi_free_scsi_task(task);
	printf("[OK]\n");
	return 0;
}

int
prefetch16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, int num, int immed, int group)
{
	struct scsi_task *task;

	printf("Send PREFETCH16 LBA:%" PRIu64 " Count:%d IMEMD:%d GROUP:%d (expecting ILLEGAL_REQUEST/LBA_OUT_OF_RANGE) ... ", lba, num, immed, group);
	task = iscsi_prefetch16_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PREFETCH16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("PREFETCH16 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
prefetch16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, int num, int immed, int group)
{
	struct scsi_task *task;

	printf("Send PREFETCH16 LBA:%" PRIu64 " Count:%d IMEMD:%d GROUP:%d (expecting NOT_READY/MEDIUM_NOT_PRESENT) ... ", lba, num, immed, group);
	task = iscsi_prefetch16_sync(iscsi, lun, lba, num, immed, group);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send PREFETCH16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("PREFETCH16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("PREFETCH16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		scsi_free_scsi_task(task);
		return -1;
	}	

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
read10(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 LBA:%d blocks:%d rdprotect:%d "
	       "dpo:%d fua:%d fua_nv:%d group:%d",
	       lba, datalen / blocksize, rdprotect,
	       dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 command: "
			"failed with sense. %s", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned SUCCESS.");
	return 0;
}

int
read10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 (Expecting INVALID_FIELD_IN_CDB) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 successful but should "
			"have failed with ILLEGAL_REQUEST/INVALID_FIELD_IN_CDB");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB) {
		logging(LOG_NORMAL, "[FAILED] READ10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"INVALID_FIELD_IN_CDB. Sense:%s\n",
			iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned ILLEGAL_REQUEST/INVALID_FIELD_IB_CDB.");
	return 0;
}

int
read10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba,
       uint32_t datalen, int blocksize, int rdprotect, 
       int dpo, int fua, int fua_nv, int group,
       unsigned char *data)
{
	struct scsi_task *task;

	logging(LOG_VERBOSE, "Send READ10 (Expecting LBA_OUT_OF_RANGE) "
		"LBA:%d blocks:%d rdprotect:%d "
		"dpo:%d fua:%d fua_nv:%d group:%d",
		lba, datalen / blocksize, rdprotect,
		dpo, fua, fua_nv, group);

	task = iscsi_read10_sync(iscsi, lun, lba, datalen, blocksize,
				 rdprotect, dpo, fua, fua_nv, group);
	if (task == NULL) {
		logging(LOG_NORMAL, "[FAILED] Failed to send READ10 command: %s",
		       iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		logging(LOG_NORMAL, "[FAILED] READ10 successful but should "
			"have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		logging(LOG_NORMAL, "[FAILED] READ10 failed with wrong sense. "
			"Should have failed with ILLEGAL_REQUEST/"
			"LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	if (data != NULL) {
		memcpy(data, task->datain.data, task->datain.size);
	}

	scsi_free_scsi_task(task);
	logging(LOG_VERBOSE, "[OK] READ10 returned ILLEGAL_REQUEST/LBA_OUT_OF_RANGE.");
	return 0;
}

int
verify10(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY10 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify10_nomedium(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY10 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting NOT_READY/MEDIUM_NOT_PRESENT) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("VERIFY10 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		scsi_free_scsi_task(task);
		return -1;
	}	

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify10_miscompare(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY10 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting MISCOMPARE) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY10 command successful but should have failed with MISCOMPARE\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->sense.key != SCSI_SENSE_MISCOMPARE) {
		printf("[FAILED]\n");
		printf("VERIFY10 command returned wrong sense key. MISCOMPARE MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s\n", SCSI_SENSE_MISCOMPARE, task->sense.key, iscsi_get_error(iscsi)); 
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify10_lbaoutofrange(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY10 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting LBA_OUT_OF_RANGE) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify10_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY10 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY10 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY10 command successful but should have failed with LBA_OUT_OF_RANGE\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("VERIFY10 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify12(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY12 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY12 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY12 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY12 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify12_nomedium(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY12 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting NOT_READY/MEDIUM_NOT_PRESENT) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY12 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY12 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("VERIFY12 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		scsi_free_scsi_task(task);
		return -1;
	}	

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify12_miscompare(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY12 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting MISCOMPARE) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY12 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY12 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY12 command successful but should have failed with MISCOMPARE\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->sense.key != SCSI_SENSE_MISCOMPARE) {
		printf("[FAILED]\n");
		printf("VERIFY12 command returned wrong sense key. MISCOMPARE MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s\n", SCSI_SENSE_MISCOMPARE, task->sense.key, iscsi_get_error(iscsi)); 
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify12_lbaoutofrange(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY12 LBA:%d blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting LBA_OUT_OF_RANGE) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify12_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY12 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY12 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY12 command successful but should have failed with LBA_OUT_OF_RANGE\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("VERIFY12 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify16(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY16 LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY16 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify16_nomedium(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY16 LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting NOT_READY/MEDIUM_NOT_PRESENT) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
	    || task->sense.key  != SCSI_SENSE_NOT_READY
	    || (task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN
	        && task->sense.ascq != SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED)) {
		printf("[FAILED]\n");
		printf("VERIFY16 after eject failed with the wrong sense code. Should fail with NOT_READY/MEDIUM_NOT_PRESENT*\n");
		scsi_free_scsi_task(task);
		return -1;
	}	

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify16_miscompare(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY16 LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting MISCOMPARE) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY16 command successful but should have failed with MISCOMPARE\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->sense.key != SCSI_SENSE_MISCOMPARE) {
		printf("[FAILED]\n");
		printf("VERIFY16 command returned wrong sense key. MISCOMPARE MISCOMPARE 0x%x expected but got key 0x%x. Sense:%s\n", SCSI_SENSE_MISCOMPARE, task->sense.key, iscsi_get_error(iscsi)); 
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
verify16_lbaoutofrange(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	printf("Send VERIFY16 LBA:%" PRIu64 " blocks:%d vprotect:%d dpo:%d bytchk:%d (expecting LBA_OUT_OF_RANGE) ... ", lba, datalen / blocksize, vprotect, dpo, bytchk);
	task = iscsi_verify16_sync(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send VERIFY16 command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status        == SCSI_STATUS_CHECK_CONDITION
	    && task->sense.key  == SCSI_SENSE_ILLEGAL_REQUEST
	    && task->sense.ascq == SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE) {
		printf("[SKIPPED]\n");
		printf("VERIFY16 is not implemented on target\n");
		scsi_free_scsi_task(task);
		return -2;
	}
	if (task->status == SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("VERIFY16 command successful but should have failed with LBA_OUT_OF_RANGE\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	if (task->status        != SCSI_STATUS_CHECK_CONDITION
		|| task->sense.key  != SCSI_SENSE_ILLEGAL_REQUEST
		|| task->sense.ascq != SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE) {
		printf("[FAILED]\n");
		printf("VERIFY16 should have failed with ILLEGAL_REQUEST/LBA_OUT_OF_RANGE. Sense:%s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

int
inquiry(struct iscsi_context *iscsi, int lun, int evpd, int page_code, int maxsize)
{
	struct scsi_task *task;

	printf("Send INQUIRY evpd:%d page_code:%d ... ", evpd, page_code);
	task = iscsi_inquiry_sync(iscsi, lun, evpd, page_code, maxsize);
	if (task == NULL) {
	        printf("[FAILED]\n");
		printf("Failed to send INQUIRY command: %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
	        printf("[FAILED]\n");
		printf("INQUIRY command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return -1;
	}

	printf("[OK]\n");
	scsi_free_scsi_task(task);
	return 0;
}

