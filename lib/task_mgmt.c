/*
   Copyright (C) 2011 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

int
iscsi_task_mgmt_async(struct iscsi_context *iscsi,
		      int lun, enum iscsi_task_mgmt_funcs function, 
		      uint32_t ritt, uint32_t rcmdsn,
		      iscsi_command_cb cb, void *private_data)
{
	struct iscsi_pdu *pdu;

	if (iscsi->is_loggedin == 0) {
		iscsi_set_error(iscsi, "trying to send task-mgmt while not "
				"logged in");
		return -1;
	}

	pdu = iscsi_allocate_pdu(iscsi,
				 ISCSI_PDU_SCSI_TASK_MANAGEMENT_REQUEST,
				 ISCSI_PDU_SCSI_TASK_MANAGEMENT_RESPONSE,
				 iscsi_itt_post_increment(iscsi),
				 ISCSI_PDU_DROP_ON_RECONNECT);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Failed to allocate task mgmt pdu");
		return -1;
	}

	/* immediate flag */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, 0x80 | function);

	/* lun */
	iscsi_pdu_set_lun(pdu, lun);

	/* ritt */
	iscsi_pdu_set_ritt(pdu, ritt);

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);

	/* rcmdsn */
	iscsi_pdu_set_rcmdsn(pdu, rcmdsn);

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "failed to queue iscsi taskmgmt pdu");
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

int
iscsi_process_task_mgmt_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			    struct iscsi_in_pdu *in)
{
	uint32_t response = in->hdr[2];

	if (pdu->callback) {
		pdu->callback(iscsi, SCSI_STATUS_GOOD, &response, pdu->private_data);
	}
	return 0;
}

int
iscsi_task_mgmt_abort_task_async(struct iscsi_context *iscsi,
		      struct scsi_task *task,
		      iscsi_command_cb cb, void *private_data)
{
	return iscsi_task_mgmt_async(iscsi,
		      task->lun, ISCSI_TM_ABORT_TASK,
		      task->itt, task->cmdsn,
		      cb, private_data);
}

int
iscsi_task_mgmt_abort_task_set_async(struct iscsi_context *iscsi,
		      uint32_t lun,
		      iscsi_command_cb cb, void *private_data)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_async(iscsi,
		      lun, ISCSI_TM_ABORT_TASK_SET,
		      0xffffffff, 0,
		      cb, private_data);
}

int
iscsi_task_mgmt_lun_reset_async(struct iscsi_context *iscsi,
		      uint32_t lun,
		      iscsi_command_cb cb, void *private_data)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_async(iscsi,
		      lun, ISCSI_TM_LUN_RESET,
		      0xffffffff, 0,
		      cb, private_data);
}

int
iscsi_task_mgmt_target_warm_reset_async(struct iscsi_context *iscsi,
		      iscsi_command_cb cb, void *private_data)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_async(iscsi,
		      0, ISCSI_TM_TARGET_WARM_RESET,
		      0xffffffff, 0,
		      cb, private_data);
}


int
iscsi_task_mgmt_target_cold_reset_async(struct iscsi_context *iscsi,
		      iscsi_command_cb cb, void *private_data)
{
	iscsi_scsi_cancel_all_tasks(iscsi);

	return iscsi_task_mgmt_async(iscsi,
		      0, ISCSI_TM_TARGET_COLD_RESET,
		      0xffffffff, 0,
		      cb, private_data);
}


