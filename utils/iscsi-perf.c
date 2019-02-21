/* 
   Copyright (C) 2014-2015 by Peter Lieven <pl@kamp.de>
   
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <getopt.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#ifndef HAVE_CLOCK_GETTIME
#include <sys/time.h>
#endif

#define PERF_VERSION "0.1"

#define NOP_INTERVAL 5
#define MAX_NOP_FAILURES 3

const char *initiator = "iqn.2010-11.libiscsi:iscsi-perf";
int proc_alarm = 0;
int max_in_flight = 32;
int blocks_per_io = 8;
uint64_t runtime = 0;
uint64_t finished = 0;
int logging = 0;

struct client {
	int finished;
	int in_flight;
	int random;
	int random_blocks;

	struct iscsi_context *iscsi;
	struct scsi_iovec perf_iov;

	int lun;
	int blocksize;
	uint64_t num_blocks;
	uint64_t pos;
	uint64_t last_ns;
	uint64_t first_ns;
	uint64_t iops;
	uint64_t last_iops;
	uint64_t bytes;
	uint64_t last_bytes;

	int ignore_errors;
	int max_reconnects;
	int busy_cnt;
	int err_cnt;
	int retry_cnt;
};

uint64_t get_clock_ns(void) {
	int res;
	uint64_t ns;

#ifdef HAVE_CLOCK_GETTIME
	struct timespec ts;
	res = clock_gettime (CLOCK_MONOTONIC, &ts);
	ns = ts.tv_sec * 1000000000ULL + ts.tv_nsec;
#else
	struct timeval tv;
	res = gettimeofday(&tv, NULL);
	ns = tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
#endif
	if (res == -1) {
		fprintf(stderr,"could not get requested clock\n");
		exit(10);
	}
	return ns;
}

void fill_read_queue(struct client *client);

void progress(struct client *client) {
	uint64_t now = get_clock_ns();
	if (now - client->last_ns < 1000000000ULL) return;

	uint64_t _runtime = (now - client->first_ns) / 1000000000ULL;
	if (runtime) _runtime = runtime - _runtime;

	printf ("\r");
	uint64_t aiops = 1000000000.0 * (client->iops) / (now - client->first_ns);
	uint64_t ambps = 1000000000.0 * (client->bytes) / (now - client->first_ns);
	if (!_runtime) {
		finished = 1;
		printf ("iops average %" PRIu64 " (%" PRIu64 " MB/s)                                                        ", aiops, (aiops * blocks_per_io * client->blocksize) >> 20);
	} else {
		uint64_t iops = 1000000000ULL * (client->iops - client->last_iops) / (now - client->last_ns);
		uint64_t mbps = 1000000000ULL * (client->bytes - client->last_bytes) / (now - client->last_ns);
		printf ("%02" PRIu64 ":%02" PRIu64 ":%02" PRIu64 " - ", _runtime / 3600, (_runtime % 3600) / 60, _runtime % 60);
		printf ("lba %" PRIu64 ", iops current %" PRIu64 " (%" PRIu64 " MB/s), ", client->pos, iops, mbps >> 20);
		printf ("iops average %" PRIu64 " (%" PRIu64 " MB/s), in_flight %d, busy %d        ", aiops, ambps >> 20, client->in_flight, client->busy_cnt);
	}
	if (logging) {
		printf ("\n");
	}
	fflush(stdout);
	client->last_ns = now;
	client->last_iops = client->iops;
	client->last_bytes = client->bytes;
}

void cb(struct iscsi_context *iscsi _U_, int status, void *command_data, void *private_data)
{
	struct client *client = (struct client *)private_data;
	struct scsi_task *task = command_data, *task2 = NULL;
	struct scsi_read16_cdb *read16_cdb = NULL;

	read16_cdb = scsi_cdb_unmarshall(task, SCSI_OPCODE_READ16);
	if (read16_cdb == NULL) {
		fprintf(stderr, "Failed to unmarshall READ16 CDB.\n");
		client->err_cnt++;
		goto out;
	}

	if (status == SCSI_STATUS_BUSY ||
		(status == SCSI_STATUS_CHECK_CONDITION && task->sense.key == SCSI_SENSE_UNIT_ATTENTION)) {
		if (client->retry_cnt++ > 4 * max_in_flight) {
			fprintf(stderr, "maxium number of command retries reached...\n");
			client->err_cnt++;
			goto out;
		}
		task2 = iscsi_read16_task(client->iscsi,
								client->lun, read16_cdb->lba,
								read16_cdb->transfer_length * client->blocksize,
								client->blocksize, 0, 0, 0, 0, 0,
								cb, client);
		if (task2 == NULL) {
			fprintf(stderr, "failed to send read16 command\n");
			client->err_cnt++;
		}
		scsi_task_set_iov_in(task2, &client->perf_iov, 1);
		if (status == SCSI_STATUS_BUSY) {
			client->busy_cnt++;
		}
	} else if (status == SCSI_STATUS_CANCELLED) {
		client->err_cnt++;
	} else if (status == SCSI_STATUS_GOOD) {
		client->retry_cnt = 0;
		client->bytes += read16_cdb->transfer_length * client->blocksize;
	} else {
		fprintf(stderr, "Read16 failed with %s\n", iscsi_get_error(iscsi));
		if (!client->ignore_errors) {
			client->err_cnt++;
		}
	}

out:
	scsi_free_scsi_task(task);
	
	if (!client->err_cnt) {
		progress(client);
		client->iops++;
		client->in_flight--;
		fill_read_queue(client);
	}
}


void fill_read_queue(struct client *client)
{
	int64_t num_blocks;

	if (finished) return;

	if (client->pos >= client->num_blocks) client->pos = 0;
	while(client->in_flight < max_in_flight && client->pos < client->num_blocks) {
		struct scsi_task *task;
		client->in_flight++;

		if (client->random) {
			client->pos = rand() % client->num_blocks;
		}

		num_blocks = client->num_blocks - client->pos;
		if (num_blocks > blocks_per_io) {
			num_blocks = blocks_per_io;
		}
		
		if (client->random_blocks) {
			num_blocks = rand() % num_blocks + 1;
		}

		task = iscsi_read16_task(client->iscsi,
								client->lun, client->pos,
								(uint32_t)(num_blocks * client->blocksize),
								client->blocksize, 0, 0, 0, 0, 0,
								cb, client);
		if (task == NULL) {
			fprintf(stderr, "failed to send read16 command\n");
			iscsi_destroy_context(client->iscsi);
			exit(10);
		}
		scsi_task_set_iov_in(task, &client->perf_iov, 1);
		client->pos += num_blocks;
	}
}

void usage(void) {
	fprintf(stderr,"Usage: iscsi-perf [-i <initiator-name>] [-m <max_requests>] [-b blocks_per_request] [-t timeout] [-r|--random] [-l|--logging] [-n|--ignore-errors] [-x <max_reconnects>] <LUN>\n");
	exit(1);
}

void sig_handler (int signum ) {
	if (signum == SIGALRM) {
		if (proc_alarm) {
			fprintf(stderr, "\n\nABORT: Last alarm was not processed.\n");
			exit(10);
		}
		proc_alarm = 1;
		alarm(NOP_INTERVAL);
	} else {
		finished++;
	}
}

int main(int argc, char *argv[])
{
	char *url = NULL;
	struct iscsi_url *iscsi_url;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int c;
	struct pollfd pfd[1];
	struct client client;

	static struct option long_options[] = {
		{"initiator-name", required_argument,    NULL,        'i'},
		{"max",            required_argument,    NULL,        'm'},
		{"blocks",         required_argument,    NULL,        'b'},
		{"runtime",        required_argument,    NULL,        't'},
		{"random",         no_argument,          NULL,        'r'},
		{"random-blocks",  no_argument,          NULL,        'R'},
		{"logging",        no_argument,          NULL,        'l'},
		{"ignore-errors",  no_argument,          NULL,        'n'},
		{0, 0, 0, 0}
	};
	int option_index;

	memset(&client, 0, sizeof(client));
	client.max_reconnects = -1;

	srand(time(NULL));
	
	printf("iscsi-perf version %s - (c) 2014-2015 by Peter Lieven <pl@ĸamp.de>\n\n", PERF_VERSION);

	while ((c = getopt_long(argc, argv, "i:m:b:t:lnrRx:", long_options,
			&option_index)) != -1) {
		switch (c) {
		case 'i':
			initiator = optarg;
			break;
		case 'm':
			max_in_flight = atoi(optarg);
			break;
		case 't':
			runtime = atoi(optarg);
			break;
		case 'b':
			blocks_per_io = atoi(optarg);
			break;
		case 'n':
			client.ignore_errors = 1;
			break;
		case 'r':
			client.random = 1;
			break;
		case 'R':
			client.random_blocks = 1;
			break;
		case 'l':
			logging = 1;
			break;
		case 'x':
			client.max_reconnects = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Unrecognized option '%c'\n\n", c);
			usage();
		}
	}

	if (optind != argc -1 ) usage();

	if (argv[optind] != NULL) {
		url = strdup(argv[optind]);
	}

	if (url == NULL) usage();

	client.iscsi = iscsi_create_context(initiator);
	if (client.iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(10);
	}

	iscsi_url = iscsi_parse_full_url(client.iscsi, url);
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n",
			iscsi_get_error(client.iscsi));
		exit(10);
	}

	iscsi_set_session_type(client.iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(client.iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_full_connect_sync(client.iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(client.iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(client.iscsi);
		exit(10);
	}

	printf("connected to %s\n", url);
	free(url);

	client.lun = iscsi_url->lun;
	iscsi_destroy_url(iscsi_url);

	task = iscsi_readcapacity16_sync(client.iscsi, client.lun);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "failed to send readcapacity command\n");
		exit(10);
	}

	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		fprintf(stderr, "failed to unmarshall readcapacity16 data\n");
		exit(10);
	}

	client.blocksize  = rc16->block_length;
	client.num_blocks  = rc16->returned_lba + 1;

	scsi_free_scsi_task(task);

	client.perf_iov.iov_base = malloc(blocks_per_io * client.blocksize);
	if (!client.perf_iov.iov_base) {
		fprintf(stderr, "Out of Memory\n");
		exit(10);
	}
	client.perf_iov.iov_len = blocks_per_io * client.blocksize;

	printf("capacity is %" PRIu64 " blocks or %" PRIu64 " byte (%" PRIu64 " MB)\n", client.num_blocks, client.num_blocks * client.blocksize,
	                                                        (client.num_blocks * client.blocksize) >> 20);

	printf("performing %s READ with %d parallel requests\n", client.random ? "RANDOM" : "SEQUENTIAL", max_in_flight);

	if (client.random_blocks) {
		printf("RANDOM transfer size of 1 - %d blocks (%d - %d byte)\n", blocks_per_io, client.blocksize, blocks_per_io * client.blocksize);
	} else {
		printf("FIXED transfer size of %d blocks (%d byte)\n", blocks_per_io, blocks_per_io * client.blocksize);
	}

	if (runtime) {
		printf("will run for %" PRIu64 " seconds.\n", runtime);
	} else {
		printf("infinite runtime - press CTRL-C to abort.\n");
	}

	struct sigaction sa;
	sa.sa_handler = &sig_handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask); 

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);

	printf("\n");

	client.first_ns = client.last_ns = get_clock_ns();

	iscsi_set_reconnect_max_retries(client.iscsi, client.max_reconnects);

	fill_read_queue(&client);

	alarm(NOP_INTERVAL);

	while (client.in_flight && !client.err_cnt && finished < 2) {
		pfd[0].fd = iscsi_get_fd(client.iscsi);
		pfd[0].events = iscsi_which_events(client.iscsi);

		if (proc_alarm) {
			if (iscsi_get_nops_in_flight(client.iscsi) > MAX_NOP_FAILURES) {
				iscsi_reconnect(client.iscsi);
			} else {
				iscsi_nop_out_async(client.iscsi, NULL, NULL, 0, NULL);
			}
			if (!iscsi_get_nops_in_flight(client.iscsi)) {
				finished = 0;
			}
			proc_alarm = 0;
		}

		if (!pfd[0].events) {
			sleep(1);
			continue;
		}

		if (poll(&pfd[0], 1, -1) < 0) {
			continue;
		}
		if (iscsi_service(client.iscsi, pfd[0].revents) < 0) {
			fprintf(stderr, "iscsi_service failed with : %s\n", iscsi_get_error(client.iscsi));
			break;
		}
	}
	
	alarm(0);

	progress(&client);
	
	if (!client.err_cnt && finished < 2) {
		printf ("\n\nfinished.\n");
		iscsi_logout_sync(client.iscsi);
	} else {
		printf ("\nABORTED!\n");
	}
	iscsi_destroy_context(client.iscsi);

	free(client.perf_iov.iov_base);

	return client.err_cnt ? 1 : 0;
}

