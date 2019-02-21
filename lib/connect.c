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
#if defined(_WIN32)
#include "win32/win32_compat.h"
#else
#include <unistd.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
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
	int num_uas;
};

static void
iscsi_connect_cb(struct iscsi_context *iscsi, int status, void *command_data _U_,
		 void *private_data);


/* During a reconnect all new SCSI commands are normally deferred to the
 * old context and not actually issued until we have completed the re-connect
 * and switched the contexts.
 * This is what we want most of the time. However, IF we want to send TURs
 * during the reconnect to eat all the UAs, then we want to send them out
 * on this temporary context and NOT just queue them for until later.
 * Hence this function.
 *
 * By setting ->old_iscsi temporarily to NULL when we are creating the TUR
 * we avoid the check in iscsi_scsi_command_async() that otehrwise will try
 * to defer this command until later.
 */
static struct scsi_task *
iscsi_testunitready_connect(struct iscsi_context *iscsi, int lun,
			    iscsi_command_cb cb, void *private_data)
{
	struct scsi_task *task;
	struct iscsi_context *old_iscsi = iscsi->old_iscsi;

	iscsi->old_iscsi = NULL;
	task = iscsi_testunitready_task(iscsi, lun, cb, private_data);
	iscsi->old_iscsi = old_iscsi;

	return task;
}

static void
iscsi_testunitready_cb(struct iscsi_context *iscsi, int status,
		       void *command_data, void *private_data)
{
	struct connect_task *ct = private_data;
	struct scsi_task *task = command_data;

	if (status != 0) {
		if (task->sense.key == SCSI_SENSE_UNIT_ATTENTION) {
			/* This is just the normal unitattention/busreset
			 * you always get just after a fresh login. Try
			 * again. Instead of enumerating all of them we
			 * assume that there will be at most 10 or else
			 * there is something broken.
			 */
			ct->num_uas++;
			if (ct->num_uas > 10) {
				iscsi_set_error(iscsi, "iscsi_testunitready "
						"Too many UnitAttentions "
						"during login.");
				ct->cb(iscsi, SCSI_STATUS_ERROR, NULL,
				       ct->private_data);
				iscsi_free(iscsi, ct);
				scsi_free_scsi_task(task);
				return;
			}
			if (iscsi_testunitready_connect(iscsi, ct->lun,
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

	/* If the application has requested no UA on reconnect OR if this is
	   the initial connection attempt then we need to consume any/all
	   UAs that might be present.
	*/
	if (iscsi->no_ua_on_reconnect || (ct->lun != -1 && !iscsi->old_iscsi)) {
		if (iscsi_testunitready_connect(iscsi, ct->lun,
						iscsi_testunitready_cb,
						ct) == NULL) {
			iscsi_set_error(iscsi, "iscsi_testunitready_async failed.");
			ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
			iscsi_free(iscsi, ct);
		}
	} else {
		ct->cb(iscsi, SCSI_STATUS_GOOD, NULL, ct->private_data);
		iscsi_free(iscsi, ct);
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
		iscsi_set_error(iscsi, "iscsi_login_async failed: %s",
				iscsi_get_error(iscsi));
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
	if (iscsi->portal != portal) {
		strncpy(iscsi->portal, portal, MAX_STRING_SIZE);
	}

	ct = iscsi_malloc(iscsi, sizeof(struct connect_task));
	if (ct == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory. Failed to allocate "
				"connect_task structure.");
		return -ENOMEM;
	}
	ct->cb           = cb;
	ct->lun          = lun;
	ct->num_uas      = 0;
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

/* Set ua reconnect status. The default is that we just reconnect and then
   any/all UAs that are generated by the target will be passed back to the
   application.

   For test applications it can be more convenient to just reconnect
   and have any UAs be consumed and ignored by the library.
*/
void iscsi_set_no_ua_on_reconnect(struct iscsi_context *iscsi, int state)
{
	iscsi->no_ua_on_reconnect = state;
}

void iscsi_set_reconnect_max_retries(struct iscsi_context *iscsi, int count)
{
	iscsi->reconnect_max_retries = count;
}

void iscsi_defer_reconnect(struct iscsi_context *iscsi)
{
	iscsi->reconnect_deferred = 1;

	ISCSI_LOG(iscsi, 2, "reconnect deferred, cancelling all tasks");

	iscsi_cancel_pdus(iscsi);
}

void iscsi_reconnect_cb(struct iscsi_context *iscsi _U_, int status,
                        void *command_data _U_, void *private_data _U_)
{
	struct iscsi_context *old_iscsi;
	int i;

	if (status != SCSI_STATUS_GOOD) {
		int backoff = ++iscsi->old_iscsi->retry_cnt;
		if (backoff > 10) {
			backoff += rand() % 10;
			backoff -= 5;
		}
		if (backoff > 30) {
			backoff = 30;
		}
		if (iscsi->reconnect_max_retries != -1 &&
		    iscsi->old_iscsi->retry_cnt > iscsi->reconnect_max_retries) {
			/* we will exit iscsi_service with -1 the next time we enter it. */
			backoff = 0;
		}
		ISCSI_LOG(iscsi, 1, "reconnect try %d failed, waiting %d seconds", iscsi->old_iscsi->retry_cnt, backoff);
		iscsi->next_reconnect = time(NULL) + backoff;
		iscsi->pending_reconnect = 1;
		return;
	}

	old_iscsi = iscsi->old_iscsi;
	iscsi->old_iscsi = NULL;

	while (old_iscsi->outqueue) {
		struct iscsi_pdu *pdu = old_iscsi->outqueue;
		ISCSI_LIST_REMOVE(&old_iscsi->outqueue, pdu);
		ISCSI_LIST_ADD_END(&old_iscsi->waitpdu, pdu);
	}

	while (old_iscsi->waitpdu) {
		struct iscsi_pdu *pdu = old_iscsi->waitpdu;

		ISCSI_LIST_REMOVE(&old_iscsi->waitpdu, pdu);
		if (pdu->itt == 0xffffffff) {
			iscsi->drv->free_pdu(old_iscsi, pdu);
			continue;
		}

		if (pdu->flags & ISCSI_PDU_DROP_ON_RECONNECT) {
			/*
			 * We only want to re-queue SCSI COMMAND PDUs.
			 * All other PDUs are discarded at this point.
			 * This includes DATA-OUT, NOP and task management.
			 */
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED,
				              NULL, pdu->private_data);
			}
			iscsi->drv->free_pdu(old_iscsi, pdu);
			continue;
		}

		scsi_task_reset_iov(&pdu->scsi_cbdata.task->iovector_in);
		scsi_task_reset_iov(&pdu->scsi_cbdata.task->iovector_out);

		/* We pass NULL as 'd' since any databuffer has already
		 * been converted to a task-> iovector first time this
		 * PDU was sent.
		 */
		if (iscsi_scsi_command_async(iscsi, pdu->lun,
					     pdu->scsi_cbdata.task,
					     pdu->scsi_cbdata.callback,
					     NULL,
					     pdu->scsi_cbdata.private_data)) {
			/* not much we can really do at this point */
		}
		iscsi->drv->free_pdu(old_iscsi, pdu);
	}

	if (old_iscsi->incoming != NULL) {
		iscsi_free_iscsi_in_pdu(old_iscsi, old_iscsi->incoming);
	}

	if (old_iscsi->outqueue_current != NULL && old_iscsi->outqueue_current->flags & ISCSI_PDU_DELETE_WHEN_SENT) {
		iscsi->drv->free_pdu(old_iscsi, old_iscsi->outqueue_current);
	}

	iscsi_free(old_iscsi, old_iscsi->opaque);

	for (i = 0; i < old_iscsi->smalloc_free; i++) {
		iscsi_free(old_iscsi, old_iscsi->smalloc_ptrs[i]);
	}

	iscsi->mallocs += old_iscsi->mallocs;
	iscsi->frees += old_iscsi->frees;

	free(old_iscsi);
	
	/* avoid a reconnect faster than 3 seconds */
	iscsi->next_reconnect = time(NULL) + 3;

	ISCSI_LOG(iscsi, 2, "reconnect was successful");

	iscsi->pending_reconnect = 0;
}

int iscsi_reconnect(struct iscsi_context *iscsi)
{
	struct iscsi_context *tmp_iscsi;

	/* if there is already a deferred reconnect do not try again */
	if (iscsi->reconnect_deferred) {
		ISCSI_LOG(iscsi, 2, "reconnect initiated, but reconnect is already deferred");
		return -1;
	}

	/* This is mainly for tests, where we do not want to automatically
	   reconnect but rather want the commands to fail with an error
	   if the target drops the session.
	 */
	if (iscsi->no_auto_reconnect) {
		iscsi_defer_reconnect(iscsi);
		return 0;
	}

	if (iscsi->old_iscsi && !iscsi->pending_reconnect) {
		return 0;
	}

	if (time(NULL) < iscsi->next_reconnect) {
		iscsi->pending_reconnect = 1;
		return 0;
	}

	if (iscsi->reconnect_max_retries != -1 && iscsi->old_iscsi &&
	    iscsi->old_iscsi->retry_cnt > iscsi->reconnect_max_retries) {
		iscsi_defer_reconnect(iscsi);
		return -1;
	}

	tmp_iscsi = iscsi_create_context(iscsi->initiator_name);
	if (tmp_iscsi == NULL) {
		ISCSI_LOG(iscsi, 2, "failed to create new context for reconnection");
		return -1;
	}

	ISCSI_LOG(iscsi, 2, "reconnect initiated");

	iscsi_set_targetname(tmp_iscsi, iscsi->target_name);

	iscsi_set_header_digest(tmp_iscsi, iscsi->want_header_digest);

	iscsi_set_initiator_username_pwd(tmp_iscsi, iscsi->user, iscsi->passwd);
	iscsi_set_target_username_pwd(tmp_iscsi, iscsi->target_user, iscsi->target_passwd);

	iscsi_set_session_type(tmp_iscsi, ISCSI_SESSION_NORMAL);

	tmp_iscsi->lun = iscsi->lun;

	strncpy(tmp_iscsi->portal, iscsi->portal, MAX_STRING_SIZE);
	
	strncpy(tmp_iscsi->bind_interfaces, iscsi->bind_interfaces, MAX_STRING_SIZE);
	tmp_iscsi->bind_interfaces_cnt = iscsi->bind_interfaces_cnt;
	
	tmp_iscsi->log_level = iscsi->log_level;
	tmp_iscsi->log_fn = iscsi->log_fn;
	tmp_iscsi->tcp_user_timeout = iscsi->tcp_user_timeout;
	tmp_iscsi->tcp_keepidle = iscsi->tcp_keepidle;
	tmp_iscsi->tcp_keepcnt = iscsi->tcp_keepcnt;
	tmp_iscsi->tcp_keepintvl = iscsi->tcp_keepintvl;
	tmp_iscsi->tcp_syncnt = iscsi->tcp_syncnt;
	tmp_iscsi->cache_allocations = iscsi->cache_allocations;
	tmp_iscsi->scsi_timeout = iscsi->scsi_timeout;
	tmp_iscsi->no_ua_on_reconnect = iscsi->no_ua_on_reconnect;

	tmp_iscsi->reconnect_max_retries = iscsi->reconnect_max_retries;

	if (iscsi->old_iscsi) {
		int i;
		for (i = 0; i < iscsi->smalloc_free; i++) {
			iscsi_free(iscsi, iscsi->smalloc_ptrs[i]);
		}
		tmp_iscsi->old_iscsi = iscsi->old_iscsi;
	} else {
		tmp_iscsi->old_iscsi = malloc(sizeof(struct iscsi_context));
		memcpy(tmp_iscsi->old_iscsi, iscsi, sizeof(struct iscsi_context));
	}
	memcpy(iscsi, tmp_iscsi, sizeof(struct iscsi_context));
	free(tmp_iscsi);

	return iscsi_full_connect_async(iscsi, iscsi->portal,
	                                iscsi->lun, iscsi_reconnect_cb, NULL);
}
