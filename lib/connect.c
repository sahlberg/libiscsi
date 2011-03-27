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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

struct connect_task {
	iscsi_command_cb cb;
	void *private_data;
	int lun;
};

static void
iscsi_testunitready_cb(struct iscsi_context *iscsi, int status,
		       void *command_data, void *private_data)
{
	struct connect_task *ct = private_data;
	struct scsi_task *task = command_data;

	if (status != 0) {
		if (task->sense.key == SCSI_SENSE_UNIT_ATTENTION
		    && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET) {
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
				free(ct);
			}
			scsi_free_scsi_task(task);
			return;
		}
	}

	ct->cb(iscsi, status?SCSI_STATUS_ERROR:SCSI_STATUS_GOOD, NULL,
	       ct->private_data);
	free(ct);
	scsi_free_scsi_task(task);
}

static void
iscsi_login_cb(struct iscsi_context *iscsi, int status, void *command_data _U_,
	       void *private_data)
{
	struct connect_task *ct = private_data;

	if (status != 0) {
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
		free(ct);
		return;
	}

	if (iscsi_testunitready_task(iscsi, ct->lun,
				      iscsi_testunitready_cb, ct) == NULL) {
		iscsi_set_error(iscsi, "iscsi_testunitready_async failed.");
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
		free(ct);
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
		free(ct);
		return;
	}

	if (iscsi_login_async(iscsi, iscsi_login_cb, ct) != 0) {
		iscsi_set_error(iscsi, "iscsi_login_async failed.");
		ct->cb(iscsi, SCSI_STATUS_ERROR, NULL, ct->private_data);
		free(ct);
	}
}


int
iscsi_full_connect_async(struct iscsi_context *iscsi, const char *portal,
			 int lun, iscsi_command_cb cb, void *private_data)
{
	struct connect_task *ct;

	ct = malloc(sizeof(struct connect_task));
	if (ct == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory. Failed to allocate "
				"connect_task structure.");
		return -ENOMEM;
	}
	ct->cb           = cb;
	ct->lun          = lun;
	ct->private_data = private_data;
	if (iscsi_connect_async(iscsi, portal, iscsi_connect_cb, ct) != 0) {
		free(ct);
		return -ENOMEM;
	}
	return 0;
}
