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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"

int
iscsi_discovery_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		      void *private_data)
{
	struct iscsi_pdu *pdu;
	char *str;

	if (iscsi->session_type != ISCSI_SESSION_DISCOVERY) {
		iscsi_set_error(iscsi, "Trying to do discovery on "
				"non-discovery session.");
		return -1;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_TEXT_REQUEST,
				 ISCSI_PDU_TEXT_RESPONSE,
				 iscsi_itt_post_increment(iscsi),
				 ISCSI_PDU_DROP_ON_RECONNECT);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
				"text pdu.");
		return -1;
	}

	/* immediate */
	iscsi_pdu_set_immediate(pdu);

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_TEXT_FINAL);

	/* target transfer tag */
	iscsi_pdu_set_ttt(pdu, 0xffffffff);

	/* sendtargets */
	str = (char *)"SendTargets=All";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"text pdu.");
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

static void
iscsi_free_discovery_addresses(struct iscsi_context *iscsi, struct iscsi_discovery_address *addresses)
{
	while (addresses != NULL) {
		struct iscsi_discovery_address *next = addresses->next;

		iscsi_free(iscsi, discard_const(addresses->target_name));
		addresses->target_name = NULL;

		while (addresses->portals != NULL) {
			struct iscsi_target_portal *next_portal = addresses->portals->next;

			iscsi_free(iscsi, discard_const(addresses->portals->portal));
			iscsi_free(iscsi, discard_const(addresses->portals));

			addresses->portals = next_portal;
		}
		addresses->portals = NULL;

		addresses->next = NULL;
		iscsi_free(iscsi, addresses);
		addresses = next;
	}
}

int
iscsi_process_text_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			 struct iscsi_in_pdu *in)
{
	struct iscsi_discovery_address *targets = NULL;
	unsigned char *ptr = in->data;
	int size = in->data_pos;

	/* verify the response looks sane */
	if (in->hdr[1] != ISCSI_PDU_TEXT_FINAL) {
		iscsi_set_error(iscsi, "unsupported flags in text "
				"reply %02x", in->hdr[1]);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			              pdu->private_data);
		}
		return -1;
	}

	while (size > 0) {
		unsigned char *end;
		int len;

		end = memchr(ptr, 0, size);
		if (end == NULL) {
			iscsi_set_error(iscsi, "NUL not found after offset %ld "
					"when parsing discovery data",
					(long)(ptr - in->data));
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				              pdu->private_data);
			}
			iscsi_free_discovery_addresses(iscsi, targets);
			return -1;
		}

		len = end - ptr;
		if (len == 0) {
			break;
		}

		/* parse the strings */
		if (!strncmp((char *)ptr, "TargetName=", 11)) {
			struct iscsi_discovery_address *target;

			target = iscsi_zmalloc(iscsi, sizeof(struct iscsi_discovery_address));
			if (target == NULL) {
				iscsi_set_error(iscsi, "Failed to allocate "
						"data for new discovered "
						"target");
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				iscsi_free_discovery_addresses(iscsi, targets);
				return -1;
			}
			target->target_name = iscsi_strdup(iscsi,(char *)ptr+11);
			if (target->target_name == NULL) {
				iscsi_set_error(iscsi, "Failed to allocate "
						"data for new discovered "
						"target name");
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				iscsi_free(iscsi, target);
				target = NULL;
				iscsi_free_discovery_addresses(iscsi, targets);
				return -1;
			}
			target->next = targets;
			targets = target;
		} else if (!strncmp((char *)ptr, "TargetAddress=", 14)) {
			struct iscsi_target_portal *portal;

			if (targets == NULL) {
				iscsi_set_error(iscsi, "Invalid discovery "
						"reply");
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				iscsi_free_discovery_addresses(iscsi, targets);
				return -1;
			}
			portal = iscsi_zmalloc(iscsi, sizeof(struct iscsi_target_portal));
			if (portal == NULL) {
				iscsi_set_error(iscsi, "Failed to malloc "
						"portal structure");
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				iscsi_free_discovery_addresses(iscsi, targets);
				return -1;
			}

			portal->next = targets->portals;
			targets->portals = portal;

			portal->portal = iscsi_strdup(iscsi, (char *)ptr+14);
			if (portal->portal == NULL) {
				iscsi_set_error(iscsi, "Failed to allocate "
						"data for new discovered "
						"target address");
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				iscsi_free_discovery_addresses(iscsi, targets);
				return -1;
			}
		} else {
			iscsi_set_error(iscsi, "Don't know how to handle "
					"discovery string : %s", ptr);
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				              pdu->private_data);
			}
			iscsi_free_discovery_addresses(iscsi, targets);
			return -1;
		}

		ptr  += len + 1;
		size -= len + 1;
	}

	if (pdu->callback) {
		pdu->callback(iscsi, SCSI_STATUS_GOOD, targets, pdu->private_data);
	}
	iscsi_free_discovery_addresses(iscsi, targets);

	return 0;
}
