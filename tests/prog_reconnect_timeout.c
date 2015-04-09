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

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:prog-reconnect-timeout";

struct client_state {
       int finished;
       int status;
       int lun;
       int concurrency;
       int read_pos;
       int num_remaining;
       uint32_t block_size;
       uint32_t num_lbas;
       char portal[MAX_STRING_SIZE];
       int got_zero_events;
};

struct read16_state {
       uint32_t lba;
       struct client_state *client;
};

void event_loop(struct iscsi_context *iscsi, struct client_state *state)
{
	struct pollfd pfd;

	while (state->finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);
	       
		if (pfd.events == 0) {
			state->got_zero_events = 1;
			printf("iscsi_which_events() returned 0\n");
			sleep(1);
			printf("change portal back to the right portal\n");
			iscsi_full_connect_async(iscsi, state->portal,
						 state->lun, NULL, NULL);
		}
		if (poll(&pfd, 1, -1) < 0) {
			fprintf(stderr, "Poll failed");
			exit(10);
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			fprintf(stderr, "iscsi_service failed with : %s\n",
				iscsi_get_error(iscsi));
			exit(10);
		}
       }
}

void logout_cb(struct iscsi_context *iscsi, int status,
	       void *command_data _U_, void *private_data)
{
	struct client_state *state = (struct client_state *)private_data;

	if (status != 0) {
		fprintf(stderr, "Failed to logout from target. : %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}

	if (iscsi_disconnect(iscsi) != 0) {
		fprintf(stderr, "Failed to disconnect old socket\n");
		exit(10);
	}

	state->finished = 1;
}

void read_cb(struct iscsi_context *iscsi, int status,
	      void *command_data, void *private_data)
{
	struct read16_state *r16_state = private_data;
	struct client_state *state = r16_state->client;
	struct scsi_task *task = command_data;

	printf("READ returned for LBA %d\n", (int)r16_state->lba);
	if (status == SCSI_STATUS_CHECK_CONDITION &&
	    task->sense.key == SCSI_SENSE_UNIT_ATTENTION) {
		printf("Received UNIT_ATTENTION. Ignoring.\n");
	} else if (status != 0) {
		fprintf(stderr, "READ16 failed. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		exit(10);
	}

	if (state->read_pos == 6) {
		char buf[256];
		ssize_t count;

		printf("write garbage to the socket to trigger a server "
		       "disconnect\n");
		memset(buf, 0, sizeof(buf));

		/* If we do a full connect on an already connected context
		 * we will update the iscsi->portal field but aotherwise
		 * fail the command completely.
		 * This should trigger the reconnect to fail to establish
		 * a connection and thus lead to iscsi_which_events()
		 * returning 0, signaling a 'wait and try again later'.
		 */
		printf("change portal to point to a closed socket\n");
		iscsi_full_connect_async(iscsi, "127.0.0.1:1", state->lun,
					 NULL, NULL);

		count = write(iscsi_get_fd(iscsi), buf, sizeof(buf));
		if (count < (ssize_t)sizeof(buf)) {
			fprintf(stderr, "write failed.\n");
			scsi_free_scsi_task(task);
			exit(10);
		}
	}
	free(r16_state);
	scsi_free_scsi_task(task);

	if (state->num_remaining > state->concurrency) {
		r16_state = malloc(sizeof(struct read16_state));
		r16_state->lba = state->read_pos++;
		r16_state->client = state;

		printf("SENT READ for LBA %d\n", r16_state->lba);
		if (iscsi_read16_task(iscsi,
				      state->lun, r16_state->lba,
				      state->block_size,
				      state->block_size, 0, 0, 0, 0, 0,
				      read_cb, r16_state) == NULL) {
			fprintf(stderr, "iscsi_read16_task failed : %s\n",
				iscsi_get_error(iscsi));
			exit(10);
		}
	}

	if (--state->num_remaining) {
		return;
	}

	if (iscsi_logout_async(iscsi, logout_cb, state) != 0) {
		fprintf(stderr, "iscsi_logout_async failed : %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}
}

void print_usage(void)
{
	fprintf(stderr, "Usage: prog_reconnect_timeout [-?|--help] [--usage] "
		"[-i|--initiator-name=iqn-name]\n"
		"\t\t<iscsi-portal-url>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "This command is used to test that during reconnect,"
		"if we fail to connect the TCP socket then "
		"iscsi_which_events() will return 0 to signal "
		"'no events right now, wait a while and try again'\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: prog_reconnect_timeout [OPTION...] <iscsi-url>\n");
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

int main(int argc, char *argv[])
{
	struct iscsi_context *iscsi;
	struct iscsi_url *iscsi_url = NULL;
	struct client_state state;
	const char *url = NULL;
	int i, c;
	static int show_help = 0, show_usage = 0, debug = 0;
	struct scsi_readcapacity10 *rc10;
	struct scsi_task *task;

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

	memset(&state, 0, sizeof(state));

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

	state.lun = iscsi_url->lun;
	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun)
	    != 0) {
		fprintf(stderr, "iscsi_connect failed. %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}
	task = iscsi_readcapacity10_sync(iscsi, iscsi_url->lun, 0, 0);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "failed to send readcapacity command\n");
		exit(10);
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		fprintf(stderr, "failed to unmarshall readcapacity10 data\n");
		exit(10);
	}
	state.block_size = rc10->block_size;
	state.num_lbas = rc10->lba;
	scsi_free_scsi_task(task);

	state.num_remaining = 10;
	state.concurrency = 3;
	strncpy(state.portal, iscsi_url->portal, MAX_STRING_SIZE);

	/* Queue up a bunch of READ16 calls and then send more are
	 * the replies come trickling in. Once all num_remaining
	 * reads have been processed we will log out and end the test.
	 */
	for (i = 0; i < state.concurrency; i++) {
		struct read16_state *r16_state;
		r16_state = malloc(sizeof(struct read16_state));
		r16_state->lba = state.read_pos++;
		r16_state->client = &state;
		printf("SENT READ for LBA %d\n", r16_state->lba);
		if (iscsi_read16_task(iscsi,
				      state.lun,
				      r16_state->lba,
				      state.block_size,
				      state.block_size, 0, 0, 0, 0, 0,
				      read_cb, r16_state) == NULL) {
			fprintf(stderr, "iscsi_read16_task failed : %s\n",
				iscsi_get_error(iscsi));
			exit(10);
		}
	}

	event_loop(iscsi, &state);

	iscsi_destroy_url(iscsi_url);
	iscsi_destroy_context(iscsi);

	if (state.got_zero_events != 1) {
		fprintf(stderr, "iscsi_which_events() never returned 0\n");
		exit(10);
	}
	return 0;
}

