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
#else
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
				 ISCSI_PDU_TEXT_RESPONSE);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
				"text pdu.");
		return -1;
	}

	/* immediate */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_TEXT_FINAL);

	/* target transfer tag */
	iscsi_pdu_set_ttt(pdu, 0xffffffff);

	/* sendtargets */
	str = (char *)"SendTargets=All";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"text pdu.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

static void
iscsi_free_discovery_addresses(struct iscsi_discovery_address *addresses)
{
	while (addresses != NULL) {
		struct iscsi_discovery_address *next = addresses->next;

		free(discard_const(addresses->target_name));
		addresses->target_name = NULL;

		free(discard_const(addresses->target_address));
		addresses->target_address = NULL;

		addresses->next = NULL;
		free(addresses);
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
		pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			      pdu->private_data);
		return -1;
	}

	while (size > 0) {
		int len;

		len = strlen((char *)ptr);

		if (len == 0) {
			break;
		}

		if (len > size) {
			iscsi_set_error(iscsi, "len > size when parsing "
					"discovery data %d>%d", len, size);
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				      pdu->private_data);
			iscsi_free_discovery_addresses(targets);
			return -1;
		}

		/* parse the strings */
		if (!strncmp((char *)ptr, "TargetName=", 11)) {
			struct iscsi_discovery_address *target;

			target = malloc(sizeof(struct iscsi_discovery_address));
			if (target == NULL) {
				iscsi_set_error(iscsi, "Failed to allocate "
						"data for new discovered "
						"target");
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					      pdu->private_data);
				iscsi_free_discovery_addresses(targets);
				return -1;
			}
			memset(target, 0, sizeof(struct iscsi_discovery_address));
			target->target_name = strdup((char *)ptr+11);
			if (target->target_name == NULL) {
				iscsi_set_error(iscsi, "Failed to allocate "
						"data for new discovered "
						"target name");
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					      pdu->private_data);
				free(target);
				target = NULL;
				iscsi_free_discovery_addresses(targets);
				return -1;
			}
			target->next = targets;
			targets = target;
		} else if (!strncmp((char *)ptr, "TargetAddress=", 14)) {
			targets->target_address = strdup((char *)ptr+14);
			if (targets->target_address == NULL) {
				iscsi_set_error(iscsi, "Failed to allocate "
						"data for new discovered "
						"target address");
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					      pdu->private_data);
				iscsi_free_discovery_addresses(targets);
				return -1;
			}
		} else {
			iscsi_set_error(iscsi, "Dont know how to handle "
					"discovery string : %s", ptr);
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				      pdu->private_data);
			iscsi_free_discovery_addresses(targets);
			return -1;
		}

		ptr  += len + 1;
		size -= len + 1;
	}

	pdu->callback(iscsi, SCSI_STATUS_GOOD, targets, pdu->private_data);
	iscsi_free_discovery_addresses(targets);

	return 0;
}
