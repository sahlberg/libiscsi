/*
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef AROS
#include "aros/aros_compat.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

int showluns;
const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-ls";

struct client_state {
       int finished;
       int status;
       int lun;
       int type;
       const char *username;
       const char *password;
};


void event_loop(struct iscsi_context *iscsi, struct client_state *state)
{
       struct pollfd pfd;

       while (state->finished == 0) {
               pfd.fd = iscsi_get_fd(iscsi);
               pfd.events = iscsi_which_events(iscsi);

               if (poll(&pfd, 1, -1) < 0) {
                       fprintf(stderr, "Poll failed");
                       exit(10);
               }
               if (iscsi_service(iscsi, pfd.revents) < 0) {
                       fprintf(stderr, "iscsi_service failed with : %s\n", iscsi_get_error(iscsi));
                       exit(10);
               }
       }
}

void show_lun(struct iscsi_context *iscsi, int lun)
{
	struct scsi_task *task;
	struct scsi_inquiry_standard *inq;
	int type, no_media;
	long long size = 0;
	int size_pf = 0;
	static const char sf[] = {' ', 'k', 'M', 'G', 'T' };

	/* check we can talk to the lun */
tur_try_again:
	if ((task = iscsi_testunitready_sync(iscsi, lun)) == NULL) {
		fprintf(stderr, "testunitready failed\n");
		exit(10);
	}
	if (task->status == SCSI_STATUS_CHECK_CONDITION) {
		if (task->sense.key == SCSI_SENSE_UNIT_ATTENTION && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET) {
			scsi_free_scsi_task(task);
			goto tur_try_again;
		}
	}

	no_media = 0;
	if (task->status     == SCSI_STATUS_CHECK_CONDITION
	&&  task->sense.key  == SCSI_SENSE_NOT_READY
	&&  task->sense.ascq == SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT) {
	    	/* not an error, just a cdrom without a disk most likely */
		no_media = 1;
	} else if (task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "TESTUNITREADY failed with %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
	scsi_free_scsi_task(task);



	/* check what type of lun we have */
	task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "failed to send inquiry command : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		fprintf(stderr, "failed to unmarshall inquiry datain blob\n");
		exit(10);
	}
	type = inq->device_type;
	scsi_free_scsi_task(task);



	if (type == SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		struct scsi_readcapacity10 *rc10;

		task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "failed to send readcapacity command\n");
			exit(10);
		}

		rc10 = scsi_datain_unmarshall(task);
		if (rc10 == NULL) {
			fprintf(stderr, "failed to unmarshall readcapacity10 data\n");
			exit(10);
		}

		size  = rc10->block_size;
		size *= rc10->lba;

		for (size_pf=0; size_pf<4 && size > 1024; size_pf++) {
			size /= 1024;
		}

		scsi_free_scsi_task(task);
	}


	printf("Lun:%-4d Type:%s", lun, scsi_devtype_to_str(type));
	if (type == SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
		printf(" (Size:%lld%c)", size, sf[size_pf]);
	}
	if (no_media) {
		printf(" (No media loaded)");
	}
	printf("\n");
}

void list_luns(struct client_state *clnt, const char *target, const char *portal)
{
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_reportluns_list *list;
	int full_report_size;
	int i;

	iscsi = iscsi_create_context(initiator);
	if (iscsi == NULL) {
		printf("Failed to create context\n");
		exit(10);
	}

	iscsi_set_initiator_username_pwd(iscsi, clnt->username, clnt->password);

	if (iscsi_set_targetname(iscsi, target)) {
		fprintf(stderr, "Failed to set target name\n");
		exit(10);
	}
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_full_connect_sync(iscsi, portal, -1) != 0) {
		printf("iscsi_connect failed. %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	/* get initial reportluns data, all targets can report 16 bytes but some
	 * fail if we ask for too much.
	 */
	if ((task = iscsi_reportluns_sync(iscsi, 0, 16)) == NULL) {
		fprintf(stderr, "reportluns failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
	full_report_size = scsi_datain_getfullsize(task);
	if (full_report_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_reportluns_sync(iscsi, 0, full_report_size)) == NULL) {
			fprintf(stderr, "reportluns failed : %s\n", iscsi_get_error(iscsi));
			exit(10);
		}
	}

	list = scsi_datain_unmarshall(task);
	if (list == NULL) {
		fprintf(stderr, "failed to unmarshall reportluns datain blob\n");
		exit(10);
	}
	for (i=0; i < (int)list->num; i++) {
		show_lun(iscsi, list->luns[i]);
	}

	scsi_free_scsi_task(task);
	iscsi_destroy_context(iscsi);
}




void discoverylogout_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	struct client_state *state = (struct client_state *)private_data;

	if (status != 0) {
		fprintf(stderr, "Failed to logout from target. : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	if (iscsi_disconnect(iscsi) != 0) {
		fprintf(stderr, "Failed to disconnect old socket\n");
		exit(10);
	}

	state->finished = 1;
}

void discovery_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct iscsi_discovery_address *addr;

	if (status != 0) {
		fprintf(stderr, "Failed to do discovery on target. : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	for(addr=command_data; addr; addr=addr->next) {
		printf("Target:%s Portal:%s\n", addr->target_name, addr->target_address);
		if (showluns != 0) {
			list_luns(private_data, addr->target_name, addr->target_address);
		}
	}

	if (iscsi_logout_async(iscsi, discoverylogout_cb, private_data) != 0) {
		fprintf(stderr, "iscsi_logout_async failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}


void discoverylogin_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	if (status != 0) {
		fprintf(stderr, "Login failed. %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	if (iscsi_discovery_async(iscsi, discovery_cb, private_data) != 0) {
		fprintf(stderr, "failed to send discovery command : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}

void discoveryconnect_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	if (status != 0) {
		fprintf(stderr, "discoveryconnect_cb: connection failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	if (iscsi_login_async(iscsi, discoverylogin_cb, private_data) != 0) {
		fprintf(stderr, "iscsi_login_async failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}

void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-ls [-?|--help] [--usage] [-i|--initiator-name=iqn-name]\n"
			"\t\t[-s|--show-luns] <iscsi-portal-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-ls [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -s, --show-luns                   Show the luns for each target\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Help options:\n");
	fprintf(stderr, "  -?, --help                        Show this help message\n");
	fprintf(stderr, "      --usage                       Display brief usage message\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "iSCSI Portal URL format : %s\n", ISCSI_PORTAL_URL_SYNTAX);
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
	int c;
	static int show_help = 0, show_usage = 0, debug = 0;

	while ((c = getopt(argc, argv, "h?udi:s")) != -1) {
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
			showluns = 1;
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

	url = strdup(argv[optind]);
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

	iscsi_url = iscsi_parse_portal_url(iscsi, url);

	if (url) {
		free(discard_const(url));
	}

	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}

	iscsi_set_session_type(iscsi, ISCSI_SESSION_DISCOVERY);

	state.username = iscsi_url->user;
	state.password = iscsi_url->passwd;

	if (iscsi_connect_async(iscsi, iscsi_url->portal, discoveryconnect_cb, &state) != 0) {
		fprintf(stderr, "iscsi_connect failed. %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	event_loop(iscsi, &state);

	iscsi_destroy_url(iscsi_url);
	iscsi_destroy_context(iscsi);
	return 0;
}

