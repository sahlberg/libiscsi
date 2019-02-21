/* 
   Copyright (C) 2012 by Peter Lieven <pl@kamp.de>
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
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <getopt.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-readcapacity16";

void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-readcapacity16 [-?] [-?|--help] [--usage] [-i|--initiator-name=iqn-name] [-s] <iscsi-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi_readcapacity16 [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -s, --size                        print target size only\n");
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
	int show_help = 0, show_usage = 0, debug = 0, size_only=0;
	int c;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;

	static struct option long_options[] = {
		{"help",           no_argument,          NULL,        'h'},
		{"usage",          no_argument,          NULL,        'u'},
		{"debug",          no_argument,          NULL,        'd'},
		{"size",           no_argument,          NULL,        's'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{"evpd",           required_argument,    NULL,        'e'},
		{"pagecode",       required_argument,    NULL,        'c'},
		{0, 0, 0, 0}
	};
	int option_index;

	while ((c = getopt_long(argc, argv, "h?udi:s", long_options,
			&option_index)) != -1) {
		switch (c) {
		case 'h':
		case '?':
			show_help = 1;
			break;
		case 'u':
			show_usage = 1;
			break;
		case 's':
			size_only = 1;
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

	iscsi = iscsi_create_context(initiator);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(10);
	}

	if (debug > 0) {
		iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
		iscsi_set_log_level(iscsi, debug);
	}

	if (argv[optind] != NULL) {
		url = strdup(argv[optind]);
	}
	if (url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		exit(10);
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
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		exit(10);
	}

	task = iscsi_readcapacity16_sync(iscsi, iscsi_url->lun);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr,"failed to send readcapacity command\n");
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		exit(10);
	}

	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		fprintf(stderr,"failed to unmarshall readcapacity16 data\n");
		scsi_free_scsi_task(task);
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		exit(10);
	}
 
	if (!size_only) {
		printf("RETURNED LOGICAL BLOCK ADDRESS:%" PRIu64 "\n", rc16->returned_lba);
		printf("LOGICAL BLOCK LENGTH IN BYTES:%u\n", rc16->block_length);
		printf("P_TYPE:%d PROT_EN:%d\n", rc16->p_type, rc16->prot_en);
		printf("P_I_EXPONENT:%d LOGICAL BLOCKS PER PHYSICAL BLOCK EXPONENT:%d\n", rc16->p_i_exp, rc16->lbppbe);
		printf("LBPME:%d LBPRZ:%d\n", rc16->lbpme, rc16->lbprz);
		printf("LOWEST ALIGNED LOGICAL BLOCK ADDRESS:%d\n", rc16->lalba);

		printf("Total size:%" PRIu64 "\n", rc16->block_length * (rc16->returned_lba + 1));
	}
	else
	{
		printf("%" PRIu64 "\n", rc16->block_length * (rc16->returned_lba + 1));
	}
	
	scsi_free_scsi_task(task);
	iscsi_destroy_url(iscsi_url);

	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return 0;
}

