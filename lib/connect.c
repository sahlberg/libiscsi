/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#if defined(WIN32)
#include "win32/win32_compat.h"
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "slist.h"
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

struct connect_task {
	iscsi_command_cb cb;
	void *private_data;
	int lun;
};

static void
iscsi_connect_cb(struct iscsi_context *iscsi, int status, void *command_data _U_,
		 void *private_data);

static void
iscsi_testunitready_cb(struct iscsi_context *iscsi, int status,
		       void *command_data, void *private_data)
{
	struct connect_task *ct = private_data;
	struct scsi_task *task = command_data;

	if (status != 0) {
		if (task->sense.key == SCSI_SENSE_UNIT_ATTENTION
		    && (task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET ||
			task->sense.ascq == SCSI_SENSE_ASCQ_POWER_ON_OCCURED ||
			task->sense.ascq == SCSI_SENSE_ASCQ_NEXUS_LOSS)) {
			/* This is just the normal unitattention/busreset
			 * you always get just after a fresh login. Try
			 * again.
			 */
			if (iscsi_testunitready_task(iscsi, ct->lun,
						      iscsi_testunitready_cb,
						      ct) == NULL) {
				iscsi_set_error(iscsi, "iscsi_testunitready "
						"failed.");
				ct->cb(iscsi, SCSI_STATUS_ERROR, NULL,
				       ct->private_data);
				iscsi_free(iscsi, ct);
			}
			scsi_free_scsi_task(task);
			return;
		}
	}

	/* Don't fail the login just because there is no medium in the device */
	if (status != 0
	&& task->sense.key == SCSI_SENSE_NOT_READY
	&& (task->sense.ascq == SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT
	 || task->sense.ascq == SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED
	 || task->sense.ascq == SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN)) {
		status = 0;
	}

	/* Don't fail the login just because the medium is reserved */
	if (status == SCSI_STATUS_RESERVATION_CONFLICT) {
		status = 0;
	}

	/* Don't fail the login just because there is a sanitize in progress */
	if (status != 0
	&& task->sense.key == SCSI_SENSE_NOT_READY
	    && task->sense.ascq == SCSI_SENSE_ASCQ_SANITIZE_IN_PROGRESS) {
		status = 0;
	}

	ct->cb(iscsi, status?SCSI_STATUS_ERROR:SCSI_STATUS_GOOD, NULL,
	       ct->private_data);
	scsi_free_scsi_task(task);
	iscsi_free(iscsi, ct);
}

static void
iscsi_login_cb(struct iscsi_context *iscsi, int status, void *command_data _U_,
	       void *private_data)
{
	struct connect_task *ct = private_data;

	if (status == SCSI_STATUS_REDIRECT && iscsi->target_address[0]) {
		iscsi_disconnect(iscsi);
		if (iscsi->bind_interfaces[0]) iscsi_decrement_iface_rr();
		if (iscsi_connect_async(iscsi, iscsi->target_address, iscsi_connect_cb, iscsi->connect_data) != 0) {
			iscsi_free(iscsi, ct);
			return;
		}
		return;
	}

	if (status != 0) {
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
		iscsi_free(iscsi, ct);
		return;
	}

	if (iscsi_testunitready_task(iscsi, ct->lun,
				      iscsi_testunitready_cb, ct) == NULL) {
		iscsi_set_error(iscsi, "iscsi_testunitready_async failed.");
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
	}
}

static void
iscsi_connect_cb(struct iscsi_context *iscsi, int status, void *command_data _U_,
		 void *private_data)
{
	struct connect_task *ct = private_data;

	if (status != 0) {
		iscsi_set_error(iscsi, "Failed to connect to iSCSI socket. "
				"%s", iscsi_get_error(iscsi));
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
		iscsi_free(iscsi, ct);
		return;
	}

	if (iscsi_login_async(iscsi, iscsi_login_cb, ct) != 0) {
		iscsi_set_error(iscsi, "iscsi_login_async failed.");
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
		iscsi_free(iscsi, ct);
	}
}


int
iscsi_full_connect_async(struct iscsi_context *iscsi, const char *portal,
			 int lun, iscsi_command_cb cb, void *private_data)
{
	struct connect_task *ct;

	iscsi->lun = lun;
	if (iscsi->portal != portal)
	 strncpy(iscsi->portal,portal,MAX_STRING_SIZE);

	ct = iscsi_malloc(iscsi, sizeof(struct connect_task));
	if (ct == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory. Failed to allocate "
				"connect_task structure.");
		return -ENOMEM;
	}
	ct->cb           = cb;
	ct->lun          = lun;
	ct->private_data = private_data;
	if (iscsi_connect_async(iscsi, portal, iscsi_connect_cb, ct) != 0) {
		iscsi_free(iscsi, ct);
		return -ENOMEM;
	}
	return 0;
}

/* Set auto reconnect status. If state !0  then we will not reconnect
   automatically upon session failure.
*/
void iscsi_set_noautoreconnect(struct iscsi_context *iscsi, int state)
{
	iscsi->no_auto_reconnect = state;

	/* If the session was dropped while auto reconnect was disabled
	   then we explicitely reconnect here again.
	 */
	if (!state && iscsi->reconnect_deferred) {
		iscsi->reconnect_deferred = 0;
		iscsi_reconnect(iscsi);
	}
}

void iscsi_set_reconnect_max_retries(struct iscsi_context *iscsi, int count)
{
	iscsi->reconnect_max_retries = count;
}

void iscsi_defer_reconnect(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;

	iscsi->reconnect_deferred = 1;

	ISCSI_LOG(iscsi, 2, "reconnect deferred, cancelling all tasks");

	while ((pdu = iscsi->outqueue)) {
		SLIST_REMOVE(&iscsi->outqueue, pdu);
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			/* If an error happened during connect/login,
			   we don't want to call any of the callbacks.
			 */
			if (iscsi->is_loggedin) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED,
					      NULL, pdu->private_data);
			}		      
		}
		iscsi_free_pdu(iscsi, pdu);
	}
	while ((pdu = iscsi->waitpdu)) {
		SLIST_REMOVE(&iscsi->waitpdu, pdu);
		/* If an error happened during connect/login,
		   we don't want to call any of the callbacks.
		 */
		if (iscsi->is_loggedin) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED,
				      NULL, pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}
}

int iscsi_reconnect(struct iscsi_context *old_iscsi)
{
	struct iscsi_context *iscsi;
	int retry = 0;

	/* if there is already a deferred reconnect do not try again */
	if (old_iscsi->reconnect_deferred) {
		ISCSI_LOG(old_iscsi, 2, "reconnect initiated, but reconnect is already deferred");
		return -1;
	}

	ISCSI_LOG(old_iscsi, 2, "reconnect initiated");

	/* This is mainly for tests, where we do not want to automatically
	   reconnect but rather want the commands to fail with an error
	   if the target drops the session.
	 */
	if (old_iscsi->no_auto_reconnect) {
		iscsi_defer_reconnect(old_iscsi);
		return 0;
	}

	if (old_iscsi->last_reconnect) {
		if (time(NULL) - old_iscsi->last_reconnect < 5) sleep(5);
	}

try_again:

	iscsi = iscsi_create_context(old_iscsi->initiator_name);
	if (iscsi == NULL) {
		ISCSI_LOG(old_iscsi, 2, "failed to create new context for reconnection");
		return -1;
	}

	iscsi->is_reconnecting = 1;

	iscsi_set_targetname(iscsi, old_iscsi->target_name);

	iscsi_set_header_digest(iscsi, old_iscsi->want_header_digest);

	if (old_iscsi->user[0]) {
		iscsi_set_initiator_username_pwd(iscsi, old_iscsi->user, old_iscsi->passwd);
	}

	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);

	iscsi->lun = old_iscsi->lun;

	strncpy(iscsi->portal,old_iscsi->portal,MAX_STRING_SIZE);
	
	strncpy(iscsi->bind_interfaces,old_iscsi->bind_interfaces,MAX_STRING_SIZE);
	iscsi->bind_interfaces_cnt = old_iscsi->bind_interfaces_cnt;
	
	iscsi->log_level = old_iscsi->log_level;
	iscsi->log_fn = old_iscsi->log_fn;
	iscsi->tcp_user_timeout = old_iscsi->tcp_user_timeout;
	iscsi->tcp_keepidle = old_iscsi->tcp_keepidle;
	iscsi->tcp_keepcnt = old_iscsi->tcp_keepcnt;
	iscsi->tcp_keepintvl = old_iscsi->tcp_keepintvl;
	iscsi->tcp_syncnt = old_iscsi->tcp_syncnt;

	iscsi->reconnect_max_retries = old_iscsi->reconnect_max_retries;

	if (iscsi_full_connect_sync(iscsi, iscsi->portal, iscsi->lun) != 0) {
		int backoff = retry;

		if (iscsi->reconnect_max_retries != -1 && retry >= iscsi->reconnect_max_retries) {
			iscsi_defer_reconnect(old_iscsi);
			iscsi_destroy_context(iscsi);
			return -1;
		}
		if (backoff > 10) {
			backoff += rand() % 10;
			backoff -= 5;
		}
		if (backoff > 30) {
			backoff = 30;
		}
		ISCSI_LOG(old_iscsi, 1, "reconnect try %d failed, waiting %d seconds", retry, backoff);
		iscsi_destroy_context(iscsi);
		sleep(backoff);
		retry++;
		goto try_again;
	}

	while (old_iscsi->outqueue) {
		struct iscsi_pdu *pdu = old_iscsi->outqueue;
		SLIST_REMOVE(&old_iscsi->outqueue, pdu);
		SLIST_ADD_END(&old_iscsi->waitpdu, pdu);
	}

	while (old_iscsi->waitpdu) {
		struct iscsi_pdu *pdu = old_iscsi->waitpdu;

		SLIST_REMOVE(&old_iscsi->waitpdu, pdu);
		if (pdu->itt == 0xffffffff) {
			continue;
		}

		if (pdu->flags & ISCSI_PDU_DROP_ON_RECONNECT) {
			/* We don't want to requeue things like DATA-OUT since these guys
			 * will be reissued automatically anyway once the corresponding
			 * write command is replayed.
			 * Similarly we don't want to requeue NOPs. 
		 	 */
			iscsi_free_pdu(old_iscsi, pdu);
			continue;
		}

		pdu->itt   = iscsi_itt_post_increment(iscsi);
		iscsi_pdu_set_itt(pdu, pdu->itt);

		/* do not increase cmdsn for PDUs marked for immediate delivery
		 * this will result in a protocol error */
		pdu->cmdsn = (pdu->outdata.data[0] & ISCSI_PDU_IMMEDIATE)?iscsi->cmdsn:iscsi->cmdsn++;
		iscsi_pdu_set_cmdsn(pdu, pdu->cmdsn);

		iscsi_pdu_set_expstatsn(pdu, iscsi->statsn);
		iscsi->statsn++;

		pdu->outdata_written = 0;
		pdu->payload_written = 0;
		iscsi_queue_pdu(iscsi, pdu);
	}

	if (dup2(iscsi->fd, old_iscsi->fd) == -1) {
		iscsi_destroy_context(iscsi);
		goto try_again;
	}
	
	if (old_iscsi->incoming != NULL) {
		iscsi_free_iscsi_in_pdu(old_iscsi, old_iscsi->incoming);
	}
	if (old_iscsi->inqueue != NULL) {
		iscsi_free_iscsi_inqueue(old_iscsi, old_iscsi->inqueue);
	}

	if (old_iscsi->outqueue_current != NULL && old_iscsi->outqueue_current->flags & ISCSI_PDU_DELETE_WHEN_SENT) {
		iscsi_free_pdu(old_iscsi, old_iscsi->outqueue_current);
	}

	close(iscsi->fd);
	iscsi->fd = old_iscsi->fd;
	iscsi->mallocs+=old_iscsi->mallocs;
	iscsi->frees+=old_iscsi->frees;

	memcpy(old_iscsi, iscsi, sizeof(struct iscsi_context));
	free(iscsi);

	ISCSI_LOG(old_iscsi, 2, "reconnect was successful");

	old_iscsi->is_reconnecting = 0;
	old_iscsi->last_reconnect = time(NULL);

	return 0;
}
