/* 
   Copyright (C) 2015 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <getopt.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:prog-timeout";

void print_usage(void)
{
	fprintf(stderr, "Usage: prog_timeout [-?|--help] [--usage] "
		"[-i|--initiator-name=iqn-name]\n"
		"\t\t<iscsi-portal-url>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "This command is used to test that if we do not "
		"receive a responmse we recognize to a task that we will "
		"trigger the task timeout.\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: prog_timeout [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     "
		"Initiatorname to use\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Help options:\n");
	fprintf(stderr, "  -?, --help                        "
		"Show this help message\n");
	fprintf(stderr, "      --usage                       "
		"Display brief usage message\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "iSCSI Portal URL format : %s\n",
		ISCSI_PORTAL_URL_SYNTAX);
	fprintf(stderr, "\n");
	fprintf(stderr, "<host> is either of:\n");
	fprintf(stderr, "  \"hostname\"       iscsi.example\n");
	fprintf(stderr, "  \"ipv4-address\"   10.1.1.27\n");
	fprintf(stderr, "  \"ipv6-address\"   [fce0::1]\n");
}

void tur_cb(struct iscsi_context *iscsi _U_, int status, void *command_data _U_, void *private_data)
{
	uint32_t *i = private_data;

	(*i)--;

	printf("testunitready cb\n");
	if (status != SCSI_STATUS_TIMEOUT) {
		printf("Failed. We did NOT get a TIMEOUT error for the SCSI "
		       "task\n");
		exit(10);
	}
}

void logout_cb(struct iscsi_context *iscsi _U_, int status, void *command_data _U_, void *private_data)
{
	uint32_t *i = private_data;

	(*i)--;

	printf("logout command cb\n");
	if (status != SCSI_STATUS_TIMEOUT) {
		printf("Failed. We did NOT get a TIMEOUT error for the iSCSI "
		       "logout command\n");
		exit(10);
	}
}

int main(int argc, char *argv[])
{
	struct iscsi_context *iscsi;
	struct iscsi_url *iscsi_url = NULL;
	const char *url = NULL;
	int c;
	static int show_help = 0, show_usage = 0, debug = 0;
	uint32_t count;

	static struct option long_options[] = {
		{"help",           no_argument,          NULL,        'h'},
		{"usage",          no_argument,          NULL,        'u'},
		{"debug",          no_argument,          NULL,        'd'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{0, 0, 0, 0}
	};
	int option_index;

	while ((c = getopt_long(argc, argv, "h?uUdi:s", long_options,
			&option_index)) != -1) {
		switch (c) {
		case 'h':
		case '?':
			show_help = 1;
			break;
		case 'u':
			show_usage = 1;
			break;
		case 'd':
			debug = 1;
			break;
		case 'i':
			initiator = optarg;
			break;
		default:
			fprintf(stderr, "Unrecognized option '%c'\n\n", c);
			print_help();
			exit(0);
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

	if (optind != argc -1) {
		print_usage();
		exit(0);
	}

	if (argv[optind] != NULL) {
		url = strdup(argv[optind]);
	}
	if (url == NULL) {
		fprintf(stderr, "You must specify iscsi target portal.\n");
		print_usage();
		exit(10);
	}

	iscsi = iscsi_create_context(initiator);
	if (iscsi == NULL) {
		printf("Failed to create context\n");
		exit(10);
	}

	if (debug > 0) {
		iscsi_set_log_level(iscsi, debug);
		iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
	}

	iscsi_url = iscsi_parse_full_url(iscsi, url);
	
	if (url) {
		free(discard_const(url));
	}

	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n", 
			iscsi_get_error(iscsi));
		exit(10);
	}

	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun)
	    != 0) {
		fprintf(stderr, "iscsi_connect failed. %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}

	iscsi_set_timeout(iscsi, 3);
	count=0;

	printf("Send a TUR we will never get a reply for\n");
	count++;
	iscsi_testunitready_task(iscsi, iscsi_url->lun, tur_cb, &count);

	printf("Send a LOGOUT we will never get a reply for\n");
	count++;
	iscsi_logout_async(iscsi, logout_cb, &count);


	printf("Spin on iscsi_service(iscsi, 0) until all callbacks are "
	       "triggered\n");
	while (count) {
	      	iscsi_service(iscsi, 0);
	      	sleep(1);
	}
	printf("yey, we got all timeouts we expected\n");

	iscsi_destroy_url(iscsi_url);
	iscsi_destroy_context(iscsi);
	return 0;
}

