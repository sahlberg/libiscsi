/* 
   Copyright (C) 2023 by zhenwei pi <pizhenwei@bytedance.com>

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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <getopt.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-discard";

/* query unmap/write zero max blocks */
static uint64_t inquiry_max_blocks(struct iscsi_context *iscsi, int lun, int zeroout)
{
	struct scsi_task *task;
	int full_size;
	struct scsi_inquiry_block_limits *inq;
	uint64_t max_blocks = 0;

	/* See how big this inquiry data is */
	task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		exit(EIO);
	}

	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, full_size)) == NULL) {
			fprintf(stderr, "Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			exit(EIO);
		}
	}

	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		fprintf(stderr, "failed to unmarshall inquiry datain blob\n");
		exit(EIO);
	}

	if (zeroout) {
		/* A MAXIMUM WRITE SAME LENGTH field set to zero indicates
		   that the device server does not report a limit on the number
		   of logical blocks that may be requested for a single
		   WRITE SAME command */
		max_blocks = inq->max_ws_len;
		if (!max_blocks)
			max_blocks = (uint64_t)-1ULL;
	} else {
		/* A MAXIMUM UNMAP LBA COUNT field set to 0000_0000h indicates
		   that the device server does not implement the UNMAP command */
		max_blocks = inq->max_unmap;
	}

	scsi_free_scsi_task(task);

	return max_blocks;
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi_readcapacity16 [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -o, --offset                      "
			"Byte offset into the target from which to start discarding. "
			"The provided value must be aligned to the target sector size. "
			"The default value is zero.\n");
	fprintf(stderr, "  -l, --length                      "
			"The number of bytes to discard (counting from the starting point). "
			"The provided value must be aligned to the target sector size. "
			"If the specified value extends past the end of the device, "
			"iscsi-discard will stop at the device size boundary. "
			"The default value extends to the end of the device.\n");
	fprintf(stderr, "  -z, --zeroout                     "
			"Zero-fill rather than discard.\n");
	fprintf(stderr, "  -d, --debug=integer               debug level (0=disabled)\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Help options:\n");
	fprintf(stderr, "  -?, --help                        Show this help message\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "iSCSI URL format : %s\n", ISCSI_URL_SYNTAX);
	fprintf(stderr, "\n");
	fprintf(stderr, "<host> is either of:\n");
	fprintf(stderr, "  \"hostname\"       iscsi.example\n");
	fprintf(stderr, "  \"ipv4-address\"   10.1.1.27\n");
	fprintf(stderr, "  \"ipv6-address\"   [fce0::1]\n");

	exit(0);
}

int main(int argc, char *argv[])
{
	struct iscsi_context *iscsi;
	char *url = NULL;
	struct iscsi_url *iscsi_url = NULL;
	int debug = 0, zeroout = 0;
	int option_index, c;
	unsigned int block_length;
	uint64_t offset = 0, length = 0, capacity, lba, blocks, max_blocks;
	struct scsi_task *task;
	struct scsi_readcapacity16 *rc16;
	int ret = EINVAL;

	static struct option long_options[] = {
		{"offset",         required_argument,    NULL,        'o'},
		{"length",         required_argument,    NULL,        'l'},
		{"zeroout",        no_argument,          NULL,        'z'},
		{"debug",          required_argument,    NULL,        'd'},
		{"help",           no_argument,          NULL,        'h'},
		{"initiator-name", required_argument,    NULL,        'i'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "o:l:zd:i:h?", long_options,
					&option_index)) != -1) {
		switch (c) {
			case 'o':
				offset = strtoll(optarg, NULL, 0);
				break;
			case 'l':
				length = strtoll(optarg, NULL, 0);
				break;
			case 'z':
				zeroout = 1;
				break;
			case 'd':
				debug = strtol(optarg, NULL, 0);
				break;
			case 'i':
				initiator = optarg;
				break;
			case 'h':
			case '?':
				print_help();
				break;
			default:
				fprintf(stderr, "Unrecognized option '%c'\n\n", c);
				print_help();
				break;
		}
	}

	iscsi = iscsi_create_context(initiator);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(EINVAL);
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
		print_help();
	}
	iscsi_url = iscsi_parse_full_url(iscsi, url);

	free(url);

	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n", 
				iscsi_get_error(iscsi));
		exit(EINVAL);
	}

	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		goto out;
	}

	task = iscsi_readcapacity16_sync(iscsi, iscsi_url->lun);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr,"Failed to send readcapacity command\n");
		goto out;
	}

	rc16 = scsi_datain_unmarshall(task);
	if (rc16 == NULL) {
		fprintf(stderr,"Failed to unmarshall readcapacity16 data\n");
		goto out;
	}

	block_length = rc16->block_length;
	if (offset & (block_length - 1)) {
		fprintf(stderr,"Unaligned offset of %u\n", block_length);
		goto free_task;
	}

	capacity = block_length * (rc16->returned_lba + 1);
	if (offset > capacity) {
		fprintf(stderr,"Offset(%lu) exceeds capacity(%lu)\n", offset, capacity);
		goto free_task;
	}

	if (!length || (offset + length > capacity)) {
		length = block_length * (rc16->returned_lba + 1) - offset;
	}

	/* free readcapacity16 task */
	scsi_free_scsi_task(task);

	max_blocks = inquiry_max_blocks(iscsi, iscsi_url->lun, zeroout);
	if (!max_blocks) {
		fprintf(stderr, "Operation not supported\n");
		exit(EOPNOTSUPP);
	}

	lba = offset / block_length;
	blocks = length / block_length;
	for (uint64_t endlba = lba + blocks; lba < endlba; ) {
		uint64_t towrite = MIN(max_blocks, endlba - lba);

		if (zeroout) {
			static void *zerobuf;

			if (!zerobuf) {
				zerobuf = calloc(block_length, 1);
				assert(zerobuf);
			}

			task = iscsi_writesame16_sync(iscsi, iscsi_url->lun, lba, zerobuf,
					block_length, towrite, 0, 0, 0, 0);
			if (task == NULL || task->status != SCSI_STATUS_GOOD) {
				fprintf(stderr,"Failed to execute writesame16 command\n");
				goto out;
			}
		} else {
			struct unmap_list list;

			list.lba = lba;
			list.num = towrite;
			task = iscsi_unmap_sync(iscsi, iscsi_url->lun, 0, 0, &list, 1);
			if (task == NULL || task->status != SCSI_STATUS_GOOD) {
				fprintf(stderr,"Failed to execute unmap command\n");
				goto out;
			}
		}

		lba += towrite;
		scsi_free_scsi_task(task);
	}

	ret = 0;
	goto out;

free_task:
	scsi_free_scsi_task(task);

out:
	iscsi_destroy_url(iscsi_url);
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);

	return ret;
}
