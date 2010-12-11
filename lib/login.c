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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "iscsi.h"
#include "iscsi-private.h"

int
iscsi_login_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		  void *private_data)
{
	struct iscsi_pdu *pdu;
	char *str;

	if (iscsi->is_loggedin != 0) {
		iscsi_set_error(iscsi, "Trying to login while already logged "
				"in.");
		return -1;
	}

	switch (iscsi->session_type) {
	case ISCSI_SESSION_DISCOVERY:
	case ISCSI_SESSION_NORMAL:
		break;
	default:
		iscsi_set_error(iscsi, "trying to login without setting "
				"session type.");
		return -1;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_LOGIN_REQUEST,
				 ISCSI_PDU_LOGIN_RESPONSE);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
				"login pdu.");
		return -1;
	}

	/* login request */
	iscsi_pdu_set_immediate(pdu);

	/* flags */
	iscsi_pdu_set_pduflags(pdu, ISCSI_PDU_LOGIN_TRANSIT
					| ISCSI_PDU_LOGIN_CSG_OPNEG
					| ISCSI_PDU_LOGIN_NSG_FF);


	/* initiator name */
	if (iscsi_pdu_add_data(iscsi, pdu,
			       (unsigned char *)"InitiatorName=",
			       14) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
				"failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	if (iscsi_pdu_add_data(iscsi, pdu,
			       (unsigned char *)iscsi->initiator_name,
			       strlen(iscsi->initiator_name) +1) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
				"failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* optional alias */
	if (iscsi->alias) {
		if (iscsi_pdu_add_data(iscsi, pdu,
				       (unsigned char *)"InitiatorAlias=",
				       15) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
					"failed.");
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
		if (iscsi_pdu_add_data(iscsi, pdu,
				       (unsigned char *)iscsi->alias,
				       strlen(iscsi->alias) +1) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
					"failed.");
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
	}

	/* target name */
	if (iscsi->session_type == ISCSI_SESSION_NORMAL) {
		if (iscsi->target_name == NULL) {
			iscsi_set_error(iscsi, "Trying normal connect but "
					"target name not set.");
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}

		if (iscsi_pdu_add_data(iscsi, pdu,
				       (unsigned char *)"TargetName=",
				       11) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
					"failed.");
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
		if (iscsi_pdu_add_data(iscsi, pdu,
				       (unsigned char *)iscsi->target_name,
				       strlen(iscsi->target_name) +1) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
					"failed.");
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
	}

	/* session type */
	switch (iscsi->session_type) {
	case ISCSI_SESSION_DISCOVERY:
		str = (char *)"SessionType=Discovery";
		break;
	case ISCSI_SESSION_NORMAL:
		str = (char *)"SessionType=Normal";
		break;
	default:
		iscsi_set_error(iscsi, "Can not handle sessions %d yet.",
				iscsi->session_type);
		return -1;
	}
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	switch (iscsi->want_header_digest) {
	case ISCSI_HEADER_DIGEST_NONE:
		str = (char *)"HeaderDigest=None";
		break;
	case ISCSI_HEADER_DIGEST_NONE_CRC32C:
		str = (char *)"HeaderDigest=None,CRC32C";
		break;
	case ISCSI_HEADER_DIGEST_CRC32C_NONE:
		str = (char *)"HeaderDigest=CRC32C,None";
		break;
	case ISCSI_HEADER_DIGEST_CRC32C:
		str = (char *)"HeaderDigest=CRC32C";
		break;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"DataDigest=None";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"InitialR2T=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"ImmediateData=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"MaxBurstLength=262144";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"FirstBurstLength=262144";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"MaxRecvDataSegmentLength=262144";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"DataPDUInOrder=Yes";
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}
	str = (char *)"DataSequenceInOrder=Yes";
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
				"pdu.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

int
iscsi_process_login_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			  const unsigned char *hdr, int size)
{
	int status;

	if (size < ISCSI_HEADER_SIZE) {
		iscsi_set_error(iscsi, "dont have enough data to read status "
				"from login reply");
		return -1;
	}

	status = ntohs(*(uint16_t *)&hdr[36]);
	if (status != 0) {
		pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			      pdu->private_data);
		return 0;
	}

	iscsi->statsn = ntohs(*(uint16_t *)&hdr[24]);

	/* XXX here we should parse the data returned in case the target
	 * renegotiated some some parameters.
	 *  we should also do proper handshaking if the target is not yet
	 * prepared to transition to the next stage
	 */
	/* skip past the header */
	hdr  += ISCSI_HEADER_SIZE;
	size -= ISCSI_HEADER_SIZE;

	while (size > 0) {
		int len;

		len = strlen((char *)hdr);

		if (len == 0) {
			break;
		}

		if (len > size) {
			iscsi_set_error(iscsi, "len > size when parsing "
					"login data %d>%d", len, size);
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				      pdu->private_data);
			return -1;
		}

		/* parse the strings */
		if (!strncmp((char *)hdr, "HeaderDigest=", 13)) {
			if (!strcmp((char *)hdr + 13, "CRC32C")) {
				iscsi->header_digest
				  = ISCSI_HEADER_DIGEST_CRC32C;
			} else {
				iscsi->header_digest
				  = ISCSI_HEADER_DIGEST_NONE;
			}
		}

		hdr  += len + 1;
		size -= len + 1;
	}


	iscsi->is_loggedin = 1;
	pdu->callback(iscsi, SCSI_STATUS_GOOD, NULL, pdu->private_data);

	return 0;
}


int
iscsi_logout_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		   void *private_data)
{
	struct iscsi_pdu *pdu;

	if (iscsi->is_loggedin == 0) {
		iscsi_set_error(iscsi, "Trying to logout while not logged in.");
		return -1;
	}

	pdu = iscsi_allocate_pdu(iscsi, ISCSI_PDU_LOGOUT_REQUEST,
				 ISCSI_PDU_LOGOUT_RESPONSE);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
				"logout pdu.");
		return -1;
	}

	/* logout request has the immediate flag set */
	iscsi_pdu_set_immediate(pdu);

	/* flags : close the session */
	iscsi_pdu_set_pduflags(pdu, 0x80);


	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"logout pdu.");
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

int
iscsi_process_logout_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
const unsigned char *hdr _U_, int size _U_)
{
	iscsi->is_loggedin = 0;
	pdu->callback(iscsi, SCSI_STATUS_GOOD, NULL, pdu->private_data);

	return 0;
}

int
iscsi_set_session_type(struct iscsi_context *iscsi,
		       enum iscsi_session_type session_type)
{
	if (iscsi->is_loggedin) {
		iscsi_set_error(iscsi, "trying to set session type while "
				"logged in");
		return -1;
	}

	iscsi->session_type = session_type;

	return 0;
}
