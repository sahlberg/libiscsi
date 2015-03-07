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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <getopt.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

const char *initiator = "iqn.2010-11.ronnie:iscsi-inq";
int max_in_flight = 50;
int blocks_per_io = 200;

struct client {
	int finished;
	int in_flight;

	struct iscsi_context *src_iscsi;
	int src_lun;
	int src_blocksize;
	uint64_t src_num_blocks;
	uint64_t pos;

	struct iscsi_context *dst_iscsi;
	int dst_lun;
	int dst_blocksize;
	uint64_t dst_num_blocks;
	int use_16_for_rw;
	int progress;
	int ignore_errors;
};


void fill_read_queue(struct client *client);

struct write_task {
       struct scsi_task *rt;
       struct client *client;
};

void write_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct write_task *wt = (struct write_task *)private_data;
	struct scsi_task *task = command_data;
	struct client *client = wt->client;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Write10/16 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	if (status != SCSI_STATUS_GOOD) {
		printf("Write10/16 failed with %s\n", iscsi_get_error(iscsi));
		if (!client->ignore_errors) {
			scsi_free_scsi_task(task);
			exit(10);
		}
	}

	client->in_flight--;
	fill_read_queue(client);

	if (client->progress) {
		printf("\r%"PRIu64" of %"PRIu64" blocks transferred.", client->pos, client->src_num_blocks);
	}

	if ((client->in_flight == 0) && (client->pos == client->src_num_blocks)) {
		client->finished = 1;
		if (client->progress) {
			printf("\n");
		}
	}
	scsi_free_scsi_task(wt->rt);
	scsi_free_scsi_task(task);
	free(wt);
}

void read_cb(struct iscsi_context *iscsi _U_, int status, void *command_data, void *private_data)
{
	struct client *client = (struct client *)private_data;
	struct scsi_task *task = command_data;
	struct write_task *wt;
	struct scsi_read10_cdb *read10_cdb = NULL;
	struct scsi_read16_cdb *read16_cdb = NULL;
	struct scsi_task *task2;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Read10/16 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	if (status != SCSI_STATUS_GOOD) {
		printf("Read10/16 failed with %s\n", iscsi_get_error(iscsi));
		if (!client->ignore_errors) {
			scsi_free_scsi_task(task);
			exit(10);
		}
	}

	wt = malloc(sizeof(struct write_task));
	wt->rt = task;
	wt->client = client;

	if (client->use_16_for_rw) {
		read16_cdb = scsi_cdb_unmarshall(task, SCSI_OPCODE_READ16);
		if (read16_cdb == NULL) {
			printf("Failed to unmarshall READ16 CDB.\n");
			exit(10);
		}
		task2 = iscsi_write16_task(client->dst_iscsi, client->dst_lun,
									read16_cdb->lba, task->datain.data, task->datain.size,
									client->dst_blocksize, 0, 0, 0, 0, 0,
									write_cb, wt);
	} else {
		read10_cdb = scsi_cdb_unmarshall(task, SCSI_OPCODE_READ10);
		if (read10_cdb == NULL) {
			printf("Failed to unmarshall READ16 CDB.\n");
			exit(10);
		}
		task2 = iscsi_write10_task(client->dst_iscsi, client->dst_lun,
									read10_cdb->lba, task->datain.data, task->datain.size,
									client->dst_blocksize, 0, 0, 0, 0, 0,
									write_cb, wt);
	}
	if (task2 == NULL) {
		printf("failed to send read16 command\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
}


void fill_read_queue(struct client *client)
{
	int num_blocks;

	while(client->in_flight < max_in_flight && client->pos < client->src_num_blocks) {
		struct scsi_task *task;
		client->in_flight++;

		num_blocks = client->src_num_blocks - client->pos;
		if (num_blocks > blocks_per_io) {
			num_blocks = blocks_per_io;
		}

		if (client->use_16_for_rw) {
			task = iscsi_read16_task(client->src_iscsi,
									client->src_lun, client->pos,
									num_blocks * client->src_blocksize,
									client->src_blocksize, 0, 0, 0, 0, 0,
									read_cb, client);
		} else {
			task = iscsi_read10_task(client->src_iscsi,
									client->src_lun, client->pos,
									num_blocks * client->src_blocksize,
									client->src_blocksize, 0, 0, 0, 0, 0,
									read_cb, client);
		}
		if (task == NULL) {
			printf("failed to send read10/16 command\n");
			exit(10);
		}
		client->pos += num_blocks;
	}
}

int main(int argc, char *argv[])
{
	char *src_url = NULL;
	char *dst_url = NULL;
	struct iscsi_url *iscsi_url;
	struct scsi_task *task;
	struct scsi_readcapacity10 *rc10;
	struct scsi_readcapacity16 *rc16;
	int c;
	struct pollfd pfd[2];
	struct client client;

	static struct option long_options[] = {
		{"dst",            required_argument,    NULL,        'd'},
		{"src",            required_argument,    NULL,        's'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{"progress",       no_argument,          NULL,        'p'},
		{"16",             no_argument,          NULL,        '6'},
		{"max",            required_argument,    NULL,        'm'},
		{"blocks",         required_argument,    NULL,        'b'},
		{"ignore-errors",  no_argument,          NULL,        'n'},
		{0, 0, 0, 0}
	};
	int option_index;

	memset(&client, 0, sizeof(client));

	while ((c = getopt_long(argc, argv, "d:s:i:m:b:p6n", long_options,
			&option_index)) != -1) {
		switch (c) {
		case 'd':
			dst_url = optarg;
			break;
		case 's':
			src_url = optarg;
			break;
		case 'i':
			initiator = optarg;
			break;
		case 'p':
			client.progress = 1;
			break;
		case '6':
			client.use_16_for_rw = 1;
			break;
		case 'm':
			max_in_flight = atoi(optarg);
			break;
		case 'b':
			blocks_per_io = atoi(optarg);
			break;
		case 'n':
			client.ignore_errors = 1;
			break;
		default:
			fprintf(stderr, "Unrecognized option '%c'\n\n", c);
			exit(1);
		}
	}

	if (src_url == NULL) {
		fprintf(stderr, "You must specify source url\n");
		fprintf(stderr, "  --src iscsi://<host>[:<port>]/<target-iqn>/<lun>\n");
		exit(10);
	}
	if (dst_url == NULL) {
		fprintf(stderr, "You must specify destination url\n");
		fprintf(stderr, "  --dst iscsi://<host>[:<port>]/<target-iqn>/<lun>\n");
		exit(10);
	}

	client.src_iscsi = iscsi_create_context(initiator);
	if (client.src_iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(10);
	}
	iscsi_url = iscsi_parse_full_url(client.src_iscsi, src_url);
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n",
			iscsi_get_error(client.src_iscsi));
		exit(10);
	}
	iscsi_set_targetname(client.src_iscsi, iscsi_url->target);
	iscsi_set_session_type(client.src_iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(client.src_iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
	if (iscsi_url->user[0] != '\0') {
		if (iscsi_set_initiator_username_pwd(client.src_iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
			fprintf(stderr, "Failed to set initiator username and password\n");
			exit(10);
		}
	}
	if (iscsi_full_connect_sync(client.src_iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(client.src_iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(client.src_iscsi);
		exit(10);
	}
	client.src_lun = iscsi_url->lun;
	iscsi_destroy_url(iscsi_url);

	if (client.use_16_for_rw) {
		task = iscsi_readcapacity16_sync(client.src_iscsi, client.src_lun);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "failed to send readcapacity command\n");
			exit(10);
		}
		rc16 = scsi_datain_unmarshall(task);
		if (rc16 == NULL) {
			fprintf(stderr, "failed to unmarshall readcapacity16 data\n");
			exit(10);
		}
		client.src_blocksize  = rc16->block_length;
		client.src_num_blocks  = rc16->returned_lba + 1;
		scsi_free_scsi_task(task);
	} else {
		task = iscsi_readcapacity10_sync(client.src_iscsi, client.src_lun, 0, 0);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "failed to send readcapacity command\n");
			exit(10);
		}
		rc10 = scsi_datain_unmarshall(task);
		if (rc10 == NULL) {
			fprintf(stderr, "failed to unmarshall readcapacity10 data\n");
			exit(10);
		}
		client.src_blocksize  = rc10->block_size;
		client.src_num_blocks  = rc10->lba;
		scsi_free_scsi_task(task);
	}

	client.dst_iscsi = iscsi_create_context(initiator);
	if (client.dst_iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(10);
	}
	iscsi_url = iscsi_parse_full_url(client.dst_iscsi, dst_url);
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n",
			iscsi_get_error(client.dst_iscsi));
		exit(10);
	}
	iscsi_set_targetname(client.dst_iscsi, iscsi_url->target);
	iscsi_set_session_type(client.dst_iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(client.dst_iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
	if (iscsi_url->user[0] != '\0') {
		if (iscsi_set_initiator_username_pwd(client.dst_iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
			fprintf(stderr, "Failed to set initiator username and password\n");
			exit(10);
		}
	}
	if (iscsi_full_connect_sync(client.dst_iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(client.dst_iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(client.dst_iscsi);
		exit(10);
	}
	client.dst_lun = iscsi_url->lun;
	iscsi_destroy_url(iscsi_url);

	if (client.use_16_for_rw) {
		task = iscsi_readcapacity16_sync(client.dst_iscsi, client.dst_lun);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "failed to send readcapacity command\n");
			exit(10);
		}
		rc16 = scsi_datain_unmarshall(task);
		if (rc16 == NULL) {
			fprintf(stderr, "failed to unmarshall readcapacity16 data\n");
			exit(10);
		}
		client.dst_blocksize  = rc16->block_length;
		client.dst_num_blocks  = rc16->returned_lba + 1;
		scsi_free_scsi_task(task);
	} else {
		task = iscsi_readcapacity10_sync(client.dst_iscsi, client.dst_lun, 0, 0);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "failed to send readcapacity command\n");
			exit(10);
		}
		rc10 = scsi_datain_unmarshall(task);
		if (rc10 == NULL) {
			fprintf(stderr, "failed to unmarshall readcapacity10 data\n");
			exit(10);
		}
		client.dst_blocksize  = rc10->block_size;
		client.dst_num_blocks  = rc10->lba;
		scsi_free_scsi_task(task);
	}

	if (client.src_blocksize != client.dst_blocksize) {
		fprintf(stderr, "source LUN has different blocksize than destination than destination (%d != %d sectors)\n", client.src_blocksize, client.dst_blocksize);
		exit(10);
	}

	if (client.src_num_blocks > client.dst_num_blocks) {
		fprintf(stderr, "source LUN is bigger than destination (%"PRIu64" > %"PRIu64" sectors)\n", client.src_num_blocks, client.dst_num_blocks);
		exit(10);
	}

	fill_read_queue(&client);

	while (client.finished == 0) {
		pfd[0].fd = iscsi_get_fd(client.src_iscsi);
		pfd[0].events = iscsi_which_events(client.src_iscsi);
		pfd[1].fd = iscsi_get_fd(client.dst_iscsi);
		pfd[1].events = iscsi_which_events(client.dst_iscsi);

		if (poll(&pfd[0], 2, -1) < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (iscsi_service(client.src_iscsi, pfd[0].revents) < 0) {
			printf("iscsi_service failed with : %s\n", iscsi_get_error(client.src_iscsi));
			break;
		}
		if (iscsi_service(client.dst_iscsi, pfd[1].revents) < 0) {
			printf("iscsi_service failed with : %s\n", iscsi_get_error(client.dst_iscsi));
			break;
		}
	}

	iscsi_logout_sync(client.src_iscsi);
	iscsi_destroy_context(client.src_iscsi);
	iscsi_logout_sync(client.dst_iscsi);
	iscsi_destroy_context(client.dst_iscsi);

	return 0;
}

