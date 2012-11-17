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
#include <winsock2.h>
#include "win32/win32_compat.h"
#else
#include <poll.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

struct iscsi_sync_state {
   int finished;
   int status;
   struct scsi_task *task;
};

static void
event_loop(struct iscsi_context *iscsi, struct iscsi_sync_state *state)
{
	struct pollfd pfd;
	int ret;

	while (state->finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if ((ret = poll(&pfd, 1, 1000)) < 0) {
			iscsi_set_error(iscsi, "Poll failed");
			return;
		}
		if (ret == 0) {
			/* poll timedout, try again */
			continue;
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

	if (state != NULL) {
		state->status    = status;
		state->finished = 1;
	}
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

	return state.status;
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

	return state.status;
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

	return state.status;
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
iscsi_writesame10_sync(struct iscsi_context *iscsi, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint32_t lba, uint16_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writesame10_task(iscsi, lun, data, datalen,
	   			 lba, num_blocks,
	   			 anchor, unmap, pbdata, lbdata,
				 wrprotect, group,
				 scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WRITESAME10 command");
		return NULL;
	}

	event_loop(iscsi, &state);

	return state.task;
}

struct scsi_task *
iscsi_writesame16_sync(struct iscsi_context *iscsi, int lun,
		       unsigned char *data, uint32_t datalen,
		       uint64_t lba, uint32_t num_blocks,
		       int anchor, int unmap, int pbdata, int lbdata,
		       int wrprotect, int group)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_writesame16_task(iscsi, lun, data, datalen,
	   			 lba, num_blocks,
	   			 anchor, unmap, pbdata, lbdata,
				 wrprotect, group,
				 scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi,
				"Failed to send WRITESAME16 command");
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
iscsi_report_supported_opcodes_sync(struct iscsi_context *iscsi, int lun, int return_timeouts, int maxsize)
{
	struct iscsi_sync_state state;

	memset(&state, 0, sizeof(state));

	if (iscsi_report_supported_opcodes_task(iscsi, lun, return_timeouts, maxsize, scsi_sync_cb, &state) == NULL) {
		iscsi_set_error(iscsi, "Failed to send MaintenanceIn:"
				"Report Supported Opcodes command");
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
