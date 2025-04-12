/* 
   Copyright (C) 2025 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"

static int finished;

/*
 * Number of iscsi connections to the server.
 */
#define NUM_CONTEXTS 2

/*
 * Number of threads parallely read device data.
 * Usually one thread per context should be sufficient, but here we use more
 * threads to demonstrate that multiple threads can very well write to the same
 * context.
 */
#define NUM_THREADS 4


const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-inq";


void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-pthreads-readloop [-?] [-?|--help] [--usage] [-i|--initiator-name=iqn-name]\n"
			"\t\t<iscsi-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-pthreads-readloop [OPTION...] <iscsi-url>\n");
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

struct read_data {
        struct iscsi_context *iscsi;
        int lun;
        int i;
        pthread_t thread;
};                

static void *iscsi_read_thread(void *arg)
{
        struct read_data *rd = arg;
        struct scsi_task *task;

        while (!finished) {
                task = iscsi_read10_sync(rd->iscsi, rd->lun, 0,
                                         512, 512,
                                         0, 0, 0, 0, 0);
                printf("task:%p\n", task);
                scsi_free_scsi_task(task);
                if (task == NULL) {
                        fprintf(stderr, "Failed to read from lun in thread #%d\n", rd->i);
                        exit(10);
                }
        }
        return NULL;
}

static void sig_alarm(int sig)
{
        finished = 1;
}

int main(int argc, char *argv[])
{
	struct iscsi_context *iscsi[NUM_CONTEXTS] = {NULL,};
	char *url = NULL;
	struct iscsi_url *iscsi_url = NULL;
	int show_help = 0, show_usage = 0, debug = 0;
	int c, i;
        struct read_data *rd;
        
	static struct option long_options[] = {
		{"help",           no_argument,          NULL,        'h'},
		{"usage",          no_argument,          NULL,        'u'},
		{"debug",          required_argument,    NULL,        'd'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{0, 0, 0, 0}
	};
	int option_index;
        
	while ((c = getopt_long(argc, argv, "h?ud:i:e:c:", long_options,
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

	for (i = 0; i < NUM_CONTEXTS; i++) {
                printf("Creating context #%d\n", i);
                iscsi[i] = iscsi_create_context(initiator);
                if (iscsi[i] == NULL) {
                        fprintf(stderr, "Failed to create context\n");
                        exit(10);
                }
                if (debug > 0) {
                        iscsi_set_log_level(iscsi[i], debug);
                        iscsi_set_log_fn(iscsi[i], iscsi_log_to_stderr);
                }

                if (i > 0) {
                        iscsi_destroy_url(iscsi_url);
                }        
                if (argv[optind] != NULL) {
                        url = strdup(argv[optind]);
                }
                if (url == NULL) {
                        fprintf(stderr, "You must specify the URL\n");
                        print_usage();
                        exit(10);
                }
                iscsi_url = iscsi_parse_full_url(iscsi[i], url);
                
                free(url);
                
                if (iscsi_url == NULL) {
                        fprintf(stderr, "Failed to parse URL: %s\n", 
                                iscsi_get_error(iscsi[i]));
                        exit(10);
                }

                iscsi_set_session_type(iscsi[i], ISCSI_SESSION_NORMAL);
                iscsi_set_header_digest(iscsi[i], ISCSI_HEADER_DIGEST_NONE_CRC32C);

                if (iscsi_full_connect_sync(iscsi[i], iscsi_url->portal, iscsi_url->lun) != 0) {
                        fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi[i]));
                        iscsi_destroy_url(iscsi_url);
                        iscsi_destroy_context(iscsi[i]);
                        exit(10);
                }

                if (iscsi_mt_service_thread_start(iscsi[i])) {
                        fprintf(stderr, "failed to start service thread #%d\n", i);
                        exit(10);
                }

        }

	if ((rd = malloc(sizeof(struct read_data) * NUM_THREADS)) == NULL) {
		fprintf(stderr, "Failed to allocated read_data\n");
                exit(10);
        }
        for (i = 0; i < NUM_THREADS; i++) {
                rd[i].iscsi = iscsi[i % NUM_CONTEXTS];
		rd[i].lun = iscsi_url->lun;
                rd[i].i = i;
                if (pthread_create(&rd[i].thread, NULL,
                                   &iscsi_read_thread, &rd[i])) {
                        printf("Failed to create read thread #%d\n", i);
                        exit(10);
                }
	}

        /* run for 5 seconds */
	signal(SIGALRM, sig_alarm);
        alarm(5);
        
	/*
	 * Wait for all threads to complete
	 */
        for (i = 0; i < NUM_THREADS; i++) {
                pthread_join(rd[i].thread, NULL);
        }
        
	iscsi_destroy_url(iscsi_url);

	for (i = 0; i < NUM_CONTEXTS; i++) {
                iscsi_mt_service_thread_stop(iscsi[i]);
                iscsi_logout_sync(iscsi[i]);
                iscsi_destroy_context(iscsi[i]);
        }
	return 0;
}

