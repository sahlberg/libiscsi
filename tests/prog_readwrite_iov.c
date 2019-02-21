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

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:prog-readwrite-iov";

void print_usage(void)
{
	fprintf(stderr, "Usage: prog_readwrite_iov [-?|--help] [--usage] "
		"[-i|--initiator-name=iqn-name]\n"
		"\t\t<iscsi-portal-url>\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "This command is used to test reading/writing"
		"using iovectors\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: prog_readwrite_iov [OPTION...] <iscsi-url>\n");
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
	const char *url = NULL;
	static int show_help = 0, show_usage = 0, debug = 0;
        struct scsi_task *task;
        struct scsi_iovec iov[4];
        unsigned int i;
        int c;
        unsigned char *data;
	struct scsi_readcapacity10 *rc10;
        
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
		fprintf(stderr, "Failed to create context\n");
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

	task = iscsi_readcapacity10_sync(iscsi, iscsi_url->lun, 0, 0);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "failed to send readcapacity command\n");
		exit(10);
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL || rc10->block_size == 0) {
		fprintf(stderr, "failed to unmarshall readcapacity10 data\n");
		exit(10);
	}
	data = malloc(rc10->block_size);
        
        /* initialize data with a known pattern */
        for (i = 0; i < rc10->block_size; i++) {
                data[i] = i & 0xff;
        }
        /* Split the data into iovectors */
        iov[0].iov_base = data;
        iov[0].iov_len = 3;
        iov[1].iov_base = data + 3;
        iov[1].iov_len = 100;
        iov[2].iov_base = data + 103;
        iov[2].iov_len = 153;
        iov[3].iov_base = data + 256;
        iov[3].iov_len = rc10->block_size - 256;
        
        task = iscsi_write16_iov_sync(iscsi, iscsi_url->lun, 0, NULL,
                                      rc10->block_size, rc10->block_size,
                                      0, 0, 0, 0, 0,
                                      iov, 4);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
                fprintf(stderr, "Failed to send WRITE16\n");
                exit(10);
        }

        memset(data, 0, rc10->block_size);

        /* Use different iovectors to ead the data back */
        iov[0].iov_base = data;
        iov[0].iov_len = 16;
        iov[1].iov_base = data + 16;
        iov[1].iov_len = 16;
        iov[2].iov_base = data + 32;
        iov[2].iov_len = 32;
        iov[3].iov_base = data + 64;
        iov[3].iov_len = rc10->block_size - 64;

        task = iscsi_read16_iov_sync(iscsi, iscsi_url->lun, 0,
                                     rc10->block_size, rc10->block_size,
                                     0, 0, 0, 0, 0,
                                     iov, 4);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
                fprintf(stderr, "Failed to send READ16\n");
                exit(10);
        }

        /* verify the data */
        for (i = 0; i < rc10->block_size; i++) {
                if (data[i] != (i & 0xff)) {
                        fprintf(stderr, "Data mismatch\n");
                        exit(10);
                }
        }

        return 0;
}

