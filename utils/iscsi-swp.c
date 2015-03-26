/* 
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-swp";


void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-swp [-?] [-?|--help] [--usage] [-i|--initiator-name=iqn-name]\n"
			"\t\t[-s|--swp {on|off}] <iscsi-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-swp [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -s, --swp={on|off}                Turn software write protect on/off\n");
	fprintf(stderr, "  -d, --debug=integer               debug level (0=disabled)\n");
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

int main(int argc, char *argv[])
{
	struct iscsi_context *iscsi;
	const char *url = NULL;
	struct iscsi_url *iscsi_url = NULL;
	int show_help = 0, show_usage = 0, debug = 0;
	int c;
	int ret = 0;
	int swp = 0;
	struct scsi_task *sense_task = NULL;
	struct scsi_task *select_task = NULL;
	struct scsi_mode_sense *ms;
	struct scsi_mode_page *mp;

	static struct option long_options[] = {
		{"help",           no_argument,          NULL,        'h'},
		{"usage",          no_argument,          NULL,        'u'},
		{"debug",          no_argument,          NULL,        'd'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{"swp",            required_argument,    NULL,        's'},
		{0, 0, 0, 0}
	};
	int option_index;

	while ((c = getopt_long(argc, argv, "h?udi:s:", long_options,
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
		case 's':
			if (!strcmp(optarg, "on") || !strcmp(optarg, "ON")) {
				swp = 1;
			}
			if (!strcmp(optarg, "off") || !strcmp(optarg, "OFF")) {
				swp = 2;
			}
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

	iscsi = iscsi_create_context(initiator);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(10);
	}

	if (debug > 0) {
		iscsi_set_log_level(iscsi, debug);
		iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
	}

	if (argv[optind] != NULL) {
		url = strdup(argv[optind]);
	}
	if (url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		ret = 10;
		goto finished;
	}
	iscsi_url = iscsi_parse_full_url(iscsi, url);
	
	free(discard_const(url));

	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n", 
			iscsi_get_error(iscsi));
		ret = 10;
		goto finished;
	}

	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		ret = 10;
		goto finished;
	}


	sense_task = iscsi_modesense10_sync(iscsi, iscsi_url->lun,
		0, 1, SCSI_MODESENSE_PC_CURRENT,
		SCSI_MODEPAGE_CONTROL,
		0, 255);
	if (sense_task == NULL) {
		printf("Failed to send MODE_SENSE10 command: %s\n",
			iscsi_get_error(iscsi));
		ret = 10;
		goto finished;
	}
	if (sense_task->status != SCSI_STATUS_GOOD) {
		printf("MODE_SENSE10 failed: %s\n",
			iscsi_get_error(iscsi));
		ret = 10;
		goto finished;
	}
	ms = scsi_datain_unmarshall(sense_task);
	if (ms == NULL) {
		printf("failed to unmarshall mode sense datain blob\n");
		ret = 10;
		goto finished;
	}
	mp = scsi_modesense_get_page(ms, SCSI_MODEPAGE_CONTROL, 0);
	if (mp == NULL) {
		printf("failed to read control mode page\n");
		ret = 10;
		goto finished;
	}

	/* For MODE SELECT PS is reserved and hence must be cleared */
	mp->ps = 0;

	printf("SWP:%d\n", mp->control.swp);

	switch (swp) {
	case 1:
		mp->control.swp = 1;
		break;
	case 2:
		mp->control.swp = 0;
		break;
	default:
		goto finished;
	}

	printf("Turning SWP %s\n", (swp == 1) ? "ON" : "OFF"); 
	select_task = iscsi_modeselect10_sync(iscsi, iscsi_url->lun,
		    1, 0, mp);
	if (select_task == NULL) {
		printf("Failed to send MODE_SELECT10 command: %s\n",
			iscsi_get_error(iscsi));
		ret = 10;
		goto finished;
	}
	if (select_task->status != SCSI_STATUS_GOOD) {
		printf("MODE_SELECT10 failed: %s\n",
			iscsi_get_error(iscsi));
		ret = 10;
		goto finished;
	}


finished:
	if (sense_task != NULL) {
		scsi_free_scsi_task(sense_task);
	}
	if (select_task != NULL) {
		scsi_free_scsi_task(select_task);
	}
	if (iscsi_url != NULL) {
		iscsi_destroy_url(iscsi_url);
	}
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return ret;
}

