/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

struct scsi_sync_state {
       int finished;
       struct scsi_task *task;
};

struct iscsi_sync_state {
       int finished;
       int status;
};

static void
event_loop(struct iscsi_context *iscsi, struct scsi_sync_state *state)
{
	struct pollfd pfd;

	while (state->finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if (poll(&pfd, 1, -1) < 0) {
			iscsi_set_error(iscsi, "Poll failed");
			return;
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			iscsi_set_error(iscsi,
					"iscsi_service failed with : %s",
					iscsi_get_error(iscsi));
			return;
		}
	}
}

/*
 * Synchronous iSCSI commands
 */
static void
iscsi_sync_cb(struct iscsi_context *iscsi _U_, int status,
	      void *command_data _U_, void *private_data)
{
	struct iscsi_sync_state *state = private_data;

	state->status    = status;
	state->finished = 1;
}

int
iscsi_connect_sync(struct iscsi_context *iscsi, const char *portal)
{
	struct iscsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_connect_async(iscsi, portal,
				iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to start connect() %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, (struct scsi_sync_state *)&state);

	return state.status;
}

int
iscsi_full_connect_sync(struct iscsi_context *iscsi,
			const char *portal, int lun)
{
	struct iscsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_full_connect_async(iscsi, portal, lun,
				     iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to start full connect %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, (struct scsi_sync_state *)&state);

	return state.status;
}

int iscsi_login_sync(struct iscsi_context *iscsi)
{
	struct iscsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_login_async(iscsi, iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to login. %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, (struct scsi_sync_state *)&state);

	return state.status;
}

int iscsi_logout_sync(struct iscsi_context *iscsi)
{
	struct iscsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_logout_async(iscsi, iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to start logout() %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, (struct scsi_sync_state *)&state);

	return state.status;
}



/*
 * Synchronous SCSI commands
 */
static void
scsi_sync_cb(struct iscsi_context *iscsi _U_, int status, void *command_data,
	     void *private_data)
{
	struct scsi_task *task = command_data;
	struct scsi_sync_state *state = private_data;

	task->status    = status;
	state->finished = 1;
	state->task     = task;
	iscsi_cbdata_steal_scsi_task(task);
}

struct scsi_task *
iscsi_reportluns_sync(struct iscsi_context *iscsi, int report_type,
		      int alloc_len)
{
	struct scsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_reportluns_async(iscsi, report_type, alloc_len,
				   scsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to send ReportLuns command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}


struct scsi_task *
iscsi_testunitready_sync(struct iscsi_context *iscsi, int lun)
{
	struct scsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_testunitready_async(iscsi, lun,
				      scsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to send TestUnitReady command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_inquiry_sync(struct iscsi_context *iscsi, int lun, int evpd,
		   int page_code, int maxsize)
{
	struct scsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_inquiry_async(iscsi, lun, evpd, page_code, maxsize,
				scsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to send Inquiry command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_readcapacity10_sync(struct iscsi_context *iscsi, int lun, int lba,
			  int pmi)
{
	struct scsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_readcapacity10_async(iscsi, lun, lba, pmi,
				       scsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to send ReadCapacity10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_synchronizecache10_sync(struct iscsi_context *iscsi, int lun, int lba,
			      int num_blocks, int syncnv, int immed)
{
	struct scsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_synchronizecache10_async(iscsi, lun, lba, num_blocks,
					   syncnv, immed,
					   scsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to send SynchronizeCache10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_scsi_command_sync(struct iscsi_context *iscsi, int lun,
			struct scsi_task *task, struct iscsi_data *data)
{
	struct scsi_sync_state state;

	bzero(&state, sizeof(state));

	if (iscsi_scsi_command_async(iscsi, lun, task,
				     scsi_sync_cb, data, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to send SCSI command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}


