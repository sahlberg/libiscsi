/* 
   iscsi-test tool

   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>
#include <popt.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi.h"
#include "iscsi-test.h"

const char *initiator = "iqn.2010-11.iscsi-test";

struct scsi_test {
       const char *name;
       int (*test)(const char *initiator, const char *url);
};

struct scsi_test tests[] = {
/* read10*/
{ "T0100_read10_simple",		T0100_read10_simple },
{ "T0101_read10_beyond_eol",		T0101_read10_beyond_eol },
{ "T0102_read10_0blocks",		T0102_read10_0blocks },
{ "T0103_read10_rdprotect",		T0103_read10_rdprotect },
{ "T0104_read10_flags",			T0104_read10_flags },
{ "T0105_read10_invalid",      		T0105_read10_invalid },

/* readcapacity10*/
{ "T0110_readcapacity10_simple",	T0110_readcapacity10_simple },
{ "T0111_readcapacity10_pmi",		T0111_readcapacity10_pmi },

/* read6*/
{ "T0120_read6_simple",			T0120_read6_simple },
{ "T0121_read6_beyond_eol",		T0121_read6_beyond_eol },
{ "T0122_read6_invalid",      		T0122_read6_invalid },

/* verify10*/
{ "T0130_verify10_simple",		T0130_verify10_simple },
{ "T0131_verify10_mismatch",		T0131_verify10_mismatch },
{ "T0132_verify10_mismatch_no_cmp",	T0132_verify10_mismatch_no_cmp },

{ NULL, NULL }
};

void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-inq [-?] [-?|--help] [--usage] [-i|--initiator-name=iqn-name]\n"
			"\t\t<iscsi-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-inq [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -t, --test=test-name              Which test to run. Default is to run all tests.\n");
	fprintf(stderr, "  -l, --list                        List all tests.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Help options:\n");
	fprintf(stderr, "  -?, --help                        Show this help message\n");
	fprintf(stderr, "      --usage                       Display brief usage message\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "iSCSI URL format : %s\n", ISCSI_URL_SYNTAX);
	fprintf(stderr, "\n");
	fprintf(stderr, "<host> is either of:\n");
	fprintf(stderr, "  \"hostname\"       iscsi.example\n");
	fprintf(stderr, "  \"ipv4-address\"   10.1.1.27\n");
	fprintf(stderr, "  \"ipv6-address\"   [fce0::1]\n");
}


struct iscsi_context *iscsi_context_login(const char *initiatorname, const char *url, int *lun)
{
	struct iscsi_context *iscsi;
	struct iscsi_url *iscsi_url;

	iscsi = iscsi_create_context(initiatorname);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		return NULL;
	}

	iscsi_url = iscsi_parse_full_url(iscsi, url);
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n", 
			iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return NULL;
	}

	iscsi_set_targetname(iscsi, iscsi_url->target);
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_url->user != NULL) {
		if (iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
			fprintf(stderr, "Failed to set initiator username and password\n");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			return NULL;
		}
	}

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		return NULL;
	}
	if (lun != NULL) {
		*lun = iscsi_url->lun;
	}

	iscsi_destroy_url(iscsi_url);
	return iscsi;
}


int main(int argc, const char *argv[])
{
	poptContext pc;
	const char **extra_argv;
	int extra_argc = 0;
	const char *url = NULL;
	int show_help = 0, show_usage = 0, list_tests = 0;
	int res;
	struct scsi_test *test;
	char *testname = NULL;

	struct poptOption popt_options[] = {
		{ "help", '?', POPT_ARG_NONE, &show_help, 0, "Show this help message", NULL },
		{ "usage", 0, POPT_ARG_NONE, &show_usage, 0, "Display brief usage message", NULL },
		{ "list", 'l', POPT_ARG_NONE, &list_tests, 0, "List all tests", NULL },
		{ "initiator-name", 'i', POPT_ARG_STRING, &initiator, 0, "Initiatorname to use", "iqn-name" },
		{ "test", 't', POPT_ARG_STRING, &testname, 0, "Which test to run", "testname" },
		POPT_TABLEEND
	};

	pc = poptGetContext(argv[0], argc, argv, popt_options, POPT_CONTEXT_POSIXMEHARDER);
	if ((res = poptGetNextOpt(pc)) < -1) {
		fprintf(stderr, "Failed to parse option : %s %s\n",
			poptBadOption(pc, 0), poptStrerror(res));
		exit(10);
	}
	extra_argv = poptGetArgs(pc);
	if (extra_argv) {
		url = *extra_argv;
		extra_argv++;
		while (extra_argv[extra_argc]) {
			extra_argc++;
		}
	}

	if (show_help != 0) {
		print_help();
		exit(0);
	}

	if (show_usage != 0) {
		print_usage();
		exit(0);
	}

	if (list_tests != 0) {
		for (test = &tests[0]; test->name; test++) {
			printf("%s\n", test->name);
		}
		exit(0);
	}
	poptFreeContext(pc);

	if (url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		exit(10);
	}

	for (test = &tests[0]; test->name; test++) {
		if (testname != NULL && strcmp(testname, test->name)) {
			continue;
		}

		printf("=========\n");
		printf("Running test %s\n", test->name);
		res = test->test(initiator, url);
		if (res == 0) {
			printf("TEST %s [OK]\n", test->name);
		} else {
			printf("TEST %s [FAILED]\n", test->name);
		}
		printf("\n");
	}

	return 0;
}

