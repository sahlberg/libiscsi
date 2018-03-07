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
#include <unistd.h>
#include <limits.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

const char *initiator = "iqn.2010-11.ronnie:iscsi-inq";
uint32_t max_in_flight = 50;
uint32_t blocks_per_io = 200;

struct client {
	int finished;
	uint32_t in_flight;

	struct iscsi_context *src_iscsi;
	int src_lun;
	int src_blocksize;
	uint64_t src_num_blocks;
	struct scsi_inquiry_device_designator src_tgt_desig;
	uint64_t pos;

	struct iscsi_context *dst_iscsi;
	int dst_lun;
	int dst_blocksize;
	uint64_t dst_num_blocks;
	struct scsi_inquiry_device_designator dst_tgt_desig;
	int use_16_for_rw;
	int use_xcopy;
	int progress;
	int ignore_errors;
};


void fill_read_queue(struct client *client);
void fill_xcopy_queue(struct client *client);

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
	uint32_t num_blocks;

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

int populate_tgt_desc(unsigned char *desc,
		      struct scsi_inquiry_device_designator *tgt_desig,
		      int rel_init_port_id, uint32_t block_size)
{
	desc[0] = IDENT_DESCR_TGT_DESCR;
	desc[1] = 0;	/* peripheral type */
	desc[2] = (rel_init_port_id >> 8) & 0xFF;
	desc[3] = rel_init_port_id & 0xFF;
	desc[4] = tgt_desig->code_set;
	desc[5] = (tgt_desig->designator_type & 0xF)
		| ((tgt_desig->association & 3) << 4);
	desc[7] = tgt_desig->designator_length;
	memcpy(desc + 8, tgt_desig->designator, tgt_desig->designator_length);

	desc[28] = 0;
	desc[29] = (block_size >> 16) & 0xFF;
	desc[30] = (block_size >> 8) & 0xFF;
	desc[31] = block_size & 0xFF;

	return 32;
}

int populate_seg_desc_hdr(unsigned char *hdr, int dc, int cat, int src_index,
			  int dst_index)
{
	int desc_len = 28;

	hdr[0] = BLK_TO_BLK_SEG_DESCR;
	hdr[1] = ((dc << 1) | cat) & 0xFF;
	hdr[2] = (desc_len >> 8) & 0xFF;
	hdr[3] = (desc_len - SEG_DESC_SRC_INDEX_OFFSET) & 0xFF; /* don't account for the first 4 bytes in descriptor header*/
	hdr[4] = (src_index >> 8) & 0xFF;
	hdr[5] = src_index & 0xFF;
	hdr[6] = (dst_index >> 8) & 0xFF;
	hdr[7] = dst_index & 0xFF;

	return desc_len;
}

int populate_seg_desc_b2b(unsigned char *desc, int dc, int cat,
			  int src_index, int dst_index, int num_blks,
			  uint64_t src_lba, uint64_t dst_lba)
{
	int desc_len = populate_seg_desc_hdr(desc, dc, cat,
					     src_index, dst_index);

	desc[10] = (num_blks >> 8) & 0xFF;
	desc[11] = num_blks & 0xFF;
	desc[12] = (src_lba >> 56) & 0xFF;
	desc[13] = (src_lba >> 48) & 0xFF;
	desc[14] = (src_lba >> 40) & 0xFF;
	desc[15] = (src_lba >> 32) & 0xFF;
	desc[16] = (src_lba >> 24) & 0xFF;
	desc[17] = (src_lba >> 16) & 0xFF;
	desc[18] = (src_lba >> 8) & 0xFF;
	desc[19] = src_lba & 0xFF;
	desc[20] = (dst_lba >> 56) & 0xFF;
	desc[21] = (dst_lba >> 48) & 0xFF;
	desc[22] = (dst_lba >> 40) & 0xFF;
	desc[23] = (dst_lba >> 32) & 0xFF;
	desc[24] = (dst_lba >> 24) & 0xFF;
	desc[25] = (dst_lba >> 16) & 0xFF;
	desc[26] = (dst_lba >> 8) & 0xFF;
	desc[27] = dst_lba & 0xFF;

	return desc_len;
}

void populate_param_header(unsigned char *buf, int list_id, int str, int list_id_usage, int prio, int tgt_desc_len, int seg_desc_len, int inline_data_len)
{
	buf[0] = list_id;
	buf[1] = ((str & 1) << 5) | ((list_id_usage & 3) << 3) | (prio & 7);
	buf[2] = (tgt_desc_len >> 8) & 0xFF;
	buf[3] = tgt_desc_len & 0xFF;
	buf[8] = (seg_desc_len >> 24) & 0xFF;
	buf[9] = (seg_desc_len >> 16) & 0xFF;
	buf[10] = (seg_desc_len >> 8) & 0xFF;
	buf[11] = seg_desc_len & 0xFF;
	buf[12] = (inline_data_len >> 24) & 0xFF;
	buf[13] = (inline_data_len >> 16) & 0xFF;
	buf[14] = (inline_data_len >> 8) & 0xFF;
	buf[15] = inline_data_len & 0xFF;
}

void xcopy_cb(struct iscsi_context *iscsi _U_, int status, void *command_data, void *private_data)
{
	struct client *client = (struct client *)private_data;
	struct scsi_task *task = command_data;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("XCOPY failed with sense key:%d ascq:%04x\n",
			task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	if (status != SCSI_STATUS_GOOD) {
		printf("XCOPY failed with %s\n", iscsi_get_error(iscsi));
		if (!client->ignore_errors) {
			scsi_free_scsi_task(task);
			exit(10);
		}
	}

	client->in_flight--;
	fill_xcopy_queue(client);

	if (client->progress) {
		printf("\r%"PRIu64" of %"PRIu64" blocks transferred.",
			client->pos, client->src_num_blocks);
	}

	if ((client->in_flight == 0) && (client->pos == client->src_num_blocks)) {
		client->finished = 1;
		if (client->progress) {
			printf("\n");
		}
	}
	scsi_free_scsi_task(task);
}

void fill_xcopy_queue(struct client *client)
{
	while (client->in_flight < max_in_flight && client->pos < client->src_num_blocks) {
		struct scsi_task *task;
		struct iscsi_data data;
		unsigned char *xcopybuf;
		int offset;
		uint32_t num_blocks;
		int tgt_desc_len;
		int seg_desc_len;

		client->in_flight++;

		num_blocks = client->src_num_blocks - client->pos;
		if (num_blocks > blocks_per_io) {
			num_blocks = blocks_per_io;
		}

		data.size = XCOPY_DESC_OFFSET +
			32 * 2 +	/* IDENT_DESCR_TGT_DESCR */
			28;		/* BLK_TO_BLK_SEG_DESCR */
		data.data = malloc(data.size);
		if (data.data == NULL) {
			printf("failed to alloc XCOPY buffer\n");
			exit(10);
		}

		xcopybuf = data.data;
		memset(xcopybuf, 0, data.size);

		/* Initialise CSCD list with one src + one dst descriptor */
		offset = XCOPY_DESC_OFFSET;
		offset += populate_tgt_desc(xcopybuf + offset,
					&client->src_tgt_desig,
					0, client->src_blocksize);
		offset += populate_tgt_desc(xcopybuf + offset,
					&client->dst_tgt_desig,
					0, client->dst_blocksize);
		tgt_desc_len = offset - XCOPY_DESC_OFFSET;

		/* Initialise one segment descriptor */
		seg_desc_len = populate_seg_desc_b2b(xcopybuf + offset, 0, 0,
				0, 1, num_blocks, client->pos, client->pos);
		offset += seg_desc_len;

		/* Initialise the parameter list header */
		populate_param_header(xcopybuf, 1, 0, LIST_ID_USAGE_DISCARD, 0,
				tgt_desc_len, seg_desc_len, 0);

		task = iscsi_extended_copy_task(client->src_iscsi,
						client->src_lun,
						&data, xcopy_cb, client);
		if (task == NULL) {
			printf("failed to send XCOPY command\n");
			exit(10);
		}

		client->pos += num_blocks;
	}
}

void cscd_ident_inq(struct iscsi_context *iscsi,
		int lun,
		struct scsi_inquiry_device_designator *_tgt_desig)
{
	struct scsi_task *task = NULL;
	struct scsi_inquiry_device_identification *inq_di = NULL;
	struct scsi_inquiry_device_designator *desig, *tgt_desig = NULL;
	enum scsi_designator_type prev_type = 0;

	/* check what type of lun we have */
	task = iscsi_inquiry_sync(iscsi, lun, 1,
			SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION, 255);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "failed to send inquiry command: %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}

	inq_di = scsi_datain_unmarshall(task);
	if (inq_di == NULL) {
		fprintf(stderr, "failed to unmarshall inquiry datain blob\n");
		exit(10);
	}

	for (desig = inq_di->designators; desig; desig = desig->next) {
		switch (desig->designator_type) {
			case SCSI_DESIGNATOR_TYPE_VENDOR_SPECIFIC:
			case SCSI_DESIGNATOR_TYPE_T10_VENDORT_ID:
			case SCSI_DESIGNATOR_TYPE_EUI_64:
			case SCSI_DESIGNATOR_TYPE_NAA:
				if (prev_type <= desig->designator_type) {
					tgt_desig = desig;
					prev_type = desig->designator_type;
				}
				/* fall through */
			default:
				continue;
		}
	}

	if (tgt_desig == NULL) {
		fprintf(stderr, "No suitalble target descriptor format found");
		exit(10);
	}

	/* copy what's needed for XCOPY */
	_tgt_desig->code_set = tgt_desig->code_set;
	_tgt_desig->association = tgt_desig->association;
	_tgt_desig->designator_type = tgt_desig->designator_type;
	_tgt_desig->designator_length = tgt_desig->designator_length;
	_tgt_desig->designator = malloc(tgt_desig->designator_length);
	memcpy(_tgt_desig->designator, tgt_desig->designator, tgt_desig->designator_length);

	scsi_free_scsi_task(task);
}

void cscd_param_check(struct iscsi_context *iscsi,
		      int lun,
		      uint32_t blocksize)
{
	struct scsi_task *task = NULL;
	struct scsi_copy_results_op_params *opp;
	uint32_t io_segment_bytes;

	task = iscsi_receive_copy_results_sync(iscsi, lun,
					SCSI_COPY_RESULTS_OP_PARAMS, 0, 1024);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "XCOPY RECEIVE COPY RESULTS failed: %s\n",
			iscsi_get_error(iscsi));
		exit(10);
	}

	opp = scsi_datain_unmarshall(task);
	if (opp == NULL) {
		fprintf(stderr, "failed to unmarshall XCOPY RCR datain blob\n");
		exit(10);
	}

	if (opp->max_target_desc_count < 2) {
		fprintf(stderr, "XCOPY max CSCD desc count %d too small\n",
			opp->max_target_desc_count);
		exit(10);
	}
	if (opp->max_segment_desc_count < 1) {
		fprintf(stderr, "XCOPY max segment desc count %d too small\n",
			opp->max_segment_desc_count);
		exit(10);
	}

	io_segment_bytes = blocks_per_io * blocksize;
	if (io_segment_bytes > opp->max_segment_length) {
		fprintf(stderr,
			"%u bytes per I/O exceeds XCOPY max segment len %u\n",
			io_segment_bytes, opp->max_segment_length);
		exit(10);
	}
	if (blocks_per_io > USHRT_MAX) {
		fprintf(stderr,
			"%u blocks per I/O exceeds XCOPY field width max %u\n",
			blocks_per_io, USHRT_MAX);
		exit(10);
	}

	scsi_free_scsi_task(task);
}

void readcap(struct iscsi_context *iscsi, int lun, int use_16,
		int *_blocksize, uint64_t *_num_blocks)
{
	struct scsi_task *task;

	if (use_16) {
		struct scsi_readcapacity16 *rc16;

		task = iscsi_readcapacity16_sync(iscsi, lun);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr,
				"failed to send readcapacity command\n");
			exit(10);
		}
		rc16 = scsi_datain_unmarshall(task);
		if (rc16 == NULL) {
			fprintf(stderr,
				"failed to unmarshall readcapacity16 data\n");
			exit(10);
		}
		*_blocksize  = rc16->block_length;
		*_num_blocks  = rc16->returned_lba + 1;
	} else {
		struct scsi_readcapacity10 *rc10;

		task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr,
				"failed to send readcapacity command\n");
			exit(10);
		}
		rc10 = scsi_datain_unmarshall(task);
		if (rc10 == NULL) {
			fprintf(stderr,
				"failed to unmarshall readcapacity10 data\n");
			exit(10);
		}
		*_blocksize  = rc10->block_size;
		*_num_blocks  = rc10->lba;
	}

	scsi_free_scsi_task(task);
	return;
}

int main(int argc, char *argv[])
{
	char *src_url = NULL;
	char *dst_url = NULL;
	struct iscsi_url *iscsi_url;
	int c;
	struct pollfd pfd[2];
	struct client client;

	static struct option long_options[] = {
		{"dst",            required_argument,    NULL,        'd'},
		{"src",            required_argument,    NULL,        's'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{"progress",       no_argument,          NULL,        'p'},
		{"16",             no_argument,          NULL,        '6'},
		{"xcopy",          no_argument,          NULL,        'x'},
		{"max",            required_argument,    NULL,        'm'},
		{"blocks",         required_argument,    NULL,        'b'},
		{"ignore-errors",  no_argument,          NULL,        'n'},
		{0, 0, 0, 0}
	};
	int option_index;

	memset(&client, 0, sizeof(client));

	while ((c = getopt_long(argc, argv, "d:s:i:m:b:p6nx", long_options,
			&option_index)) != -1) {
		char *endptr;

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
		case 'x':
			client.use_xcopy = 1;
			break;
		case 'm':
			max_in_flight = strtoul(optarg, &endptr, 10);
			if (*endptr != '\0' || max_in_flight == UINT_MAX) {
				fprintf(stderr, "Invalid max in flight: %s\n",
					optarg);
				exit(10);
			}
			break;
		case 'b':
			blocks_per_io = strtoul(optarg, &endptr, 10);
			if (*endptr != '\0' || blocks_per_io == UINT_MAX) {
				fprintf(stderr, "Invalid blocks per I/O: %s\n",
					optarg);
				exit(10);
			}
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
	iscsi_set_session_type(client.src_iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(client.src_iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
	if (iscsi_full_connect_sync(client.src_iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(client.src_iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(client.src_iscsi);
		exit(10);
	}
	client.src_lun = iscsi_url->lun;
	iscsi_destroy_url(iscsi_url);

	readcap(client.src_iscsi, client.src_lun, client.use_16_for_rw,
		&client.src_blocksize, &client.src_num_blocks);

	if (client.use_xcopy) {
		cscd_ident_inq(client.src_iscsi, client.src_lun,
				&client.src_tgt_desig);
		cscd_param_check(client.src_iscsi, client.src_lun,
				 client.src_blocksize);
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
	iscsi_set_session_type(client.dst_iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(client.dst_iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
	if (iscsi_full_connect_sync(client.dst_iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(client.dst_iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(client.dst_iscsi);
		exit(10);
	}
	client.dst_lun = iscsi_url->lun;
	iscsi_destroy_url(iscsi_url);

	readcap(client.dst_iscsi, client.dst_lun, client.use_16_for_rw,
		&client.dst_blocksize, &client.dst_num_blocks);

	if (client.use_xcopy) {
		cscd_ident_inq(client.dst_iscsi, client.dst_lun,
				&client.dst_tgt_desig);
		cscd_param_check(client.dst_iscsi, client.dst_lun,
				 client.dst_blocksize);
	}

	if (client.src_blocksize != client.dst_blocksize) {
		fprintf(stderr, "source LUN has different blocksize than destination than destination (%d != %d sectors)\n", client.src_blocksize, client.dst_blocksize);
		exit(10);
	}

	if (client.src_num_blocks > client.dst_num_blocks) {
		fprintf(stderr, "source LUN is bigger than destination (%"PRIu64" > %"PRIu64" sectors)\n", client.src_num_blocks, client.dst_num_blocks);
		exit(10);
	}

	if (client.use_xcopy) {
		fill_xcopy_queue(&client);
	} else {
		fill_read_queue(&client);
	}

	while (client.finished == 0) {
		pfd[0].fd = iscsi_get_fd(client.src_iscsi);
		pfd[0].events = iscsi_which_events(client.src_iscsi);
		pfd[1].fd = iscsi_get_fd(client.dst_iscsi);
		pfd[1].events = iscsi_which_events(client.dst_iscsi);

		if (!pfd[0].events && !pfd[1].events) {
			sleep(1);
			continue;
		}

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

