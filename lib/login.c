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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include "win32/win32_compat.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "md5.h"
#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>
#endif

static int
iscsi_login_add_initiatorname(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send InitiatorName during opneg or the first leg of secneg */
	if ((iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP)
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

	if (snprintf(str, MAX_STRING_SIZE, "InitiatorName=%s", iscsi->initiator_name) == -1) {
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
	if ((iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP)
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

	if (snprintf(str, MAX_STRING_SIZE, "InitiatorAlias=%s", iscsi->alias) == -1) {
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
	if ((iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP)
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
		return 0;
	}

	if (!iscsi->target_name[0]) {
		iscsi_set_error(iscsi, "Trying normal connect but "
				"target name not set.");
		return -1;
	}

	if (snprintf(str, MAX_STRING_SIZE, "TargetName=%s", iscsi->target_name) == -1) {
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

	/* We only send SessionType during opneg or the first leg of secneg */
	if ((iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG
	&& iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP)
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP) {
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

	if (snprintf(str, MAX_STRING_SIZE, "InitialR2T=%s", iscsi->want_initial_r2t == ISCSI_INITIAL_R2T_NO ?
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

	if (snprintf(str, MAX_STRING_SIZE, "ImmediateData=%s", iscsi->want_immediate_data == ISCSI_IMMEDIATE_DATA_NO ?
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

	if (snprintf(str, MAX_STRING_SIZE, "MaxBurstLength=%d", iscsi->max_burst_length) == -1) {
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

	if (snprintf(str, MAX_STRING_SIZE, "FirstBurstLength=%d", iscsi->first_burst_length) == -1) {
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

	if (snprintf(str, MAX_STRING_SIZE, "MaxRecvDataSegmentLength=%d", iscsi->initiator_max_recv_data_segment_length) == -1) {
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
iscsi_login_add_rdma_extensions(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send DataSequenceInOrder during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}
	/* RDMAExtensions is only valid for iSER transport */
	if (iscsi->transport != ISER_TRANSPORT)
	{
		return 0;
	}
	strncpy(str,"RDMAExtensions=Yes",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1)
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_initiatorrecvdatasegmentlength(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send InitiatorRecvDataSegmentLength during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}
	/* InitiatorRecvDataSegmentLength is only valid for iSER transport */
	if (iscsi->transport != ISER_TRANSPORT)
	{
		return 0;
	}

	if (snprintf(str, MAX_STRING_SIZE, "InitiatorRecvDataSegmentLength=%d",
                 iscsi->initiator_max_recv_data_segment_length) == -1) {
		iscsi_set_error(iscsi, "Out-of-memory: aprintf failed.");
		return -1;
	}
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str)+1) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	return 0;
}

static int
iscsi_login_add_targetrecvdatasegmentlength(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];

	/* We only send InitiatorRecvDataSegmentLength during opneg */
	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_OPNEG) {
		return 0;
	}
	/* TargetRecvDataSegmentLength is only valid for iSER transport */
	if (iscsi->transport != ISER_TRANSPORT)
	{
		return 0;
	}

	if (snprintf(str, MAX_STRING_SIZE, "TargetRecvDataSegmentLength=%d",
				 iscsi->target_max_recv_data_segment_length) == -1) {
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

#ifndef HAVE_LIBGCRYPT
typedef struct MD5Context *gcry_md_hd_t;
#define gcry_md_write MD5Update
#define GCRY_MD_MD5 1

static void gcry_md_open(gcry_md_hd_t *hd, int algo, unsigned int flags)
{
	assert(algo == GCRY_MD_MD5 && flags == 0);
	*hd = malloc(sizeof(struct MD5Context));
	if (*hd) {
		MD5Init(*hd);
	}
}

static void gcry_md_putc(gcry_md_hd_t h, unsigned char c)
{
	MD5Update(h, &c, 1);
}

static char *gcry_md_read(gcry_md_hd_t h, int algo)
{
	unsigned char digest[16];
	assert(algo == 0 || algo == GCRY_MD_MD5);

	MD5Final(digest, h);
	return memcpy(h->buf, digest, sizeof(digest));
}

static void gcry_md_close(gcry_md_hd_t h)
{
	memset(h, 0, sizeof(*h));
	free(h);
}
#endif

/* size of the challenge used for bidirectional chap */
#define TARGET_CHAP_C_SIZE 32

static int
iscsi_login_add_chap_response(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	char str[MAX_STRING_SIZE+1];
	char * strp;
	unsigned char c, cc[2];
	unsigned char digest[CHAP_R_SIZE];
	gcry_md_hd_t ctx;
	int i;

	if (iscsi->current_phase != ISCSI_PDU_LOGIN_CSG_SECNEG
	|| iscsi->secneg_phase != ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE) {
		return 0;
	}

	if (!iscsi->chap_c[0]) {
		iscsi_set_error(iscsi, "No CHAP challenge found");
		return -1;
	}

	gcry_md_open(&ctx, GCRY_MD_MD5, 0);
	if (ctx == NULL) {
		iscsi_set_error(iscsi, "Cannot create MD5 algorithm");
		return -1;
	}
	gcry_md_putc(ctx, iscsi->chap_i);
	gcry_md_write(ctx, (unsigned char *)iscsi->passwd, strlen(iscsi->passwd));

	strp = iscsi->chap_c;
	while (*strp != 0) {
		c = (h2i(strp[0]) << 4) | h2i(strp[1]);
		strp += 2;
		gcry_md_putc(ctx, c);
	}
	memcpy(digest, gcry_md_read(ctx, 0), sizeof(digest));
	gcry_md_close(ctx);

	strncpy(str,"CHAP_R=0x",MAX_STRING_SIZE);
	if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str, strlen(str))
	    != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: pdu add data failed.");
		return -1;
	}

	for (i = 0; i < CHAP_R_SIZE; i++) {
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

	/* bidirectional chap */
	if (iscsi->target_user[0]) {
		unsigned char target_chap_c[TARGET_CHAP_C_SIZE];

		iscsi->target_chap_i++;
		snprintf(str, MAX_STRING_SIZE, "CHAP_I=%d",
			 iscsi->target_chap_i);
		if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str,
				       strlen(str) + 1) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add "
					"data failed.");
			return -1;
		}

		for (i = 0; i < TARGET_CHAP_C_SIZE; i++) {
			target_chap_c[i] = rand()&0xff;
		}
		strncpy(str, "CHAP_C=0x", MAX_STRING_SIZE);
		if (iscsi_pdu_add_data(iscsi, pdu, (unsigned char *)str,
				       strlen(str)) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
					"failed.");
			return -1;
		}
		for (i = 0; i < TARGET_CHAP_C_SIZE; i++) {
			c = target_chap_c[i];
			cc[0] = i2h((c >> 4)&0x0f);
			cc[1] = i2h((c     )&0x0f);
			if (iscsi_pdu_add_data(iscsi, pdu, &cc[0], 2) != 0) {
				iscsi_set_error(iscsi, "Out-of-memory: pdu add "
						"data failed.");
				return -1;
			}
		}
		c = 0;
		if (iscsi_pdu_add_data(iscsi, pdu, &c, 1) != 0) {
			iscsi_set_error(iscsi, "Out-of-memory: pdu add data "
					"failed.");
			return -1;
		}

		gcry_md_open(&ctx, GCRY_MD_MD5, 0);
		if (ctx == NULL) {
			iscsi_set_error(iscsi, "Cannot create MD5 algorithm");
			return -1;
		}
		gcry_md_putc(ctx, iscsi->target_chap_i);
		gcry_md_write(ctx, (unsigned char *)iscsi->target_passwd,
			      strlen(iscsi->target_passwd));
		gcry_md_write(ctx, (unsigned char *)target_chap_c,
			      TARGET_CHAP_C_SIZE);

		memcpy(iscsi->target_chap_r, gcry_md_read(ctx, 0),
		       sizeof(iscsi->target_chap_r));
		gcry_md_close(ctx);
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

	/* randomize cmdsn and itt */
	if (!iscsi->current_phase && !iscsi->secneg_phase) {
		iscsi->itt = (uint32_t) rand();
		iscsi->cmdsn = (uint32_t) rand();
		iscsi->expcmdsn = iscsi->maxcmdsn = iscsi->min_cmdsn_waiting = iscsi->cmdsn;
	}

	pdu = iscsi_allocate_pdu(iscsi,
				 ISCSI_PDU_LOGIN_REQUEST,
				 ISCSI_PDU_LOGIN_RESPONSE,
				 iscsi->itt,
				 ISCSI_PDU_DROP_ON_RECONNECT);
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
				"login pdu.");
		return -1;
	}

	/* login request */
	iscsi_pdu_set_immediate(pdu);

	/* cmdsn is not increased if Immediate delivery*/
	iscsi_pdu_set_cmdsn(pdu, iscsi->cmdsn);

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
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* optional alias */
	if (iscsi->alias[0]) {
		if (iscsi_login_add_alias(iscsi, pdu) != 0) {
			iscsi->drv->free_pdu(iscsi, pdu);
			return -1;
		}
	}

	/* target name */
	if (iscsi->session_type == ISCSI_SESSION_NORMAL) {
		if (iscsi_login_add_targetname(iscsi, pdu) != 0) {
			iscsi->drv->free_pdu(iscsi, pdu);
			return -1;
		}
	}

	/* session type */
	if (iscsi_login_add_sessiontype(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* header digest */
	if (iscsi_login_add_headerdigest(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* auth method */
	if (iscsi_login_add_authmethod(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* auth algorithm */
	if (iscsi_login_add_authalgorithm(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* chap username */
	if (iscsi_login_add_chap_username(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* chap response */
	if (iscsi_login_add_chap_response(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* data digest */
	if (iscsi_login_add_datadigest(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* initial r2t */
	if (iscsi_login_add_initialr2t(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* immediate data */
	if (iscsi_login_add_immediatedata(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* max burst length */
	if (iscsi_login_add_maxburstlength(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* first burst length */
	if (iscsi_login_add_firstburstlength(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* default time 2 wait */
	if (iscsi_login_add_defaulttime2wait(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* default time 2 retain */
	if (iscsi_login_add_defaulttime2retain(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* max outstanding r2t */
	if (iscsi_login_add_maxoutstandingr2t(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* errorrecoverylevel */
	if (iscsi_login_add_errorrecoverylevel(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* ifmarker */
	if (iscsi_login_add_ifmarker(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* ofmarker */
	if (iscsi_login_add_ofmarker(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* maxconnections */
	if (iscsi_login_add_maxconnections(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* max recv data segment length */
	if (iscsi_login_add_maxrecvdatasegmentlength(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* data pdu in order */
	if (iscsi_login_add_datapduinorder(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* data sequence in order */
	if (iscsi_login_add_datasequenceinorder(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* rdmaextensions */
	if (iscsi_login_add_rdma_extensions(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* initiator recv data segment length */
	if (iscsi_login_add_initiatorrecvdatasegmentlength(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	/* target recv data segment length */
	if (iscsi_login_add_targetrecvdatasegmentlength(iscsi, pdu) != 0) {
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"pdu.");
		iscsi->drv->free_pdu(iscsi, pdu);
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
	uint32_t status;
	char *ptr = (char *)in->data;
	int size = in->data_pos;
	int must_have_chap_n = 0;
	int must_have_chap_r = 0;

	status = scsi_get_uint16(&in->hdr[36]);

	// Status-Class is 0
	if (!(status >> 8)) {
		if (!iscsi->current_phase && !iscsi->secneg_phase) {
			iscsi->statsn = scsi_get_uint32(&in->hdr[24]);
		}
	}

	/* Using bidirectional CHAP? Then we must see a chap_n and chap_r
	 * field in this PDU
	 */
	if ((in->hdr[1] & ISCSI_PDU_LOGIN_TRANSIT)
	&& (in->hdr[1] & ISCSI_PDU_LOGIN_CSG_FF) == ISCSI_PDU_LOGIN_CSG_SECNEG
	&& iscsi->target_user[0]) {
		must_have_chap_n = 1;
		must_have_chap_r = 1;
	}

	/* XXX here we should parse the data returned in case the target
	 * renegotiated some some parameters.
	 *  we should also do proper handshaking if the target is not yet
	 * prepared to transition to the next stage
	 */

	while (size > 0) {
		char *end;
		int len;

		end = memchr(ptr, 0, size);
		if (end == NULL) {
			iscsi_set_error(iscsi, "NUL not found after offset %ld "
					"when parsing login data",
					(long)((unsigned char *)ptr - in->data));
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
				              pdu->private_data);
			}
			return -1;
		}

		len = end - ptr;
		if (len == 0) {
			break;
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
			} else if (iscsi->want_immediate_data == ISCSI_IMMEDIATE_DATA_NO) {
				/* If we negotiated NO, it doesnt matter what
				 * the target said. ImmediateData is NO.
				 */
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

        /* iSER specific keys */
        if (!strncmp(ptr, "InitiatorRecvDataSegmentLength=", 31)) {
			iscsi->initiator_max_recv_data_segment_length = MIN(strtol(ptr + 31, NULL, 10),
                                                             iscsi->initiator_max_recv_data_segment_length);
        }
        if (!strncmp(ptr, "TargetRecvDataSegmentLength=", 28)) {
			iscsi->target_max_recv_data_segment_length = MIN(strtol(ptr + 28, NULL, 10),
                                                             iscsi->target_max_recv_data_segment_length);
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
			if (len-9 > MAX_CHAP_C_LENGTH) {
				iscsi_set_error(iscsi, "Wrong length of CHAP_C received from"
						" target (%d, max: %d)", len-9, MAX_CHAP_C_LENGTH);
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				return 0;
			}
			*iscsi->chap_c = '\0';
			strncat(iscsi->chap_c,ptr+9,len-9);
			iscsi->secneg_phase = ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE;
		}

		if (!strncmp(ptr, "CHAP_N=", 7)) {
			if (strcmp(iscsi->target_user, ptr + 7)) {
				iscsi_set_error(iscsi, "Failed to log in to"
						" target. Wrong CHAP targetname"
						" received: %s", ptr + 7);
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				return 0;
			}
			must_have_chap_n = 0;
		}

		if (!strncmp(ptr, "CHAP_R=0x", 9)) {
			int i;

			if (len != 9 + 2 * CHAP_R_SIZE) {
				iscsi_set_error(iscsi, "Wrong size of CHAP_R"
						" received from target.");
				if (pdu->callback) {
					pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
					              pdu->private_data);
				}
				return 0;
			}
			for (i = 0; i < CHAP_R_SIZE; i++) {
				unsigned char c;
				c = ((h2i(ptr[9 + 2 * i]) << 4) | h2i(ptr[9 + 2 * i + 1]));
				if (c != iscsi->target_chap_r[i]) {
					iscsi_set_error(iscsi, "Authentication "
						"failed. Invalid CHAP_R "
						"response from the target");
					if (pdu->callback) {
						pdu->callback(iscsi, SCSI_STATUS_ERROR,
						              NULL, pdu->private_data);
					}
					return 0;
				}
			}
			must_have_chap_r = 0;
		}

		ISCSI_LOG(iscsi, 6, "TargetLoginReply: %s", ptr);

		ptr  += len + 1;
		size -= len + 1;
	}

	if (status == SCSI_STATUS_REDIRECT && iscsi->target_address[0]) {
		ISCSI_LOG(iscsi, 2, "target requests redirect to %s",iscsi->target_address);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_REDIRECT, NULL,
			              pdu->private_data);
		}
		return 0;
	}

	if (status != 0) {
		iscsi_set_error(iscsi, "Failed to log in to target. Status: %s(%d)",
				       login_error_str(status), status);
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			              pdu->private_data);
		}
		return 0;
	}

	if (must_have_chap_n) {
		iscsi_set_error(iscsi, "Failed to log in to target. "
				"It did not return CHAP_N during SECNEG.");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			              pdu->private_data);
		}
		return 0;
	}

	if (must_have_chap_r) {
		iscsi_set_error(iscsi, "Failed to log in to target. "
				"It did not return CHAP_R during SECNEG.");
		if (pdu->callback) {
			pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL,
			              pdu->private_data);
		}
		return 0;
	}

	if (in->hdr[1] & ISCSI_PDU_LOGIN_TRANSIT) {
		iscsi->current_phase = (in->hdr[1] & ISCSI_PDU_LOGIN_NSG_FF) << 2;
	}

	if ((in->hdr[1] & ISCSI_PDU_LOGIN_TRANSIT)
	&& (in->hdr[1] & ISCSI_PDU_LOGIN_NSG_FF) == ISCSI_PDU_LOGIN_NSG_FF) {
		iscsi->is_loggedin = 1;
		iscsi_itt_post_increment(iscsi);
		iscsi->header_digest  = iscsi->want_header_digest;
		ISCSI_LOG(iscsi, 2, "login successful");
		pdu->callback(iscsi, SCSI_STATUS_GOOD, NULL, pdu->private_data);
	} else {
		if (iscsi_login_async(iscsi, pdu->callback, pdu->private_data) != 0) {
			iscsi_set_error(iscsi, "Failed to send continuation login pdu");
			if (pdu->callback) {
				pdu->callback(iscsi, SCSI_STATUS_ERROR, NULL, pdu->private_data);
			}
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

	pdu = iscsi_allocate_pdu(iscsi,
				 ISCSI_PDU_LOGOUT_REQUEST,
				 ISCSI_PDU_LOGOUT_RESPONSE,
				 iscsi_itt_post_increment(iscsi),
				 ISCSI_PDU_DROP_ON_RECONNECT|ISCSI_PDU_CORK_WHEN_SENT);
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

	pdu->callback     = cb;
	pdu->private_data = private_data;

	if (iscsi_queue_pdu(iscsi, pdu) != 0) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to queue iscsi "
				"logout pdu.");
		iscsi->drv->free_pdu(iscsi, pdu);
		return -1;
	}

	return 0;
}

int
iscsi_process_logout_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
struct iscsi_in_pdu *in _U_)
{
	iscsi->is_loggedin = 0;
	ISCSI_LOG(iscsi, 2, "logout successful");
	if (pdu->callback) {
		pdu->callback(iscsi, SCSI_STATUS_GOOD, NULL, pdu->private_data);
	}

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
