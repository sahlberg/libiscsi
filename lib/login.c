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

#if defined(WIN32)
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "md5.h"

static int
iscsi_login_add_initiatorname(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send InitiatorName during opneg or the first leg of secneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "InitiatorName=%s", iscsi->initiator_name) == -1) {
#else
	if (snprintf(str, MAX_STRING_SIZE, "InitiatorName=%s", iscsi->initiator_name) == -1) {
#endif
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	return 0;
}

static int
iscsi_login_add_alias(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send InitiatorAlias during opneg or the first leg of secneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "InitiatorAlias=%s", iscsi->alias) == -1) {
#else
	if (snprintf(str, MAX_STRING_SIZE, "InitiatorAlias=%s", iscsi->alias) == -1) {
#endif
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	return 0;
}

static int
iscsi_login_add_targetname(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send TargetName during opneg or the first leg of secneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

	if (!iscsi->target_name[0]) {
		iscsi_set_error(iscsi, "Trying normal connect but "
				"target name not set.");
		return -1;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "TargetName=%s", iscsi->target_name) == -1) {
#else
	if (snprintf(str, MAX_STRING_SIZE, "TargetName=%s", iscsi->target_name) == -1) {
#endif
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	return 0;
}

static int
iscsi_login_add_sessiontype(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send TargetName during opneg or the first leg of secneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

	switch (iscsi->session_type) {
	case ISCSI_SESSION_DISCOVERY:
		strncpy(str,"SessionType=Discovery",MAX_STRING_SIZE);
		break;
	case ISCSI_SESSION_NORMAL:
		strncpy(str,"SessionType=Normal",MAX_STRING_SIZE);
		break;
	default:
		iscsi_set_error(iscsi, "Can not handle sessions %d yet.",
				iscsi->session_type);
		return -1;
	}
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_headerdigest(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send HeaderDigest during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	switch (iscsi->want_header_digest) {
	case ISCSI_HEADER_DIGEST_NONE:
		strncpy(str,"HeaderDigest=None",MAX_STRING_SIZE);
		break;
	case ISCSI_HEADER_DIGEST_NONE_CRC32C:
		strncpy(str,"HeaderDigest=None,CRC32C",MAX_STRING_SIZE);
		break;
	case ISCSI_HEADER_DIGEST_CRC32C_NONE:
		strncpy(str,"HeaderDigest=CRC32C,None",MAX_STRING_SIZE);
		break;
	case ISCSI_HEADER_DIGEST_CRC32C:
		strncpy(str,"HeaderDigest=CRC32C",MAX_STRING_SIZE);
		break;
	default:
		iscsi_set_error(iscsi, "invalid header digest value");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_datadigest(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send DataDigest during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"DataDigest=None",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_initialr2t(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send InitialR2T during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "InitialR2T=%s", iscsi->want_initial_r2t == ISCSI_INITIAL_R2T_NO ?
#else
	if (snprintf(str, MAX_STRING_SIZE, "InitialR2T=%s", iscsi->want_initial_r2t == ISCSI_INITIAL_R2T_NO ?
#endif
		       "No" : "Yes") == -1) {
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_immediatedata(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send ImmediateData during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "ImmediateData=%s", iscsi->want_immediate_data == ISCSI_IMMEDIATE_DATA_NO ?
#else
	if (snprintf(str, MAX_STRING_SIZE, "ImmediateData=%s", iscsi->want_immediate_data == ISCSI_IMMEDIATE_DATA_NO ?
#endif
		       "No" : "Yes") == -1) {
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_maxburstlength(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send MaxBurstLength during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "MaxBurstLength=%d", iscsi->max_burst_length) == -1) {
#else
	if (snprintf(str, MAX_STRING_SIZE, "MaxBurstLength=%d", iscsi->max_burst_length) == -1) {
#endif
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	return 0;
}

static int
iscsi_login_add_firstburstlength(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send FirstBurstLength during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "FirstBurstLength=%d", iscsi->first_burst_length) == -1) {
#else
	if (snprintf(str, MAX_STRING_SIZE, "FirstBurstLength=%d", iscsi->first_burst_length) == -1) {
#endif
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	return 0;
}

static int
iscsi_login_add_maxrecvdatasegmentlength(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send MaxRecvDataSegmentLength during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

#if defined(WIN32)
	if (_snprintf_s(str, MAX_STRING_SIZE, MAX_STRING_SIZE, "MaxRecvDataSegmentLength=%d", iscsi->initiator_max_recv_data_segment_length) == -1) {
#else
	if (snprintf(str, MAX_STRING_SIZE, "MaxRecvDataSegmentLength=%d", iscsi->initiator_max_recv_data_segment_length) == -1) {
#endif
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}

	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	return 0;
}

static int
iscsi_login_add_datapduinorder(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send DataPduInOrder during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"DataPDUInOrder=Yes",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_defaulttime2wait(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send DefaultTime2Wait during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"DefaultTime2Wait=2",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_defaulttime2retain(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send DefaultTime2Retain during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"DefaultTime2Retain=0",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_ifmarker(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send IFMarker during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"IFMarker=No",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_ofmarker(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send OFMarker during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"OFMarker=No",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_maxconnections(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send MaxConnections during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"MaxConnections=1",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_maxoutstandingr2t(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send MaxOutstandingR2T during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"MaxOutstandingR2T=1",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_errorrecoverylevel(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send ErrorRecoveryLevel during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"ErrorRecoveryLevel=0",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_datasequenceinorder(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send DataSequenceInOrder during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}

	strncpy(str,"DataSequenceInOrder=Yes",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_authmethod(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_SECNEG
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

	strncpy(str,"AuthMethod=CHAP,None",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}
	
static int
iscsi_login_add_authalgorithm(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_SECNEG
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_SELECT_ALGORITHM) {
		return 0;
	}

	strncpy(str,"CHAP_A=5",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}
	
static int
iscsi_login_add_chap_username(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_SECNEG
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE) {
		return 0;
	}

	strncpy(str,"CHAP_N=",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str))
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}
	if (iscsi_pdu_add_data(iscsi, pdu,
			       (unsigned char *)iscsi->user,
			       strlen(iscsi->user) +1) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
				"failed.");
		return -1;
	}

	return 0;
}

static int
h2i(int h)
{
	if (h >= 'a' && h <= 'f') {
		return h - 'a' + 10;
	}
	if (h >= 'A' && h <= 'F') {
		return h - 'A' + 10;
	}
	return h - '0';
}
static int
i2h(int i)
{
	if (i >= 10) {
		return i - 10 + 'A';
	}

	return i + '0';
}

static int
iscsi_login_add_chap_response(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];
	char * strp;
	unsigned char c, cc[2];
	unsigned char digest[16];
	struct MD5Context ctx;
	int i;

	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_SECNEG
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE) {
		return 0;
	}

	if (!iscsi->chap_c[0]) {
		iscsi_set_error(iscsi, "No CHAP challenge found");
		return -1;
	}
	MD5Init(&ctx);
	c = iscsi->chap_i;
	MD5Update(&ctx, &c, 1);
	MD5Update(&ctx, (unsigned char *)iscsi->passwd, strlen(iscsi->passwd));
	
	strp = iscsi->chap_c;
	while (*strp != 0) {
		c = (h2i(strp[0]) << 4) | h2i(strp[1]);
		strp += 2;
		MD5Update(&ctx, &c, 1);
	}
	MD5Final(digest, &ctx);

	strncpy(str,"CHAP_R=0x",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str))
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	for (i=0; i<16; i++) {
		c = digest[i];
		cc[0] = i2h((c >> 4)&0x0f);
		cc[1] = i2h((c     )&0x0f);
		if (iscsi_pdu_add_data(iscsi, pdu, &cc[0], 2) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
				"failed.");
			return -1;
		}
	}
	c = 0;
	if (iscsi_pdu_add_data(iscsi, pdu, &c, 1) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
			"failed.");
		return -1;
	}

	return 0;
}

int
iscsi_login_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		  void *private_data)
{
	struct iscsi_pdu *pdu;
	int transit;

	if (iscsi->login_attempts++ > 10) {
		iscsi_set_error(iscsi, "login took too many tries."
				" giving up.");
		return -1;
	}
 
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

	pdu = iscsi_allocate_pdu_with_itt_flags_size(iscsi,
				ISCSI_PDU_LOGIN_REQUEST,
				ISCSI_PDU_LOGIN_RESPONSE,
				iscsi->itt, 0, 1024);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
				"login pdu.");
		return -1;
	}

	/* login request */
	iscsi_pdu_set_immediate(pdu);

	if (!iscsi->user[0]) {
		iscsi->current_phase = ISCSI_PDU_LOGIN_CSG_OPNEG;
	}

	if (iscsi->current_phase == ISCSI_PDU_LOGIN_CSG_SECNEG) {
		iscsi->next_phase    = ISCSI_PDU_LOGIN_NSG_OPNEG;
	}
	if (iscsi->current_phase == ISCSI_PDU_LOGIN_CSG_OPNEG) {
		iscsi->next_phase    = ISCSI_PDU_LOGIN_NSG_FF;
	}

	transit = 0;
	if (iscsi->current_phase == ISCSI_PDU_LOGIN_CSG_OPNEG) {
		transit = ISCSI_PDU_LOGIN_TRANSIT;
	}
	if (iscsi->current_phase == ISCSI_PDU_LOGIN_CSG_SECNEG) {
		if (iscsi->secneg_phase == ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
			transit = ISCSI_PDU_LOGIN_TRANSIT;
		}
		if (iscsi->secneg_phase == ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE) {
			transit = ISCSI_PDU_LOGIN_TRANSIT;
		}
	}

	/* flags */
	iscsi_pdu_set_pduflags(pdu, transit
					| iscsi->current_phase
					| iscsi->next_phase);


	/* initiator name */
	if (iscsi_login_add_initiatorname(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* optional alias */
	if (iscsi->alias) {
		if (iscsi_login_add_alias(iscsi, pdu) != 0) {
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
	}

	/* target name */
	if (iscsi->session_type == ISCSI_SESSION_NORMAL) {
		if (iscsi_login_add_targetname(iscsi, pdu) != 0) {
			iscsi_free_pdu(iscsi, pdu);
			return -1;
		}
	}

	/* session type */
	if (iscsi_login_add_sessiontype(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* header digest */
	if (iscsi_login_add_headerdigest(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* auth method */
	if (iscsi_login_add_authmethod(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* auth algorithm */
	if (iscsi_login_add_authalgorithm(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* chap username */
	if (iscsi_login_add_chap_username(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* chap response */
	if (iscsi_login_add_chap_response(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* data digest */
	if (iscsi_login_add_datadigest(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* initial r2t */
	if (iscsi_login_add_initialr2t(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* immediate data */
	if (iscsi_login_add_immediatedata(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* max burst length */
	if (iscsi_login_add_maxburstlength(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* first burst length */
	if (iscsi_login_add_firstburstlength(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* default time 2 wait */
	if (iscsi_login_add_defaulttime2wait(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* default time 2 retain */
	if (iscsi_login_add_defaulttime2retain(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* max outstanding r2t */
	if (iscsi_login_add_maxoutstandingr2t(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* errorrecoverylevel */
	if (iscsi_login_add_errorrecoverylevel(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* ifmarker */
	if (iscsi_login_add_ifmarker(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* ofmarker */
	if (iscsi_login_add_ofmarker(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* maxconnections */
	if (iscsi_login_add_maxconnections(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* max recv data segment length */
	if (iscsi_login_add_maxrecvdatasegmentlength(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* data pdu in order */
	if (iscsi_login_add_datapduinorder(iscsi, pdu) != 0) {
		iscsi_free_pdu(iscsi, pdu);
		return -1;
	}

	/* data sequence in order */
	if (iscsi_login_add_datasequenceinorder(iscsi, pdu) != 0) {
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

static const char *login_error_str(int status)
{
	switch (status) {
	case 0x0100: return "Target moved (unknown)"; /* Some don't set the detail */
	case 0x0101: return "Target moved temporarily";
	case 0x0102: return "Target moved permanently";
	case 0x0200: return "Initiator error";
	case 0x0201: return "Authentication failure";
	case 0x0202: return "Authorization failure";
	case 0x0203: return "Target not found";
	case 0x0204: return "Target removed";
	case 0x0205: return "Unsupported version";
	case 0x0206: return "Too many connections";
	case 0x0207: return "Missing parameter";
	case 0x0208: return "Can't include in session";
	case 0x0209: return "Session type not supported";
	case 0x020a: return "Session does not exist";
	case 0x020b: return "Invalid during login";
	case 0x0300: return "Target error";
	case 0x0301: return "Service unavailable";
	case 0x0302: return "Out of resources";
	}
	return "Unknown login error";
}


int
iscsi_process_login_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			  struct iscsi_in_pdu *in)
{
	uint32_t status, maxcmdsn;
	char *ptr = (char *)in->data;
	int size = in->data_pos;

	status = ntohs(*(uint16_t *)&in->hdr[36]);

	iscsi->statsn = ntohs(*(uint16_t *)&in->hdr[24]);

	maxcmdsn = ntohl(*(uint32_t *)&in->hdr[32]);
	if (maxcmdsn > iscsi->maxcmdsn) {
		iscsi->maxcmdsn = maxcmdsn;
	}

	/* XXX here we should parse the data returned in case the target
	 * renegotiated some some parameters.
	 *  we should also do proper handshaking if the target is not yet
	 * prepared to transition to the next stage
	 */

	while (size > 0) {
		int len;

		len = strlen(ptr);

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
		if (!strncmp(ptr, "TargetAddress=", 14)) {
			strncpy(iscsi->target_address,ptr+14,MAX_STRING_SIZE);
		}

		if (!strncmp(ptr, "HeaderDigest=", 13)) {
			if (!strcmp(ptr + 13, "CRC32C")) {
				iscsi->want_header_digest
				  = ISCSI_HEADER_DIGEST_CRC32C;
			} else {
				iscsi->want_header_digest
				  = ISCSI_HEADER_DIGEST_NONE;
			}
		}

		if (!strncmp(ptr, "FirstBurstLength=", 17)) {
			iscsi->first_burst_length = strtol(ptr + 17, NULL, 10);
		}

		if (!strncmp(ptr, "InitialR2T=", 11)) {
			if (!strcmp(ptr + 11, "No")) {
				iscsi->use_initial_r2t = ISCSI_INITIAL_R2T_NO;
			} else {
				iscsi->use_initial_r2t = ISCSI_INITIAL_R2T_YES;
			}
		}

		if (!strncmp(ptr, "ImmediateData=", 14)) {
			if (!strcmp(ptr + 14, "No")) {
				iscsi->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
			} else {
				iscsi->use_immediate_data = ISCSI_IMMEDIATE_DATA_YES;
			}
		}

		if (!strncmp(ptr, "MaxBurstLength=", 15)) {
			iscsi->max_burst_length = strtol(ptr + 15, NULL, 10);
		}

		if (!strncmp(ptr, "MaxRecvDataSegmentLength=", 25)) {
			iscsi->target_max_recv_data_segment_length = strtol(ptr + 25, NULL, 10);
		}

		if (!strncmp(ptr, "AuthMethod=", 11)) {
			if (!strcmp(ptr + 11, "CHAP")) {
				iscsi->secneg_phase = ISCSI_LOGIN_SECNEG_PHASE_SELECT_ALGORITHM;
			}
		}

		if (!strncmp(ptr, "CHAP_A=", 7)) {
			iscsi->chap_a = atoi(ptr+7);
			iscsi->secneg_phase = ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE;
		}

		if (!strncmp(ptr, "CHAP_I=", 7)) {
			iscsi->chap_i = atoi(ptr+7);
			iscsi->secneg_phase = ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE;
		}

		if (!strncmp(ptr, "CHAP_C=0x", 9)) {
			strncpy(iscsi->chap_c,ptr+9,MAX_STRING_SIZE);
			iscsi->secneg_phase = ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE;
		}

		ptr  += len + 1;
		size -= len + 1;
	}

	if (status == SCSI_STATUS_REDIRECT && iscsi->target_address) {
		ISCSI_LOG(iscsi, 2, "target requests redirect to %s",iscsi->target_address);
		pdu->callback(iscsi, SCSI_STATUS_REDIRECT, NULL,
				  pdu->private_data);
		return 0;
	}

	if (status != 0) {
		iscsi_set_error(iscsi, "Failed to log in to target. Status: %s(%d)",
				       login_error_str(status), status);
		pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			      pdu->private_data);
		return 0;
	}

	if (in->hdr[1] & ISCSI_PDU_LOGIN_TRANSIT) {
		iscsi->current_phase = (in->hdr[1] & ISCSI_PDU_LOGIN_NSG_FF) << 2;
	}

	if ((in->hdr[1] & ISCSI_PDU_LOGIN_TRANSIT)
	&& (in->hdr[1] & ISCSI_PDU_LOGIN_NSG_FF) == ISCSI_PDU_LOGIN_NSG_FF) {
		iscsi->is_loggedin = 1;
		iscsi->itt++;
		iscsi->header_digest  = iscsi->want_header_digest;
		pdu->callback(iscsi, SCSI_STATUS_GOOD, NULL, pdu->private_data);
	} else {
		if (iscsi_login_async(iscsi, pdu->callback, pdu->private_data) != 0) {
			iscsi_set_error(iscsi, "Failed to send continuation login pdu");
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL, pdu->private_data);
		}
	}

	return 0;
}


int
iscsi_logout_async(struct iscsi_context *iscsi, iscsi_command_cb cb,
		   void *private_data)
{
	struct iscsi_pdu *pdu;

	iscsi->login_attempts = 0;

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

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);
	pdu->cmdsn = iscsi->cmdsn;

	/* exp statsn */
	iscsi_pdu_set_expstatsn(pdu, iscsi->statsn+1);

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
struct iscsi_in_pdu *in)
{
	uint32_t maxcmdsn;

	maxcmdsn = ntohl(*(uint32_t *)&in->hdr[32]);
	if (maxcmdsn > iscsi->maxcmdsn) {
		iscsi->maxcmdsn = maxcmdsn;
	}

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
