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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef AROS
#include "aros/aros_compat.h"
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include "win32/win32_compat.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

struct iscsi_sync_state {
        int finished;
        int status;
        void *ptr;
        struct scsi_task *task;
};

static void
event_loop(struct iscsi_context *iscsi, struct iscsi_sync_state *state)
{
        struct pollfd pfd;
	int ret;

	while (state->finished == 0) {
		short revents;

		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if ((ret = poll(&pfd, 1, 1000)) < 0) {
			iscsi_set_error(iscsi, "Poll failed");
			state->status = -1;
			return;
		}
		revents = (ret == 0) ? 0 : pfd.revents;
		if (iscsi_service(iscsi, revents) < 0) {
			iscsi_set_error(iscsi,
				"iscsi_service failed with : %s",
				iscsi_get_error(iscsi));
			state->status = -1;
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

	memset(&state, 0, sizeof(state));

	if (iscsi_connect_async(iscsi, portal,
				iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to start connect() %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, &state);

	/* clear connect_data so it doesnt point to our stack */
	iscsi->connect_data = NULL;

	/* in case of error, cancel any pending pdus */
	if (state.status != SCSI_STATUS_GOOD) {
		iscsi_cancel_pdus(iscsi);
	}

	return (state.status == SCSI_STATUS_GOOD) ? 0 : -1;
}

int
iscsi_full_connect_sync(struct iscsi_context *iscsi,
			const char *portal, int lun)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_full_connect_async(iscsi, portal, lun,
				     iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi,
				"Failed to start full connect %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, &state);

	/* in case of error, cancel any pending pdus */
	if (state.status != SCSI_STATUS_GOOD) {
		iscsi_cancel_pdus(iscsi);
	}

	return (state.status == SCSI_STATUS_GOOD) ? 0 : -1;
}

int iscsi_login_sync(struct iscsi_context *iscsi)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_login_async(iscsi, iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to login. %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, &state);

	return (state.status == SCSI_STATUS_GOOD) ? 0 : -1;
}

int iscsi_logout_sync(struct iscsi_context *iscsi)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_logout_async(iscsi, iscsi_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to start logout() %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, &state);

	return (state.status == SCSI_STATUS_GOOD) ? 0 : -1;
}

static void
reconnect_event_loop(struct iscsi_context *iscsi, struct iscsi_sync_state *state)
{
	struct pollfd pfd;
	int ret;
	while (iscsi->old_iscsi) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if (!pfd.events) {
			sleep(1);
			continue;
		}

		if ((ret = poll(&pfd, 1, 1000)) < 0) {
			iscsi_set_error(iscsi, "Poll failed");
			state->status = -1;
			return;
		}

		if (iscsi_service(iscsi, pfd.revents) < 0) {
			iscsi_set_error(iscsi,
				"iscsi_service failed with : %s",
				iscsi_get_error(iscsi));
			state->status = -1;
			return;
		}
	}
	state->status = 0;
}

int iscsi_reconnect_sync(struct iscsi_context *iscsi)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_reconnect(iscsi) != 0) {
		iscsi_set_error(iscsi, "Failed to reconnect. %s", iscsi_get_error(iscsi));
		return -1;
	}

	reconnect_event_loop(iscsi, &state);

	return (state.status == SCSI_STATUS_GOOD) ? 0 : -1;
}

static void
iscsi_task_mgmt_sync_cb(struct iscsi_context *iscsi, int status,
	      void *command_data, void *private_data)
{
	struct iscsi_sync_state *state = private_data;

	state->status   = status;
	state->finished = 1;

	/* The task mgmt command might have completed successfully
	 * but the target might have responded with
	 * "command not implemented" or something.
	 */
	if (command_data && *(uint32_t *)command_data) {
		switch (*(uint32_t *)command_data) {
		case 1: iscsi_set_error(iscsi, "TASK MGMT responded Task Does Not Exist");
			break;
		case 2: iscsi_set_error(iscsi, "TASK MGMT responded LUN Does Not Exist");
			break;
		case 3: iscsi_set_error(iscsi, "TASK MGMT responded Task Still Allegiant");
			break;
		case 4: iscsi_set_error(iscsi, "TASK MGMT responded Task Allegiance Reassignment Not Supported");
			break;
		case 5: iscsi_set_error(iscsi, "TASK MGMT responded Task Mgmt Function Not Supported");
			break;
		case 6: iscsi_set_error(iscsi, "TASK MGMT responded Function Authorization Failed");
			break;
		case 255: iscsi_set_error(iscsi, "TASK MGMT responded Function Rejected");
			break;
		}

                state->status = SCSI_STATUS_ERROR;
	}
}

int
iscsi_task_mgmt_sync(struct iscsi_context *iscsi,
		     int lun, enum iscsi_task_mgmt_funcs function, 
		     uint32_t ritt, uint32_t rcmdsn)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_task_mgmt_async(iscsi, lun, function,
				  ritt, rcmdsn,
				  iscsi_task_mgmt_sync_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to send TASK MGMT function: %s",
				iscsi_get_error(iscsi));
		return -1;
	}

	event_loop(iscsi, &state);

	return (state.status == SCSI_STATUS_GOOD) ? 0 : -1;
}

int
iscsi_task_mgmt_abort_task_sync(struct iscsi_context *iscsi,
				struct scsi_task *task)
{
	return iscsi_task_mgmt_sync(iscsi, task->lun,
				    ISCSI_TM_ABORT_TASK,
				    task->itt, task->cmdsn);
}

int
iscsi_task_mgmt_abort_task_set_sync(struct iscsi_context *iscsi,
				    uint32_t lun)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_sync(iscsi, lun,
				    ISCSI_TM_ABORT_TASK_SET,
				    0xffffffff, 0);
}

int
iscsi_task_mgmt_lun_reset_sync(struct iscsi_context *iscsi,
			       uint32_t lun)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_sync(iscsi, lun,
				    ISCSI_TM_LUN_RESET,
				    0xffffffff, 0);
}

int
iscsi_task_mgmt_target_warm_reset_sync(struct iscsi_context *iscsi)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_sync(iscsi, 0,
				    ISCSI_TM_TARGET_WARM_RESET,
				    0xffffffff, 0);
}


int
iscsi_task_mgmt_target_cold_reset_sync(struct iscsi_context *iscsi)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_sync(iscsi, 0,
				    ISCSI_TM_TARGET_COLD_RESET,
				    0xffffffff, 0);
}


/*
 * Synchronous SCSI commands
 */
static void
scsi_sync_cb(struct iscsi_context *iscsi _U_, int status, void *command_data,
	     void *private_data)
{
	struct scsi_task *task = command_data;
	struct iscsi_sync_state *state = private_data;

	task->status    = status;

	state->status   = status;
	state->finished = 1;
	state->task     = task;
}

struct scsi_task *
iscsi_reportluns_sync(struct iscsi_context *iscsi, int report_type,
		      int alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_reportluns_task(iscsi, report_type, alloc_len,
				   scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send ReportLuns command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}


struct scsi_task *
iscsi_testunitready_sync(struct iscsi_context *iscsi, int lun)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_testunitready_task(iscsi, lun,
				      scsi_sync_cb, &state) == NULL) {
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
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_inquiry_task(iscsi, lun, evpd, page_code, maxsize,
				scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send Inquiry command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read6_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read6_task(iscsi, lun, lba, datalen, blocksize,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read6 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read6_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		     uint32_t datalen, int blocksize, struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read6_iov_task(iscsi, lun, lba, datalen, blocksize,
				 scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read6 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read10_task(iscsi, lun, lba, datalen, blocksize, rdprotect, 
			      dpo, fua, fua_nv, group_number,
			      scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      uint32_t datalen, int blocksize,
		      int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		      struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read10_iov_task(iscsi, lun, lba, datalen, blocksize, rdprotect,
				  dpo, fua, fua_nv, group_number,
				  scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read12_task(iscsi, lun, lba, datalen, blocksize, rdprotect, 
			      dpo, fua, fua_nv, group_number,
			      scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read12_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      uint32_t datalen, int blocksize,
		      int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		      struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read12_iov_task(iscsi, lun, lba, datalen, blocksize, rdprotect,
				  dpo, fua, fua_nv, group_number,
				  scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		  uint32_t datalen, int blocksize,
		  int rdprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read16_task(iscsi, lun, lba, datalen, blocksize, rdprotect, 
			      dpo, fua, fua_nv, group_number,
			      scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_read16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		      uint32_t datalen, int blocksize,
		      int rdprotect, int dpo, int fua, int fua_nv, int group_number,
		      struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_read16_iov_task(iscsi, lun, lba, datalen, blocksize, rdprotect,
				  dpo, fua, fua_nv, group_number,
				  scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Read16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_readcapacity10_sync(struct iscsi_context *iscsi, int lun, int lba,
			  int pmi)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_readcapacity10_task(iscsi, lun, lba, pmi,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send ReadCapacity10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_readcapacity16_sync(struct iscsi_context *iscsi, int lun)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_readcapacity16_task(iscsi, lun,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send ReadCapacity16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_readdefectdata10_sync(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format, uint16_t alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_readdefectdata10_task(iscsi, lun,
                                        req_plist, req_glist,
                                        defect_list_format, alloc_len,
                                        scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send ReadDefectData10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_readdefectdata12_sync(struct iscsi_context *iscsi, int lun,
                            int req_plist, int req_glist,
                            int defect_list_format,
                            uint32_t address_descriptor_index,
                            uint32_t alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_readdefectdata12_task(iscsi, lun,
                                        req_plist, req_glist,
                                        defect_list_format,
                                        address_descriptor_index, alloc_len,
                                        scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send ReadDefectData12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_sanitize_sync(struct iscsi_context *iscsi, int lun,
		    int immed, int ause, int sa, int param_len,
		    struct iscsi_data *data)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_sanitize_task(iscsi, lun,
				immed, ause, sa, param_len, data,
				scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Sanitize command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_sanitize_block_erase_sync(struct iscsi_context *iscsi, int lun,
				int immed, int ause)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_sanitize_block_erase_task(iscsi, lun,
				immed, ause,
				scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Sanitize command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_sanitize_crypto_erase_sync(struct iscsi_context *iscsi, int lun,
				int immed, int ause)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_sanitize_crypto_erase_task(iscsi, lun,
				immed, ause,
				scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Sanitize command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_sanitize_exit_failure_mode_sync(struct iscsi_context *iscsi, int lun,
				      int immed, int ause)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_sanitize_exit_failure_mode_task(iscsi, lun,
				immed, ause,
				scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Sanitize command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_get_lba_status_sync(struct iscsi_context *iscsi, int lun, uint64_t starting_lba, uint32_t alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_get_lba_status_task(iscsi, lun, starting_lba, alloc_len,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send GetLbaStatus command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_synchronizecache10_sync(struct iscsi_context *iscsi, int lun, int lba,
			      int num_blocks, int syncnv, int immed)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_synchronizecache10_task(iscsi, lun, lba, num_blocks,
					   syncnv, immed,
					   scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send SynchronizeCache10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_startstopunit_sync(struct iscsi_context *iscsi, int lun,
			 int immed, int pcm, int pc,
			 int no_flush, int loej, int start)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_startstopunit_task(iscsi, lun, immed, pcm, pc,
				     no_flush, loej, start,
				     scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send StartStopUnit command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_preventallow_sync(struct iscsi_context *iscsi, int lun,
			int prevent)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_preventallow_task(iscsi, lun, prevent,
				    scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send PreventAllowMediumRemoval command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_synchronizecache16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			      uint32_t num_blocks, int syncnv, int immed)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_synchronizecache16_task(iscsi, lun, lba, num_blocks,
					   syncnv, immed,
					   scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send SynchronizeCache16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_prefetch10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		      int num_blocks, int immed, int group)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_prefetch10_task(iscsi, lun, lba, num_blocks,
				  immed, group,
				  scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send PreFetch10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_prefetch16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		      int num_blocks, int immed, int group)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_prefetch16_task(iscsi, lun, lba, num_blocks,
				  immed, group,
				  scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send PreFetch16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_write10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_write10_task(iscsi, lun, lba, data, datalen, blocksize,
			       wrprotect, dpo, fua, fua_nv, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Write10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_write10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_write10_iov_task(iscsi, lun, lba, data, datalen, blocksize,
				   wrprotect, dpo, fua, fua_nv, group_number,
				   scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Write10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_write12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_write12_task(iscsi, lun, lba, 
			       data, datalen, blocksize, wrprotect, 
			       dpo, fua, fua_nv, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Write12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_write12_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov,int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_write12_iov_task(iscsi, lun, lba,
				   data, datalen, blocksize, wrprotect,
				   dpo, fua, fua_nv, group_number,
				   scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Write12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_write16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_write16_task(iscsi, lun, lba,
			       data, datalen, blocksize, wrprotect, 
			       dpo, fua, fua_nv, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Write16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_write16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_write16_iov_task(iscsi, lun, lba,
				   data, datalen, blocksize, wrprotect,
				   dpo, fua, fua_nv, group_number,
				   scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Write16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeatomic16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			 unsigned char *data, uint32_t datalen, int blocksize,
			 int wrprotect, int dpo, int fua, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeatomic16_task(iscsi, lun, lba,
				     data, datalen, blocksize, wrprotect,
				     dpo, fua, group_number,
				     scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WriteAtomic16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeatomic16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int fua, int group_number,
			     struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeatomic16_iov_task(iscsi, lun, lba,
					 data, datalen, blocksize, wrprotect,
					 dpo, fua, group_number,
					 scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WriteAtomic16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_orwrite_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_orwrite_task(iscsi, lun, lba,
			       data, datalen, blocksize, wrprotect, 
			       dpo, fua, fua_nv, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Orwrite command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_orwrite_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen, int blocksize,
		       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
		       struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_orwrite_iov_task(iscsi, lun, lba,
				   data, datalen, blocksize, wrprotect,
				   dpo, fua, fua_nv, group_number,
				   scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Orwrite command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_compareandwrite_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_compareandwrite_task(iscsi, lun, lba,
			       data, datalen, blocksize, wrprotect, 
			       dpo, fua, fua_nv, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send CompareAndWrite command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_compareandwrite_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			       unsigned char *data, uint32_t datalen, int blocksize,
			       int wrprotect, int dpo, int fua, int fua_nv, int group_number,
			       struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_compareandwrite_iov_task(iscsi, lun, lba,
					   data, datalen, blocksize, wrprotect,
					   dpo, fua, fua_nv, group_number,
					   scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send CompareAndWrite command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeverify10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeverify10_task(iscsi, lun, lba, data, datalen, blocksize,
			       wrprotect, dpo, bytchk, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Writeverify10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeverify10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeverify10_iov_task(iscsi, lun, lba, data, datalen, blocksize,
					 wrprotect, dpo, bytchk, group_number,
					 scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Writeverify10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeverify12_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeverify12_task(iscsi, lun, lba, 
			       data, datalen, blocksize, wrprotect, 
			       dpo, bytchk, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Writeverify12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeverify12_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			 unsigned char *data, uint32_t datalen, int blocksize,
			 int wrprotect, int dpo, int bytchk, int group_number,
			 struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeverify12_iov_task(iscsi, lun, lba,
					 data, datalen, blocksize, wrprotect,
					 dpo, bytchk, group_number,
					 scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Writeverify12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeverify16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		   unsigned char *data, uint32_t datalen, int blocksize,
		   int wrprotect, int dpo, int bytchk, int group_number)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeverify16_task(iscsi, lun, lba,
			       data, datalen, blocksize, wrprotect, 
			       dpo, bytchk, group_number,
			       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Writeverify16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writeverify16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			     unsigned char *data, uint32_t datalen, int blocksize,
			     int wrprotect, int dpo, int bytchk, int group_number,
			     struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writeverify16_iov_task(iscsi, lun, lba,
					 data, datalen, blocksize, wrprotect,
					 dpo, bytchk, group_number,
					 scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Writeverify16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_verify10_sync(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk, int blocksize)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_verify10_task(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Verify10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_verify10_iov_sync(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba,
			int vprotect, int dpo, int bytchk, int blocksize, struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_verify10_iov_task(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize,
				    scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Verify10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}


struct scsi_task *
iscsi_verify12_sync(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba,
		    int vprotect, int dpo, int bytchk, int blocksize)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_verify12_task(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Verify12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_verify12_iov_sync(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba,
			int vprotect, int dpo, int bytchk, int blocksize, struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_verify12_iov_task(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize,
				    scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Verify12 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_verify16_sync(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba,
		    int vprotect, int dpo, int bytchk, int blocksize)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_verify16_task(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Verify16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_verify16_iov_sync(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba,
			int vprotect, int dpo, int bytchk, int blocksize, struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_verify16_iov_task(iscsi, lun, data, datalen, lba, vprotect, dpo, bytchk, blocksize,
				    scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send Verify16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writesame10_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint16_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writesame10_task(iscsi, lun, lba,
				   data, datalen, num_blocks,
				   anchor, unmap, wrprotect, group,
				   scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WRITESAME10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writesame10_iov_sync(struct iscsi_context *iscsi, int lun, uint32_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint16_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writesame10_iov_task(iscsi, lun, lba,
				       data, datalen, num_blocks,
				       anchor, unmap, wrprotect, group,
				       scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WRITESAME10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writesame16_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
		       unsigned char *data, uint32_t datalen,
		       uint32_t num_blocks,
		       int anchor, int unmap, int wrprotect, int group)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writesame16_task(iscsi, lun, lba,
				   data, datalen, num_blocks,
				   anchor, unmap, wrprotect, group,
				   scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WRITESAME16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writesame16_iov_sync(struct iscsi_context *iscsi, int lun, uint64_t lba,
			   unsigned char *data, uint32_t datalen,
			   uint32_t num_blocks,
			   int anchor, int unmap, int wrprotect, int group,
			   struct scsi_iovec *iov, int niov)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writesame16_iov_task(iscsi, lun, lba,
				       data, datalen, num_blocks,
				       anchor, unmap, wrprotect, group,
				       scsi_sync_cb, &state, iov, niov) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WRITESAME16 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_persistent_reserve_in_sync(struct iscsi_context *iscsi, int lun,
				 int sa, uint16_t xferlen)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_persistent_reserve_in_task(iscsi, lun, sa, xferlen,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send PERSISTENT_RESERVE_IN command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_persistent_reserve_out_sync(struct iscsi_context *iscsi, int lun,
				  int sa, int scope, int type, void *param)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_persistent_reserve_out_task(iscsi, lun,
					      sa, scope, type, param,
					      scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send PERSISTENT_RESERVE_OUT command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_unmap_sync(struct iscsi_context *iscsi, int lun, int anchor, int group,
		 struct unmap_list *list, int list_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_unmap_task(iscsi, lun, anchor, group, list, list_len,
				       scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send UNMAP command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_readtoc_sync(struct iscsi_context *iscsi, int lun, int msf, int format, 
		   int track_session, int maxsize)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_readtoc_task(iscsi, lun, msf, format, track_session, 
			       maxsize, scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send Read TOC command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_reserve6_sync(struct iscsi_context *iscsi, int lun)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_reserve6_task(iscsi, lun, scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send RESERVE6 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_release6_sync(struct iscsi_context *iscsi, int lun)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_release6_task(iscsi, lun, scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send RELEASE6 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_report_supported_opcodes_sync(struct iscsi_context *iscsi, int lun, 
				    int rctd, int options,
				    int opcode, int sa,
				    uint32_t alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_report_supported_opcodes_task(iscsi, lun, rctd, options, opcode, sa, alloc_len, scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send MaintenanceIn:"
				"Report Supported Opcodes command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_receive_copy_results_sync(struct iscsi_context *iscsi, int lun,
				int sa, int list_id, int alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_receive_copy_results_task(iscsi, lun, sa, list_id, alloc_len,
					    scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send RECEIVE COPY RESULTS"
				" command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_extended_copy_sync(struct iscsi_context *iscsi, int lun,
			 struct iscsi_data *param_data)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_extended_copy_task(iscsi, lun, param_data,
					scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send EXTENDED COPY"
				" command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_scsi_command_sync(struct iscsi_context *iscsi, int lun,
			struct scsi_task *task, struct iscsi_data *data)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_scsi_command_async(iscsi, lun, task,
				     scsi_sync_cb, data, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to send SCSI command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}


struct scsi_task *
iscsi_modeselect6_sync(struct iscsi_context *iscsi, int lun,
		       int pf, int sp, struct scsi_mode_page *mp)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_modeselect6_task(iscsi, lun, pf, sp, mp,
				  scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send MODE_SELECT6 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_modeselect10_sync(struct iscsi_context *iscsi, int lun,
			int pf, int sp, struct scsi_mode_page *mp)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_modeselect10_task(iscsi, lun, pf, sp, mp,
				  scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send MODE_SELECT10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_modesense6_sync(struct iscsi_context *iscsi, int lun, int dbd,
		      int pc, int page_code, int sub_page_code,
		      unsigned char alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_modesense6_task(iscsi, lun, dbd, pc, page_code, sub_page_code, alloc_len,
				  scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send MODE_SENSE6 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_modesense10_sync(struct iscsi_context *iscsi, int lun, int llbaa, int dbd,
		      int pc, int page_code, int sub_page_code,
		      unsigned char alloc_len)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_modesense10_task(iscsi, lun, llbaa, dbd, pc,
				   page_code, sub_page_code, alloc_len,
				   scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send MODE_SENSE10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

void iscsi_free_discovery_data(struct iscsi_context *iscsi _U_,
                               struct iscsi_discovery_address *da)
{
        while (da) {
                struct iscsi_discovery_address *danext = da->next;

                while (da->portals) {
                        struct iscsi_target_portal *ponext = da->portals->next;
                        free(discard_const(da->portals->portal));
                        free(da->portals);
                        da->portals = ponext;
                }
                free(discard_const(da->target_name));
                free(da);
                da = danext;
        }
}

static void
iscsi_discovery_cb(struct iscsi_context *iscsi _U_, int status,
	      void *command_data, void *private_data)
{
	struct iscsi_sync_state *state = private_data;
        struct iscsi_discovery_address *da;
        struct iscsi_discovery_address *dahead = NULL;
        struct iscsi_target_portal *po;

        for (da = command_data; da != NULL; da = da->next) {
                struct iscsi_discovery_address *datmp;

                datmp = malloc(sizeof(struct iscsi_discovery_address));
                memset(datmp, 0, sizeof(struct iscsi_discovery_address));
                datmp->target_name = strdup(da->target_name);
                datmp->next = dahead;
                dahead = datmp;

                for (po = da->portals; po != NULL; po = po->next) {
                        struct iscsi_target_portal *potmp;

                        potmp = malloc(sizeof(struct iscsi_target_portal));
                        memset(potmp, 0, sizeof(struct iscsi_target_portal));
                        potmp->portal = strdup(po->portal);

                        potmp->next = dahead->portals;
                        dahead->portals = potmp;
                }
        }

	state->status    = status;
	state->finished = 1;
	state->ptr = dahead;
}

struct iscsi_discovery_address *
iscsi_discovery_sync(struct iscsi_context *iscsi)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_discovery_async(iscsi, iscsi_discovery_cb, &state) != 0) {
		iscsi_set_error(iscsi, "Failed to run discovery. %s",
				iscsi_get_error(iscsi));
                printf("async discovery call failed\n");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.ptr;
}
