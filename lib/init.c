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

#if defined(WIN32)
#else
#include <strings.h>
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
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

	memset(iscsi, 0, sizeof(struct iscsi_context));

	iscsi->initiator_name = strdup(initiator_name);
	if (iscsi->initiator_name == NULL) {
		free(iscsi);
		return NULL;
	}

	iscsi->fd = -1;

	/* initialize to a "random" isid */
	iscsi_set_isid_random(iscsi, rand(), 0);

	/* assume we start in security negotiation phase */
	iscsi->current_phase = ISCSI_PDU_LOGIN_CSG_SECNEG;
	iscsi->next_phase    = ISCSI_PDU_LOGIN_NSG_OPNEG;
	iscsi->secneg_phase  = ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP;

	iscsi->max_burst_length                       = 262144;
	iscsi->first_burst_length                     = 262144;
	iscsi->initiator_max_recv_data_segment_length = 262144;
	iscsi->target_max_recv_data_segment_length    = 8192;
	iscsi->want_initial_r2t                       = ISCSI_INITIAL_R2T_NO;
	iscsi->use_initial_r2t                        = ISCSI_INITIAL_R2T_NO;
	iscsi->want_immediate_data                    = ISCSI_IMMEDIATE_DATA_YES;
	iscsi->use_immediate_data                     = ISCSI_IMMEDIATE_DATA_YES;
	iscsi->want_header_digest                     = ISCSI_HEADER_DIGEST_NONE_CRC32C;

	iscsi->tcp_keepcnt=3;
	iscsi->tcp_keepintvl=30;
	iscsi->tcp_keepidle=30;

	return iscsi;
}

int
iscsi_set_isid_oui(struct iscsi_context *iscsi, uint32_t oui, uint32_t qualifier)
{
	iscsi->isid[0] = (oui >> 16) & 0x3f;
	iscsi->isid[1] = (oui >>  8) & 0xff;
	iscsi->isid[2] = (oui      ) & 0xff;

	iscsi->isid[3] = (qualifier >> 16) & 0xff;
	iscsi->isid[4] = (qualifier >>  8) & 0xff;
	iscsi->isid[5] = (qualifier      ) & 0xff;

	return 0;
}

int
iscsi_set_isid_en(struct iscsi_context *iscsi, uint32_t en, uint32_t qualifier)
{
	iscsi->isid[0] = 0x40;

	iscsi->isid[1] = (en >>  16) & 0xff;
	iscsi->isid[2] = (en >>   8) & 0xff;
	iscsi->isid[3] = (en       ) & 0xff;

	iscsi->isid[4] = (qualifier >>  8) & 0xff;
	iscsi->isid[5] = (qualifier      ) & 0xff;

	return 0;
}

int
iscsi_set_isid_random(struct iscsi_context *iscsi, uint32_t rnd, uint32_t qualifier)
{
	iscsi->isid[0] = 0x80;

	iscsi->isid[1] = (rnd >>  16) & 0xff;
	iscsi->isid[2] = (rnd >>   8) & 0xff;
	iscsi->isid[3] = (rnd       ) & 0xff;

	iscsi->isid[4] = (qualifier >>  8) & 0xff;
	iscsi->isid[5] = (qualifier      ) & 0xff;

	return 0;
}


int
iscsi_set_isid_reserved(struct iscsi_context *iscsi)
{
	iscsi->isid[0] = 0xc0;

	iscsi->isid[1] = 0x00;
	iscsi->isid[2] = 0x00;
	iscsi->isid[3] = 0x00;
	iscsi->isid[4] = 0x00;
	iscsi->isid[5] = 0x00;

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
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			/* If an error happened during connect/login, we dont want to
			   call any of the callbacks.
			 */
			if (iscsi->is_loggedin) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
						pdu->private_data);
			}		      
		}
		iscsi_free_pdu(iscsi, pdu);
	}
	while ((pdu = iscsi->waitpdu)) {
		SLIST_REMOVE(&iscsi->waitpdu, pdu);
		/* If an error happened during connect/login, we dont want to
		   call any of the callbacks.
		 */
		if (iscsi->is_loggedin) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
					pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}

	free(discard_const(iscsi->initiator_name));
	iscsi->initiator_name = NULL;

	free(discard_const(iscsi->target_name));
	iscsi->target_name = NULL;

	free(discard_const(iscsi->target_address));
	iscsi->target_address = NULL;

	free(discard_const(iscsi->alias));
	iscsi->alias = NULL;

	free(discard_const(iscsi->portal));
	iscsi->portal = NULL;

	if (iscsi->incoming != NULL) {
		iscsi_free_iscsi_in_pdu(iscsi->incoming);
	}
	if (iscsi->inqueue != NULL) {
		iscsi_free_iscsi_inqueue(iscsi->inqueue);
	}

	free(iscsi->error_string);
	iscsi->error_string = NULL;

	free(discard_const(iscsi->user));
	iscsi->user = NULL;

	free(discard_const(iscsi->passwd));
	iscsi->passwd = NULL;

	free(discard_const(iscsi->chap_c));
	iscsi->chap_c = NULL;

    if (iscsi->connected_portal != NULL) {
	    free(discard_const(iscsi->connected_portal));
	    iscsi->connected_portal = NULL;
	}

	iscsi->connect_data = NULL;

	free(iscsi);

	return 0;
}

void
iscsi_set_error(struct iscsi_context *iscsi, const char *error_string, ...)
{
	va_list ap;
	char *str;

	va_start(ap, error_string);
	str = malloc(1024);
	if (vsnprintf(str, 1024, error_string, ap) < 0) {
		/* not much we can do here */
		free(str);
		str = NULL;
	}

	free(iscsi->error_string);

	iscsi->error_string = str;
	
	va_end(ap);
	
	DPRINTF(iscsi,1,"%s",str);
}

void
iscsi_set_debug(struct iscsi_context *iscsi, int level)
{
	iscsi->debug = level;
	DPRINTF(iscsi,2,"set debug level to %d",level);
}

const char *
iscsi_get_error(struct iscsi_context *iscsi)
{
	return iscsi->error_string;
}

const char *
iscsi_get_target_address(struct iscsi_context *iscsi)
{
	return iscsi->target_address;
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
	if ((unsigned)header_digest > ISCSI_HEADER_DIGEST_LAST) {
		iscsi_set_error(iscsi, "invalid header digest value");
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

struct iscsi_url *
iscsi_parse_full_url(struct iscsi_context *iscsi, const char *url)
{
	struct iscsi_url *iscsi_url;
	char *str;
	char *portal;
	char *user = NULL;
	char *passwd = NULL;
	char *target;
	char *lun;
	char *tmp;
	int l;

	if (strncmp(url, "iscsi://", 8)) {
		iscsi_set_error(iscsi, "Invalid URL %s\niSCSI URL must be of "
				"the form: %s",
				url,
				ISCSI_URL_SYNTAX);
		return NULL;
	}

	str = strdup(url + 8);
	if (str == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup url %s", url);
		return NULL;
	}
	portal = str;

	user   = getenv("LIBISCSI_CHAP_USERNAME");
	passwd = getenv("LIBISCSI_CHAP_PASSWORD");

	tmp = strchr(portal, '@');
	if (tmp != NULL) {
		user = portal;
		*tmp++	= 0;
		portal = tmp;

		tmp = strchr(user, '%');
		if (tmp == NULL) {
			tmp = strchr(user, ':');
		}
		if (tmp != NULL) {
			*tmp++ = 0;
			passwd = tmp;
		}

	}

	target = strchr(portal, '/');
	if (target == NULL) {
		iscsi_set_error(iscsi, "Invalid URL %s\nCould not parse "
				"'<target-iqn>'\niSCSI URL must be of the "
				"form: %s",
				url,
				ISCSI_URL_SYNTAX);
		free(str);
		return NULL;
	}
	*target++ = 0;

	if (*target == 0) {
		iscsi_set_error(iscsi, "Invalid URL %s\nCould not parse "
				"<target-iqn>\n"
				"iSCSI URL must be of the form: %s",
				url,
				ISCSI_URL_SYNTAX);
		free(str);
		return NULL;
	}

	lun = strchr(target, '/');
	if (lun == NULL) {
		iscsi_set_error(iscsi, "Invalid URL %s\nCould not parse <lun>\n"
				"iSCSI URL must be of the form: %s",
				url,
				ISCSI_URL_SYNTAX);
		free(str);
		return NULL;
	}
	*lun++ = 0;

	l = strtol(lun, &tmp, 10);
	if (*lun == 0 || *tmp != 0) {
		iscsi_set_error(iscsi, "Invalid URL %s\nCould not parse <lun>\n"
				"iSCSI URL must be of the form: %s",
				url,
				ISCSI_URL_SYNTAX);
		free(str);
		return NULL;
	}

	iscsi_url = malloc(sizeof(struct iscsi_url));
	if (iscsi_url == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate iscsi_url structure");
		free(str);
		return NULL;
	}
	memset(iscsi_url, 0, sizeof(struct iscsi_url));

	iscsi_url->portal = strdup(portal);
	if (iscsi_url->portal == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup portal string");
		iscsi_destroy_url(iscsi_url);
		free(str);
		return NULL;
	}

	iscsi_url->target = strdup(target);
	if (iscsi_url->target == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup target string");
		iscsi_destroy_url(iscsi_url);
		free(str);
		return NULL;
	}

	if (user != NULL && passwd != NULL) {
		iscsi_url->user = strdup(user);
		if (iscsi_url->user == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup username string");
			iscsi_destroy_url(iscsi_url);
			free(str);
			return NULL;
		}

		iscsi_url->passwd = strdup(passwd);
		if (iscsi_url->passwd == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup password string");
			iscsi_destroy_url(iscsi_url);
			free(str);
			return NULL;
		}
	}
	
	iscsi_url->lun = l;
	free(str);
	return iscsi_url;
}

struct iscsi_url *
iscsi_parse_portal_url(struct iscsi_context *iscsi, const char *url)
{
	struct iscsi_url *iscsi_url;
	char *str;
	char *portal;
	char *user = NULL;
	char *passwd = NULL;
	char *tmp;

	if (strncmp(url, "iscsi://", 8)) {
		iscsi_set_error(iscsi, "Invalid URL %s\niSCSI Portal URL must be of "
				"the form: %s",
				url,
				ISCSI_PORTAL_URL_SYNTAX);
		return NULL;
	}

	str = strdup(url + 8);
	if (str == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup url %s", url);
		return NULL;
	}
	portal = str;

	user   = getenv("LIBISCSI_CHAP_USERNAME");
	passwd = getenv("LIBISCSI_CHAP_PASSWORD");

	tmp = strchr(portal, '@');
	if (tmp != NULL) {
		user = portal;
		*tmp++	= 0;
		portal = tmp;

		tmp = strchr(user, '%');
		if (tmp != NULL) {
			*tmp++ = 0;
			passwd = tmp;
		}
	}


	iscsi_url = malloc(sizeof(struct iscsi_url));
	if (iscsi_url == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate iscsi_url structure");
		free(str);
		return NULL;
	}
	memset(iscsi_url, 0, sizeof(struct iscsi_url));

	iscsi_url->portal = strdup(portal);
	if (iscsi_url->portal == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup portal string");
		iscsi_destroy_url(iscsi_url);
		free(str);
		return NULL;
	}

	if (user != NULL && passwd != NULL) {
		iscsi_url->user = strdup(user);
		if (iscsi_url->user == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup username string");
			iscsi_destroy_url(iscsi_url);
			free(str);
			return NULL;
		}

		iscsi_url->passwd = strdup(passwd);
		if (iscsi_url->passwd == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory: Failed to strdup password string");
			iscsi_destroy_url(iscsi_url);
			free(str);
			return NULL;
		}
	}
	
	free(str);
	return iscsi_url;
}

void
iscsi_destroy_url(struct iscsi_url *iscsi_url)
{
	if (iscsi_url == NULL) {
		return;
	}

	free(discard_const(iscsi_url->portal));
	free(discard_const(iscsi_url->target));
	free(discard_const(iscsi_url->user));
	free(discard_const(iscsi_url->passwd));
	free(iscsi_url);
}


int
iscsi_set_initiator_username_pwd(struct iscsi_context *iscsi,
    					    const char *user,
					    const char *passwd)
{
	free(discard_const(iscsi->user));
	iscsi->user = strdup(user);
	if (iscsi->user == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to strdup username");
		return -1;
	}

	free(discard_const(iscsi->passwd));
	iscsi->passwd = strdup(passwd);
	if (iscsi->passwd == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: failed to strdup password");
		return -1;
	}

	return 0;
}
