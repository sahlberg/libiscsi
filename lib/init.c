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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "slist.h"


struct iscsi_context *
iscsi_create_context(const char *initiator_name)
{
	struct iscsi_context *iscsi;

	iscsi = malloc(sizeof(struct iscsi_context));
	if (iscsi == NULL) {
		return NULL;
	}

	bzero(iscsi, sizeof(struct iscsi_context));

	iscsi->initiator_name = strdup(initiator_name);
	if (iscsi->initiator_name == NULL) {
		free(iscsi);
		return NULL;
	}

	iscsi->fd = -1;

	/* initialize to a "random" isid */
	iscsi_set_isid_random(iscsi, getpid() ^ time(NULL));

	return iscsi;
}

int
iscsi_set_isid_random(struct iscsi_context *iscsi, int rnd)
{
	iscsi->isid[0] = 0x80;
	iscsi->isid[1] = rnd&0xff;
	iscsi->isid[2] = rnd&0xff;
	iscsi->isid[3] = rnd&0xff;
	iscsi->isid[4] = 0;
	iscsi->isid[5] = 0;

	return 0;
}

int
iscsi_set_alias(struct iscsi_context *iscsi, const char *alias)
{
	if (iscsi->is_loggedin != 0) {
		iscsi_set_error(iscsi, "Already logged in when adding alias");
		return -1;
	}

	free(discard_const(iscsi->alias));

	iscsi->alias = strdup(alias);
	if (iscsi->alias == NULL) {
		iscsi_set_error(iscsi, "Failed to allocate alias name");
		return -1;
	}

	return 0;
}

int
iscsi_set_targetname(struct iscsi_context *iscsi, const char *target_name)
{
	if (iscsi->is_loggedin != 0) {
		iscsi_set_error(iscsi, "Already logged in when adding "
				"targetname");
		return -1;
	}

	free(discard_const(iscsi->target_name));

	iscsi->target_name = strdup(target_name);
	if (iscsi->target_name == NULL) {
		iscsi_set_error(iscsi, "Failed to allocate target name");
		return -1;
	}

	return 0;
}

int
iscsi_destroy_context(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;

	if (iscsi == NULL) {
		return 0;
	}

	if (iscsi->fd != -1) {
		iscsi_disconnect(iscsi);
	}

	while ((pdu = iscsi->outqueue)) {
		SLIST_REMOVE(&iscsi->outqueue, pdu);
		pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
			      pdu->private_data);
		iscsi_free_pdu(iscsi, pdu);
	}
	while ((pdu = iscsi->waitpdu)) {
		SLIST_REMOVE(&iscsi->waitpdu, pdu);
		pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
			      pdu->private_data);
		iscsi_free_pdu(iscsi, pdu);
	}

	free(discard_const(iscsi->initiator_name));
	iscsi->initiator_name = NULL;

	free(discard_const(iscsi->target_name));
	iscsi->target_name = NULL;

	free(discard_const(iscsi->alias));
	iscsi->alias = NULL;

	if (iscsi->inbuf != NULL) {
		free(iscsi->inbuf);
		iscsi->inbuf = NULL;
		iscsi->insize = 0;
		iscsi->inpos = 0;
	}

	free(iscsi->error_string);
	iscsi->error_string = NULL;

	free(iscsi);

	return 0;
}



void
iscsi_set_error(struct iscsi_context *iscsi, const char *error_string, ...)
{
	va_list ap;
	char *str;

	va_start(ap, error_string);
	if (vasprintf(&str, error_string, ap) < 0) {
		/* not much we can do here */
		str = NULL;
	}

	free(iscsi->error_string);

	iscsi->error_string = str;
	va_end(ap);
}


const char *
iscsi_get_error(struct iscsi_context *iscsi)
{
	return iscsi->error_string;
}

int
iscsi_set_header_digest(struct iscsi_context *iscsi,
			enum iscsi_header_digest header_digest)
{
	if (iscsi->is_loggedin) {
		iscsi_set_error(iscsi, "trying to set header digest while "
				"logged in");
		return -1;
	}

	iscsi->want_header_digest = header_digest;

	return 0;
}

int
iscsi_is_logged_in(struct iscsi_context *iscsi)
{
	return iscsi->is_loggedin;
}
