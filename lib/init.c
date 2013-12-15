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

void* iscsi_malloc(struct iscsi_context *iscsi, size_t size) {
	void * ptr = malloc(size);
	if (ptr != NULL) iscsi->mallocs++;
	return ptr;
}

void* iscsi_zmalloc(struct iscsi_context *iscsi, size_t size) {
	void * ptr = malloc(size);
	if (ptr != NULL) {
		memset(ptr,0x00,size);
		iscsi->mallocs++;
	}
	return ptr;
}

void* iscsi_realloc(struct iscsi_context *iscsi, void* ptr, size_t size) {
	void * _ptr = realloc(ptr, size);
	if (_ptr != NULL) {
		iscsi->reallocs++;
	}
	return _ptr;
}

void iscsi_free(struct iscsi_context *iscsi, void* ptr) {
	if (ptr == NULL) return;
	free(ptr);
	iscsi->frees++;
}

char* iscsi_strdup(struct iscsi_context *iscsi, const char* str) {
	char *str2 = strdup(str);
	if (str2 != NULL) iscsi->mallocs++;
	return str2;
}

void* iscsi_szmalloc(struct iscsi_context *iscsi, size_t size) {
	void *ptr;
	if (size > iscsi->smalloc_size) return NULL;
	if (iscsi->smalloc_free > 0) {
		ptr = iscsi->smalloc_ptrs[--iscsi->smalloc_free];
		memset(ptr, 0, iscsi->smalloc_size);
		iscsi->smallocs++;
	} else {
		ptr = iscsi_zmalloc(iscsi, iscsi->smalloc_size);
	}
	return ptr;
}

void iscsi_sfree(struct iscsi_context *iscsi, void* ptr) {
	if (ptr == NULL) {
		return;
	}
	if (iscsi->smalloc_free == SMALL_ALLOC_MAX_FREE) {
		int i;
		/* SMALL_ALLOC_MAX_FREE should be adjusted that this happens rarely */
		ISCSI_LOG(iscsi, 6, "smalloc free == SMALLOC_MAX_FREE");
		/* remove oldest half of free pointers and copy
		 * upper half to lower half */
		iscsi->smalloc_free >>= 1;
		for (i = 0; i < iscsi->smalloc_free; i++) {
			iscsi_free(iscsi, iscsi->smalloc_ptrs[i]);
			iscsi->smalloc_ptrs[i] = iscsi->smalloc_ptrs[i + iscsi->smalloc_free];
		}
	}
	iscsi->smalloc_ptrs[iscsi->smalloc_free++] = ptr;
}

struct iscsi_context *
iscsi_create_context(const char *initiator_name)
{
	struct iscsi_context *iscsi;
	size_t required = ISCSI_RAW_HEADER_SIZE + ISCSI_DIGEST_SIZE;

	if (!initiator_name[0]) {
		return NULL;
	}

	iscsi = malloc(sizeof(struct iscsi_context));
	if (iscsi == NULL) {
		return NULL;
	}
	
	memset(iscsi, 0, sizeof(struct iscsi_context));

	strncpy(iscsi->initiator_name,initiator_name,MAX_STRING_SIZE);

	iscsi->fd = -1;
	
	srand(time(NULL) ^ getpid() ^ (uint32_t) ((uintptr_t) iscsi));

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
	iscsi->use_initial_r2t                        = ISCSI_INITIAL_R2T_YES;
	iscsi->want_immediate_data                    = ISCSI_IMMEDIATE_DATA_YES;
	iscsi->use_immediate_data                     = ISCSI_IMMEDIATE_DATA_YES;
	iscsi->want_header_digest                     = ISCSI_HEADER_DIGEST_NONE_CRC32C;

	iscsi->tcp_keepcnt=3;
	iscsi->tcp_keepintvl=30;
	iscsi->tcp_keepidle=30;
	
	iscsi->reconnect_max_retries = -1;

	if (getenv("LIBISCSI_DEBUG") != NULL) {
		iscsi_set_log_level(iscsi, atoi(getenv("LIBISCSI_DEBUG")));
		iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
	}

	if (getenv("LIBISCSI_TCP_USER_TIMEOUT") != NULL) {
		iscsi_set_tcp_user_timeout(iscsi,atoi(getenv("LIBISCSI_TCP_USER_TIMEOUT")));
	}

	if (getenv("LIBISCSI_TCP_KEEPCNT") != NULL) {
		iscsi_set_tcp_keepcnt(iscsi,atoi(getenv("LIBISCSI_TCP_KEEPCNT")));
	}

	if (getenv("LIBISCSI_TCP_KEEPINTVL") != NULL) {
		iscsi_set_tcp_keepintvl(iscsi,atoi(getenv("LIBISCSI_TCP_KEEPINTVL")));
	}

	if (getenv("LIBISCSI_TCP_KEEPIDLE") != NULL) {
		iscsi_set_tcp_keepidle(iscsi,atoi(getenv("LIBISCSI_TCP_KEEPIDLE")));
	}

	if (getenv("LIBISCSI_TCP_SYNCNT") != NULL) {
		iscsi_set_tcp_syncnt(iscsi,atoi(getenv("LIBISCSI_TCP_SYNCNT")));
	}

	if (getenv("LIBISCSI_BIND_INTERFACES") != NULL) {
		iscsi_set_bind_interfaces(iscsi,getenv("LIBISCSI_BIND_INTERFACES"));
	}

	/* iscsi->smalloc_size is the size for small allocations. this should be
	   max(ISCSI_HEADER_SIZE, sizeof(struct iscsi_pdu), sizeof(struct iscsi_in_pdu))
	   rounded up to the next power of 2. */
	iscsi->smalloc_size = 1;
	if (sizeof(struct iscsi_pdu) > required) {
		required = sizeof(struct iscsi_pdu);
	}
	if (sizeof(struct iscsi_in_pdu) > required) {
		required = sizeof(struct iscsi_in_pdu);
	}
	while (iscsi->smalloc_size < required) {
		iscsi->smalloc_size <<= 1;
	}
	ISCSI_LOG(iscsi,5,"small allocation size is %d byte", iscsi->smalloc_size);

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

	strncpy(iscsi->alias,alias,MAX_STRING_SIZE);
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

	strncpy(iscsi->target_name,target_name,MAX_STRING_SIZE);

	return 0;
}

int
iscsi_destroy_context(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;
	int i;

	if (iscsi == NULL) {
		return 0;
	}

	if (iscsi->fd != -1) {
		iscsi_disconnect(iscsi);
	}

	while ((pdu = iscsi->outqueue)) {
		SLIST_REMOVE(&iscsi->outqueue, pdu);
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			/* If an error happened during connect/login, we don't want to
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
		/* If an error happened during connect/login, we don't want to
		   call any of the callbacks.
		 */
		if (iscsi->is_loggedin) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
					pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}

	if (iscsi->outqueue_current != NULL && iscsi->outqueue_current->flags & ISCSI_PDU_DELETE_WHEN_SENT) {
		iscsi_free_pdu(iscsi, iscsi->outqueue_current);
	}

	if (iscsi->incoming != NULL) {
		iscsi_free_iscsi_in_pdu(iscsi, iscsi->incoming);
	}
	if (iscsi->inqueue != NULL) {
		iscsi_free_iscsi_inqueue(iscsi, iscsi->inqueue);
	}

	iscsi->connect_data = NULL;

	for (i=0;i<iscsi->smalloc_free;i++) {
		iscsi_free(iscsi, iscsi->smalloc_ptrs[i]);
	}

	if (iscsi->mallocs != iscsi->frees) {
		ISCSI_LOG(iscsi,1,"%d memory blocks lost at iscsi_destroy_context() after %d malloc(s), %d realloc(s), %d free(s) and %d reused small allocations",iscsi->mallocs-iscsi->frees,iscsi->mallocs,iscsi->reallocs,iscsi->frees,iscsi->smallocs);
	} else {
		ISCSI_LOG(iscsi,5,"memory is clean at iscsi_destroy_context() after %d mallocs, %d realloc(s), %d free(s) and %d reused small allocations",iscsi->mallocs,iscsi->reallocs,iscsi->frees,iscsi->smallocs);
	}
	
	memset(iscsi, 0, sizeof(struct iscsi_context));
	free(iscsi);

	return 0;
}

void
iscsi_set_error(struct iscsi_context *iscsi, const char *error_string, ...)
{
	va_list ap;
	char errstr[MAX_STRING_SIZE + 1] = {0};

	va_start(ap, error_string);
	if (vsnprintf(errstr, MAX_STRING_SIZE, error_string, ap) < 0) {
		strncpy(errstr, "could not format error string!", MAX_STRING_SIZE);
	}
	va_end(ap);

	if (iscsi != NULL) {
		strncpy(iscsi->error_string, errstr,MAX_STRING_SIZE);
		ISCSI_LOG(iscsi, 1, "%s",iscsi->error_string);
	}
}

void
iscsi_set_log_level(struct iscsi_context *iscsi, int level)
{
	iscsi->log_level = level;
	ISCSI_LOG(iscsi, 2, "set log level to %d", level);
}

const char *
iscsi_get_error(struct iscsi_context *iscsi)
{
	return iscsi ? iscsi->error_string : "";
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

static void
iscsi_decode_url_string(char *str)
{
	while (*str) {
		char *tmp = str;
		char c;

		if (*str++ != '%') {
			continue;
		}

		if (*str == 0) {
			return;
		}
		c = h2i(*str++) << 4;

		if (*str == 0) {
			return;
		}
		c |= h2i(*str++);

		*tmp++ = c;
		memmove(tmp, str, strlen(str));
		tmp[strlen(str)] = 0;
	}
}

struct iscsi_url *
iscsi_parse_url(struct iscsi_context *iscsi, const char *url, int full)
{
	struct iscsi_url *iscsi_url;
	char str[MAX_STRING_SIZE+1];
	char *portal;
	char *user = NULL;
	char *passwd = NULL;
	char *target = NULL;
	char *lun;
	char *tmp;
	int l = 0;

	if (strncmp(url, "iscsi://", 8)) {
		if (full) {
			iscsi_set_error(iscsi, "Invalid URL %s\niSCSI URL must "
				"be of the form: %s",
				url, ISCSI_URL_SYNTAX);
		} else {
			iscsi_set_error(iscsi, "Invalid URL %s\niSCSI Portal "
				"URL must be of the form: %s",
				url, ISCSI_PORTAL_URL_SYNTAX);
		}
		return NULL;
	}

	strncpy(str,url + 8, MAX_STRING_SIZE);
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

	if (full) {
		target = strchr(portal, '/');
		if (target == NULL) {
			iscsi_set_error(iscsi, "Invalid URL %s\nCould not "
				"parse '<target-iqn>'\niSCSI URL must be of "
				"the form: %s",
				url, ISCSI_URL_SYNTAX);
			return NULL;
		}
		*target++ = 0;

		if (*target == 0) {
			iscsi_set_error(iscsi, "Invalid URL %s\nCould not "
				"parse <target-iqn>\niSCSI URL must be of the "
				"form: %s",
				url, ISCSI_URL_SYNTAX);
			return NULL;
		}

		lun = strchr(target, '/');
		if (lun == NULL) {
			iscsi_set_error(iscsi, "Invalid URL %s\nCould not "
				"parse <lun>\niSCSI URL must be of the form: "
				"%s",
				url, ISCSI_URL_SYNTAX);
			return NULL;
		}
		*lun++ = 0;

		l = strtol(lun, &tmp, 10);
		if (*lun == 0 || *tmp != 0) {
			iscsi_set_error(iscsi, "Invalid URL %s\nCould not "
				"parse <lun>\niSCSI URL must be of the form: "
				"%s",
				url, ISCSI_URL_SYNTAX);
			return NULL;
		}
	} else {
		tmp=strchr(portal,'/');
		if (tmp) {
			*tmp=0;
		}
	}
	
	if (iscsi != NULL) {
		iscsi_url = iscsi_malloc(iscsi, sizeof(struct iscsi_url));
	} else {
		iscsi_url = malloc(sizeof(struct iscsi_url));
	}

	if (iscsi_url == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: Failed to allocate "
			"iscsi_url structure");
		return NULL;
	}
	memset(iscsi_url, 0, sizeof(struct iscsi_url));
	iscsi_url->iscsi= iscsi;

	strncpy(iscsi_url->portal,portal,MAX_STRING_SIZE);

	if (user != NULL && passwd != NULL) {
		strncpy(iscsi_url->user, user, MAX_STRING_SIZE);
		strncpy(iscsi_url->passwd, passwd, MAX_STRING_SIZE);
	}

	if (full) {
		strncpy(iscsi_url->target, target, MAX_STRING_SIZE);
		iscsi_url->lun = l;
	}

	iscsi_decode_url_string(&iscsi_url->target[0]);

	return iscsi_url;
}

struct iscsi_url *
iscsi_parse_full_url(struct iscsi_context *iscsi, const char *url)
{
	return iscsi_parse_url(iscsi,url,1);
}

struct iscsi_url *
iscsi_parse_portal_url(struct iscsi_context *iscsi, const char *url)
{
	return iscsi_parse_url(iscsi,url,0);
}

void
iscsi_destroy_url(struct iscsi_url *iscsi_url)
{
	struct iscsi_context *iscsi = iscsi_url->iscsi;
	memset(iscsi_url, 0, sizeof(struct iscsi_url));
	if (iscsi != NULL)
		iscsi_free(iscsi, iscsi_url);
	else
		free(iscsi_url);
}


int
iscsi_set_initiator_username_pwd(struct iscsi_context *iscsi,
						    const char *user, const char *passwd)
{
	strncpy(iscsi->user,user,MAX_STRING_SIZE);
	strncpy(iscsi->passwd,passwd,MAX_STRING_SIZE);
	return 0;
}

int
iscsi_set_immediate_data(struct iscsi_context *iscsi, enum iscsi_immediate_data immediate_data)
{
	if (iscsi->is_loggedin != 0) {
		iscsi_set_error(iscsi, "Already logged in when trying to set immediate_data");
		return -1;
	}

	iscsi->want_immediate_data = immediate_data;
	iscsi->use_immediate_data  = immediate_data;
	return 0;
}

int
iscsi_set_initial_r2t(struct iscsi_context *iscsi, enum iscsi_initial_r2t initial_r2t)
{
	if (iscsi->is_loggedin != 0) {
		iscsi_set_error(iscsi, "Already logged in when trying to set initial_r2t");
		return -1;
	}

	iscsi->want_initial_r2t = initial_r2t;
	return 0;
}

int
iscsi_set_timeout(struct iscsi_context *iscsi, int timeout)
{
	iscsi->scsi_timeout = timeout;
	return 0;
}
