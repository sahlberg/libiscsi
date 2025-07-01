/*
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
#include <inttypes.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

void report_tpg(const struct scsi_report_target_port_groups *rtpg)
{
	int group, port;

	printf("RTPG retrieved %d groups\n", rtpg->num_groups);
	for(group = 0; group < rtpg->num_groups; ++group) {
		printf("Group 0x%04x: preferred %d, format 0x%02x, ALUA state %s,"
		       "flags 0x%02x, status code 0x%02x, port count %d\n",
		       rtpg->groups[group].port_group,
		       rtpg->groups[group].pref,
		       rtpg->groups[group].rtpg_fmt,
		       scsi_alua_state_to_str(rtpg->groups[group].alua_state),
		       rtpg->groups[group].flags,
		       rtpg->groups[group].status_code,
		       rtpg->groups[group].port_count);
		printf("Ports: [");
		for (port = 0; port < rtpg->groups[group].retrieved_port_count; ++port) {
			printf(" 0x%x", rtpg->groups[group].ports[port]);
		}
		printf("]\n");
	}
}

void do_rtpg(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	int alloc_size = 512, retries;
	struct scsi_report_target_port_groups *rtpg;

	for (retries = 0; retries < 2; ++retries) {
		int full_size;

		task = scsi_cdb_report_target_port_groups(alloc_size);
		if (task == NULL) {
			fprintf(stderr, "Failed to allocate CBD for RTPG (size %d)\n", full_size);
			exit(10);
		}

		task = iscsi_scsi_command_sync(iscsi, lun, task, NULL);
		if (task == NULL) {
			fprintf(stderr, "RTPG command failed\n");
			exit(10);
		}

		full_size = scsi_datain_getfullsize(task);
		if (full_size > alloc_size) {
			alloc_size = full_size;
			scsi_free_scsi_task(task);
			continue;
		}

		rtpg = scsi_datain_unmarshall(task);
		if (rtpg == NULL) {
			fprintf(stderr, "Failed to unmarshal RTPG data blob\n");
			exit(10);
		}

		report_tpg(rtpg);
		return;
	}

	fprintf(stderr,
	        "Gave up after 2 RTPG attempts: the report did not fit in %d bytes\n",
	        alloc_size);
	exit(10);
}

void print_usage(void)
{
	fprintf(stderr,
	        "Usage: iscsi-rtpg [-?|--help] [--usage] "
	        "[-i|--initiator-name=iqn-name] <iscsi-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-rtpg [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
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
	char *url = NULL;
	struct iscsi_url *iscsi_url = NULL;
	const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-inq";
	int show_help = 0, show_usage = 0, debug = 0;
	int c;

	static struct option long_options[] = {
		{"help",           no_argument,          NULL,        'h'},
		{"usage",          no_argument,          NULL,        'u'},
		{"debug",          required_argument,    NULL,        'd'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{0, 0, 0, 0}
	};
	int option_index;

	while ((c = getopt_long(argc, argv, "h?ud:i:", long_options,
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
			debug = strtol(optarg, NULL, 0);
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
		iscsi_set_log_level(iscsi, debug);
		iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
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

	free(url);

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

	do_rtpg(iscsi, iscsi_url->lun);
	iscsi_destroy_url(iscsi_url);

	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return 0;
}

