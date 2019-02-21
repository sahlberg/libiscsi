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

#define _GNU_SOURCE

#if defined(_WIN32)
#include "win32/win32_compat.h"
#else
#include <strings.h>
#include <unistd.h>
#endif

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include "iscsi.h"
#include "iscsi-private.h"
#ifdef HAVE_LINUX_ISER
#include "iser-private.h"
#endif
#include "slist.h"


/**
 * Initialize transport type of session
 */

int iscsi_init_transport(struct iscsi_context *iscsi,
			 enum iscsi_transport_type transport) {
	iscsi->transport = transport;

	switch (iscsi->transport) {
	case TCP_TRANSPORT:
		iscsi_init_tcp_transport(iscsi);
		break;
#ifdef HAVE_LINUX_ISER
	case ISER_TRANSPORT:
		iscsi_init_iser_transport(iscsi);
		break;
#endif
	default:
		iscsi_set_error(iscsi, "Unfamiliar transport type");
		return -1;
	}

	return 0;
}

/**
 * Whether or not the internal memory allocator caches allocations. Disable
 * memory allocation caching to improve the accuracy of Valgrind reports.
 */
void iscsi_set_cache_allocations(struct iscsi_context *iscsi, int ca)
{
	iscsi->cache_allocations = ca;
}

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

void* iscsi_smalloc(struct iscsi_context *iscsi, size_t size) {
	void *ptr;
	if (size > iscsi->smalloc_size) return NULL;
	if (iscsi->smalloc_free > 0) {
		ptr = iscsi->smalloc_ptrs[--iscsi->smalloc_free];
		iscsi->smallocs++;
	} else {
		ptr = iscsi_malloc(iscsi, iscsi->smalloc_size);
	}
	return ptr;
}

void* iscsi_szmalloc(struct iscsi_context *iscsi, size_t size) {
	void *ptr = iscsi_smalloc(iscsi, size);
	if (ptr) {
		memset(ptr, 0, size);
	}
	return ptr;
}

void iscsi_sfree(struct iscsi_context *iscsi, void* ptr) {
	if (ptr == NULL) {
		return;
	}
	if (!iscsi->cache_allocations) {
		iscsi_free(iscsi, ptr);
	} else if (iscsi->smalloc_free == SMALL_ALLOC_MAX_FREE) {
		/* SMALL_ALLOC_MAX_FREE should be adjusted that this */
		/* happens rarely */
		ISCSI_LOG(iscsi, 6, "smalloc free == SMALLOC_MAX_FREE");
		iscsi_free(iscsi, ptr);
	} else {
		iscsi->smalloc_ptrs[iscsi->smalloc_free++] = ptr;
	}
}

static bool rd_set = false;
static pthread_mutex_t rd_mutex = PTHREAD_MUTEX_INITIALIZER;

static void
iscsi_srand_init(struct iscsi_context *iscsi) {
	unsigned int seed;
	int urand_fd;
	ssize_t rc;
	int err;

	if (rd_set) {
		/* fast case, seed has been set */
		return;
	}

	err = pthread_mutex_lock(&rd_mutex);
	assert(err == 0);

	if (rd_set) {
		/* another thread initialized it in the meantime */
		goto out;
	}

	urand_fd = open("/dev/urandom", O_RDONLY);
	if (urand_fd == -1) {
		goto fallback;
	}

	rc = read(urand_fd, &seed, sizeof(seed));
	close(urand_fd);
	if (rc == -1) {
		goto fallback;
	}

	srand(seed);
	goto out;

fallback:
	/* seed based on @iscsi */
	srand(getpid() ^ (uint32_t)((uintptr_t) iscsi));

out:
	rd_set = true;
	err = pthread_mutex_unlock(&rd_mutex);
	assert(err == 0);
}

struct iscsi_context *
iscsi_create_context(const char *initiator_name)
{
	struct iscsi_context *iscsi;
	size_t required = ISCSI_RAW_HEADER_SIZE + ISCSI_DIGEST_SIZE;
	char *ca;

	if (!initiator_name[0]) {
		return NULL;
	}

	iscsi = malloc(sizeof(struct iscsi_context));
	if (iscsi == NULL) {
		return NULL;
	}

	memset(iscsi, 0, sizeof(struct iscsi_context));

	/* initalize transport of context */
	if (iscsi_init_transport(iscsi, TCP_TRANSPORT)) {
		iscsi_set_error(iscsi, "Failed allocating transport");
		return NULL;
	}

	strncpy(iscsi->initiator_name,initiator_name,MAX_STRING_SIZE);

	iscsi->fd = -1;

	/* initialize to a "random" isid */
	iscsi_srand_init(iscsi);
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
	required = MAX(required, sizeof(struct iscsi_pdu));
	required = MAX(required, sizeof(struct iscsi_in_pdu));
	iscsi->smalloc_size = 1;
	while (iscsi->smalloc_size < required) {
		iscsi->smalloc_size <<= 1;
	}
	ISCSI_LOG(iscsi,5,"small allocation size is %d byte", iscsi->smalloc_size);

	ca = getenv("LIBISCSI_CACHE_ALLOCATIONS");
	if (!ca || atoi(ca) != 0) {
		iscsi->cache_allocations = 1;
	}

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
	int i;

	if (iscsi == NULL) {
		return 0;
	}

	if (iscsi->fd != -1) {
		iscsi_disconnect(iscsi);
	}

	iscsi_cancel_pdus(iscsi);

	if (iscsi->outqueue_current != NULL && iscsi->outqueue_current->flags & ISCSI_PDU_DELETE_WHEN_SENT) {
		iscsi->drv->free_pdu(iscsi, iscsi->outqueue_current);
	}

	if (iscsi->incoming != NULL) {
		iscsi_free_iscsi_in_pdu(iscsi, iscsi->incoming);
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

	if (iscsi->old_iscsi) {
		iscsi->old_iscsi->fd = -1;
		iscsi_destroy_context(iscsi->old_iscsi);
	}

	iscsi_free(iscsi, iscsi->opaque);

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
	char *target_user = NULL;
	char *target_passwd = NULL;
	char *target = NULL;
	char *lun;
	char *tmp;
	int l = 0;
#ifdef HAVE_LINUX_ISER
	int is_iser = 0;
#endif

	if (strncmp(url, "iscsi://", 8)
#ifdef HAVE_LINUX_ISER
            && strncmp(url, "iser://", 7)
#endif
            ) {
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

#ifdef HAVE_LINUX_ISER
        if (!strncmp(url, "iser://", 7)) {
                is_iser = 1;
                strncpy(str, url + 7, MAX_STRING_SIZE);
        }
#endif
        if (!strncmp(url, "iscsi://", 8)) {
                strncpy(str, url + 8, MAX_STRING_SIZE);
        }
	portal = str;

	user          = getenv("LIBISCSI_CHAP_USERNAME");
	passwd        = getenv("LIBISCSI_CHAP_PASSWORD");
	target_user   = getenv("LIBISCSI_CHAP_TARGET_USERNAME");
	target_passwd = getenv("LIBISCSI_CHAP_TARGET_PASSWORD");

	tmp = strchr(portal, '?');
	if (tmp) {
		*tmp++ = 0;
		while (tmp && *tmp) {
			char *next = strchr(tmp, '&');
			char *key, *value;
			if (next != NULL) {
				*next++ = 0;
			}
			key = tmp;
			value = strchr(key, '=');
			if (value != NULL) {
				*value++ = 0;
			}
                        if (!strcmp(key, "header_digest")) {
                                if (!strcmp(value, "crc32c")) {
                                        iscsi_set_header_digest(
                                            iscsi, ISCSI_HEADER_DIGEST_CRC32C);
                                } else if (!strcmp(value, "none")) {
                                        iscsi_set_header_digest(
                                            iscsi, ISCSI_HEADER_DIGEST_NONE);
                                } else {
                                        iscsi_set_error(iscsi,
                                            "Invalid URL argument for header_digest: %s", value);
                                        return NULL;
                                }
                        }
			if (!strcmp(key, "target_user")) {
				target_user = value;
			} else if (!strcmp(key, "target_password")) {
				target_passwd = value;
#ifdef HAVE_LINUX_ISER
			} else if (!strcmp(key, "iser")) {
				is_iser = 1;
#endif
			}
			tmp = next;
		}
	}

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

	if (user && passwd && user[0] && passwd[0]) {
		strncpy(iscsi_url->user, user, MAX_STRING_SIZE);
		strncpy(iscsi_url->passwd, passwd, MAX_STRING_SIZE);
		/* if we do not have normal CHAP, that means we do not have
		 * bidirectional either.
		 */
		if (target_user && target_passwd && target_user[0] && target_passwd[0]) {
			strncpy(iscsi_url->target_user, target_user, MAX_STRING_SIZE);
			strncpy(iscsi_url->target_passwd, target_passwd, MAX_STRING_SIZE);
		}
	}

#ifdef HAVE_LINUX_ISER
	if (iscsi) {
		if (is_iser) {
			if (iscsi_init_transport(iscsi, ISER_TRANSPORT))
				iscsi_set_error(iscsi, "Cannot set transport to iSER");
		}
	}
	iscsi_url->transport = is_iser;
#endif

	if (full) {
		strncpy(iscsi_url->target, target, MAX_STRING_SIZE);
		iscsi_url->lun = l;
	}

	iscsi_decode_url_string(&iscsi_url->target[0]);

	/* NOTE: iscsi is allowed to be NULL. Especially qemu does call us with iscsi == NULL.
	 * If we receive iscsi != NULL we apply the parsed settings to the context. */
	if (iscsi) {
		iscsi_set_targetname(iscsi, iscsi_url->target);
		iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user, iscsi_url->passwd);
		iscsi_set_target_username_pwd(iscsi, iscsi_url->target_user, iscsi_url->target_passwd);
	}

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
	if (!user || !passwd || !user[0] || !passwd[0]) {
		iscsi->user[0] = 0;
		iscsi->passwd[0] = 0;
		return 0;
	}
	strncpy(iscsi->user,user,MAX_STRING_SIZE);
	strncpy(iscsi->passwd,passwd,MAX_STRING_SIZE);
	return 0;
}


int
iscsi_set_target_username_pwd(struct iscsi_context *iscsi,
			      const char *user, const char *passwd)
{
	if (!user || !passwd || !user[0] || !passwd[0]) {
		iscsi->target_user[0] = 0;
		iscsi->target_passwd[0] = 0;
		return 0;
	}
	strncpy(iscsi->target_user, user, MAX_STRING_SIZE);
	strncpy(iscsi->target_passwd, passwd, MAX_STRING_SIZE);
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
