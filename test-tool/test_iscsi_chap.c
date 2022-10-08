/*
   Copyright (C) 2019 SUSE LLC
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

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

static int
test_iscsi_strip_tag(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
		     const char *tag)
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

static int
chap_mod_strip_replace_queue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
			     const char *new_chap_a)
{
	int ret;

        if ((pdu->outdata.data[0] & 0x3f) != ISCSI_PDU_LOGIN_REQUEST) {
		goto out;
	}

	ret = test_iscsi_strip_tag(iscsi, pdu, "CHAP_A=");
	if (ret == -ENOENT) {
		logging(LOG_VERBOSE, "ignoring login PDU without CHAP_A");
		goto out;
	}
	if (ret < 0) {
		return ret;
	}
	ret = iscsi_pdu_add_data(iscsi, pdu, (const unsigned char *)new_chap_a,
				 strlen(new_chap_a) + 1);
	if (ret < 0) {
		return ret;
	}
	logging(LOG_VERBOSE, "replaced Login PDU CHAP_A setting with %s", new_chap_a);
out:
        return orig_queue_pdu(iscsi, pdu);

}

static int
chap_mod_many_types_queue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	return chap_mod_strip_replace_queue(iscsi, pdu, "CHAP_A=5,6,7,8");
}

static int
chap_mod_no_type_queue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	return chap_mod_strip_replace_queue(iscsi, pdu, "CHAP_A=");
}

static int
chap_mod_bad_type_queue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	/* value starts with '5', to catch targets that only check one byte */
	return chap_mod_strip_replace_queue(iscsi, pdu, "CHAP_A=56");
}

static int
test_iscsi_chap_login(int (*test_queue_pdu)(struct iscsi_context *iscsi,
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
test_iscsi_chap_simple(void)
{
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test CHAP_A negotiation");

        CHECK_FOR_ISCSI(sd);
        if (sd->iscsi_ctx->chap_a != 5) {
                const char *err = "[SKIPPED] This test requires "
                        "an iSCSI session with CHAP_A=5";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

	ret = test_iscsi_chap_login(chap_mod_many_types_queue);
	CU_ASSERT_EQUAL(ret, 0);
}

void
test_iscsi_chap_invalid(void)
{
        int ret;

        logging(LOG_VERBOSE, LOG_BLANK_LINE);
        logging(LOG_VERBOSE, "Test CHAP_A negotiation");

        CHECK_FOR_ISCSI(sd);
        if (sd->iscsi_ctx->chap_a != 5) {
                const char *err = "[SKIPPED] This test requires "
                        "an iSCSI session with CHAP_A=5";
                logging(LOG_NORMAL, "%s", err);
                CU_PASS(err);
                return;
        }

	ret = test_iscsi_chap_login(chap_mod_bad_type_queue);
	CU_ASSERT_EQUAL(ret, -EIO);

	ret = test_iscsi_chap_login(chap_mod_no_type_queue);
	CU_ASSERT_EQUAL(ret, -EIO);
}
