/*
   Copyright (C) 2011, 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#define _GNU_SOURCE

/* We need config.h since it affects the size of struct iscsi_context */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <asm/fcntl.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"


int plugin_post_open(struct iscsi_context *iscsi)
{
	struct iscsi_context *second_iscsi;

	iscsi->plugin_data = malloc(sizeof(uint32_t));
	*(uint32_t *)iscsi->plugin_data = 0;

	second_iscsi = iscsi_create_context(iscsi->initiator_name);
	if (second_iscsi == NULL) {
		return -1;
	}
	iscsi_set_targetname(second_iscsi, iscsi->target_name);
	iscsi_set_session_type(second_iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(second_iscsi, iscsi->want_header_digest);
	if (iscsi_full_connect_sync(second_iscsi, iscsi->portal, iscsi->lun) != 0) {
		iscsi_destroy_context(second_iscsi);
		return -1;
	}
	if (iscsi_add_slave_context(iscsi, second_iscsi)) {
		iscsi_destroy_context(second_iscsi);
		return -1;
	}

	return 0;
}

struct iscsi_context *plugin_select_scsi_path(struct iscsi_context *iscsi, struct scsi_task *task)
{
	uint32_t *d = iscsi->plugin_data;

	/* we have not yet intercepted the open */
	if (iscsi->plugin_data == NULL) {
		return iscsi;
	}

	if (++(*d) > 1) {
		*d = 0;
	}

	if (*d == 0) {
		return iscsi;
	} else {
		return iscsi->next_context;
	}
}

int plugin_destroy_context(struct iscsi_context *iscsi)
{
	if (iscsi->plugin_data) {
		free(iscsi->plugin_data);
		iscsi->plugin_data = NULL;
	}
	return 0;
}
