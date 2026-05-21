/*
   Copyright (C) 2019-2026 SUSE LLC
   Copyright (C) 2013 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <arpa/inet.h>
#include <CUnit/CUnit.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#include <string.h>
#include <gnutls/gnutls.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

static int
test_iscsi_strip_tag(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
		     const char *tag, char **out_val)
{
	unsigned char *s;
	unsigned char *end;
	size_t remain;
	size_t toklen;

	toklen = strlen(tag);
	if ((toklen < 2) || (tag[toklen - 1] != '=')) {
		return -EINVAL;
	}

	s = memmem(pdu->outdata.data, pdu->outdata.size, tag, toklen);
	if (s == NULL) {
		return -ENOENT;
	}

	remain = pdu->outdata.size - (s - pdu->outdata.data);
	if ((remain == 0) || (remain > pdu->outdata.size)) {
		return -EINVAL;
	}

	end = memchr(s, 0, remain);
	if (end == NULL) {
		return -EINVAL;
	}

	if (out_val != NULL) {
		/* stash tag value for the caller to use */
		*out_val = strdup((char *)(s + toklen));
	}

	toklen = end - s;
	assert(toklen > 0);

	/* handle padding */
	while ((toklen < remain) && (s[toklen] == '\0')) {
		toklen++;
	}

	memmove(s, s + toklen, remain - toklen);
	pdu->outdata.size -= toklen;

	/* update data segment length */
	scsi_set_uint32(&pdu->outdata.data[4], pdu->outdata.size
				- ISCSI_HEADER_SIZE(iscsi->header_digest));
	logging(LOG_VERBOSE, "stripped %s key and value from PDU", tag);

	return 0;
}

static void
chap_r_mod_b64_replace_queue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	int ret;
	char *chap_r_str = NULL;
	size_t chap_r_strlen;
	char *kv_buf = NULL;
	gnutls_datum_t hex;
	gnutls_datum_t bin;
	gnutls_datum_t b64;

        if ((pdu->outdata.data[0] & 0x3f) != ISCSI_PDU_LOGIN_REQUEST) {
		goto out;
	}

	ret = test_iscsi_strip_tag(iscsi, pdu, "CHAP_R=", &chap_r_str);
	if (ret == -ENOENT) {
		logging(LOG_VERBOSE, "ignoring login PDU without CHAP_R");
		goto out;
	}
	if (ret < 0) {
		return;
	}

	logging(LOG_VERBOSE, "CHAP_R=%s converting to base64", chap_r_str);

	chap_r_strlen = strlen(chap_r_str);
	if (chap_r_strlen < 2 ||
	    (chap_r_str[0] != '0' ||
	     (chap_r_str[1] != 'x' && chap_r_str[1] != 'X'))) {
		CU_FAIL("unexpected CHAP_R hex prefix from libiscsi");
		free(chap_r_str);
		goto out;
	}

	hex = (gnutls_datum_t){
		.data = (void *)(chap_r_str + 2),
		.size = strlen(chap_r_str + 2),
	};
	ret = gnutls_hex_decode2(&hex, &bin);
	free(chap_r_str);
	if (ret < 0) {
		CU_FAIL("gnutls_hex_decode2() failed");
		goto out;
	}

	ret = gnutls_base64_encode2(&bin, &b64);
	gnutls_free(bin.data);
	if (ret < 0) {
		CU_FAIL("gnutls_base64_encode2() failed");
		goto out;
	}

	kv_buf = malloc(sizeof("CHAP_R=0b") + b64.size);
	/* nulterm space from sizeof(), doesn't matter if @b64 includes it */
	sprintf(kv_buf, "CHAP_R=0b%.*s", b64.size, b64.data);
	gnutls_free(b64.data);

	ret = iscsi_pdu_add_data(iscsi, pdu, (const unsigned char *)kv_buf,
				 strlen(kv_buf) + 1);
	logging(LOG_VERBOSE, "replaced Login PDU CHAP_R with %s", kv_buf);
	free(kv_buf);
	if (ret < 0) {
		return;
	}
out:
        orig_queue_pdu(iscsi, pdu);
}

static int
test_iscsi_chap_login(void (*test_queue_pdu)(struct iscsi_context *iscsi,
                                             struct iscsi_pdu *pdu))
{
        struct iscsi_context *iscsi;
        struct iscsi_url *iscsi_url;
        int ret;

        iscsi = iscsi_create_context(initiatorname2);
	if (iscsi == NULL) {
		return -ENOMEM;
	}

        iscsi_url = iscsi_parse_full_url(iscsi, sd->iscsi_url);
	if (iscsi_url == NULL) {
		ret = -ENOMEM;
		goto err_iscsi_destroy;
	}

        iscsi_set_targetname(iscsi, iscsi_url->target);
        iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
        iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
	iscsi_set_noautoreconnect(iscsi, 1);

        iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user,
					 iscsi_url->passwd);

        /* override transport queue_pdu callback for PDU manipulation */
        iscsi->drv->queue_pdu = test_queue_pdu;

        ret = iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun);
	if (ret < 0) {
		ret = -EIO;
		goto err_url_destroy;
	}

	ret = 0;
err_url_destroy:
	iscsi_destroy_url(iscsi_url);
err_iscsi_destroy:
	iscsi_destroy_context(iscsi);
	return ret;
}

void
test_iscsi_chap_base64(void)
{
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test CHAP_C base64 encoding");

        CHECK_FOR_ISCSI(sd);
        if (sd->iscsi_ctx->chap_a != 5) {
                const char *err = "[SKIPPED] This test requires "
                        "an iSCSI session with CHAP_A=5";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

	ret = test_iscsi_chap_login(chap_r_mod_b64_replace_queue);
	CU_ASSERT_EQUAL(ret, 0);
}
