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
#include <string.h>
#include <poll.h>
#include <popt.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-inq";


void inquiry_block_limits(struct scsi_inquiry_block_limits *inq)
{
	printf("wsnz:%d\n", inq->wsnz);
	printf("maximum compare and write length:%d\n", inq->max_cmp);
	printf("optimal transfer length granularity:%d\n", inq->opt_gran);
	printf("maximum transfer length:%d\n", inq->max_xfer_len);
	printf("optimal transfer length:%d\n",inq->opt_xfer_len);
	printf("maximum prefetch xdread xdwrite transfer length:%d\n", inq->max_prefetch);
	printf("maximum unmap lba count:%d\n", inq->max_unmap);
	printf("maximum unmap block descriptor count:%d\n", inq->max_unmap_bdc);
	printf("optimal unmap granularity:%d\n", inq->opt_unmap_gran);
	printf("ugavalid:%d\n", inq->ugavalid);
	printf("unmap granularity alignment:%d\n", inq->unmap_gran_align);
	printf("maximum write same length:%d\n", (int)inq->max_ws_len);
}

void inquiry_logical_block_provisioning(struct scsi_inquiry_logical_block_provisioning *inq)
{
	printf("Threshold Exponent:%d\n", inq->threshold_exponent);	
	printf("lbpu:%d\n", inq->lbpu);	
	printf("lbpws:%d\n", inq->lbpws);	
	printf("lbpws10:%d\n", inq->lbpws10);	
	printf("lbprz:%d\n", inq->lbprz);	
	printf("anc_sup:%d\n", inq->anc_sup);	
	printf("dp:%d\n", inq->dp);	
	printf("provisioning type:%d\n", inq->provisioning_type);	
}

void inquiry_block_device_characteristics(struct scsi_inquiry_block_device_characteristics *inq)
{
	printf("Medium Rotation Rate:%dRPM\n", inq->medium_rotation_rate);	
}

void inquiry_device_identification(struct scsi_inquiry_device_identification *inq)
{
	struct scsi_inquiry_device_designator *dev;
	int i;

	printf("Peripheral Qualifier:%s\n",
		scsi_devqualifier_to_str(inq->qualifier));
	printf("Peripheral Device Type:%s\n",
		scsi_devtype_to_str(inq->device_type));
	printf("Page Code:(0x%02x) %s\n",
		inq->pagecode, scsi_inquiry_pagecode_to_str(inq->pagecode));

	for (i=0, dev = inq->designators; dev; i++, dev = dev->next) {
		printf("DEVICE DESIGNATOR #%d\n", i);
		if (dev->piv != 0) {
			printf("Device Protocol Identifier:(%d) %s\n", dev->protocol_identifier, scsi_protocol_identifier_to_str(dev->protocol_identifier));
		}
		printf("Code Set:(%d) %s\n", dev->code_set, scsi_codeset_to_str(dev->code_set));
		printf("PIV:%d\n", dev->piv);
		printf("Association:(%d) %s\n", dev->association, scsi_association_to_str(dev->association));
		printf("Designator Type:(%d) %s\n", dev->designator_type, scsi_designator_type_to_str(dev->designator_type));
		printf("Designator:[%s]\n", dev->designator);
	}
}

void inquiry_unit_serial_number(struct scsi_inquiry_unit_serial_number *inq)
{
	printf("Unit Serial Number:[%s]\n", inq->usn);
}

void inquiry_supported_pages(struct scsi_inquiry_supported_pages *inq)
{
	int i;

	for (i = 0; i < inq->num_pages; i++) {
		printf("Page:0x%02x %s\n", inq->pages[i], scsi_inquiry_pagecode_to_str(inq->pages[i]));
	}
}

void inquiry_standard(struct scsi_inquiry_standard *inq)
{
	printf("Peripheral Qualifier:%s\n",
		scsi_devqualifier_to_str(inq->qualifier));
	printf("Peripheral Device Type:%s\n",
		scsi_devtype_to_str(inq->device_type));
	printf("Removable:%d\n", inq->rmb);
	printf("Version:%d %s\n", inq->version, scsi_version_to_str(inq->version));
	printf("NormACA:%d\n", inq->normaca);
	printf("HiSup:%d\n", inq->hisup);
	printf("ReponseDataFormat:%d\n", inq->response_data_format);
	printf("SCCS:%d\n", inq->sccs);
	printf("ACC:%d\n", inq->acc);
	printf("TPGS:%d\n", inq->tpgs);
	printf("3PC:%d\n", inq->threepc);
	printf("Protect:%d\n", inq->protect);
	printf("EncServ:%d\n", inq->encserv);
	printf("MultiP:%d\n", inq->multip);
	printf("SYNC:%d\n", inq->sync);
	printf("CmdQue:%d\n", inq->cmdque);
	printf("Vendor:%s\n", inq->vendor_identification);
	printf("Product:%s\n", inq->product_identification);
	printf("Revision:%s\n", inq->product_revision_level);
}

void do_inquiry(struct iscsi_context *iscsi, int lun, int evpd, int pc)
{
	struct scsi_task *task;
	int full_size;
	void *inq;

	/* See how big this inquiry data is */
	task = iscsi_inquiry_sync(iscsi, lun, evpd, pc, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		fprintf(stderr, "Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		if ((task = iscsi_inquiry_sync(iscsi, lun, evpd, pc, full_size)) == NULL) {
			fprintf(stderr, "Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			exit(10);
		}
	}

	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		fprintf(stderr, "failed to unmarshall inquiry datain blob\n");
		exit(10);
	}

	if (evpd == 0) {
		inquiry_standard(inq);
	} else {
		switch (pc) {
		case SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES:
			inquiry_supported_pages(inq);
			break;
		case SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER:
			inquiry_unit_serial_number(inq);
			break;
		case SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION:
			inquiry_device_identification(inq);
			break;
		case SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS:
			inquiry_block_limits(inq);
			break;
		case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
			inquiry_block_device_characteristics(inq);
			break;
		case SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING:
			inquiry_logical_block_provisioning(inq);
			break;
		default:
			fprintf(stderr, "Usupported pagecode:0x%02x\n", pc);
		}
	}
	scsi_free_scsi_task(task);
}


void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-inq [-?] [-?|--help] [--usage] [-i|--initiator-name=iqn-name]\n"
			"\t\t[-e|--evpd=integer] [-c|--pagecode=integer] <iscsi-url>\n");
}

void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-inq [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -e, --evpd=integer                evpd\n");
	fprintf(stderr, "  -c, --pagecode=integer            page code\n");
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

int main(int argc, const char *argv[])
{
	poptContext pc;
	struct iscsi_context *iscsi;
	const char **extra_argv;
	int extra_argc = 0;
	char *url = NULL;
	struct iscsi_url *iscsi_url = NULL;
	int evpd = 0, pagecode = 0;
	int show_help = 0, show_usage = 0, debug = 0;
	int res;

	struct poptOption popt_options[] = {
		{ "help", '?', POPT_ARG_NONE, &show_help, 0, "Show this help message", NULL },
		{ "usage", 0, POPT_ARG_NONE, &show_usage, 0, "Display brief usage message", NULL },
		{ "initiator-name", 'i', POPT_ARG_STRING, &initiator, 0, "Initiatorname to use", "iqn-name" },
		{ "evpd", 'e', POPT_ARG_INT, &evpd, 0, "evpd", "integer" },
		{ "pagecode", 'c', POPT_ARG_INT, &pagecode, 0, "page code", "integer" },
		{ "debug", 'd', POPT_ARG_INT, &debug, 0, "Debugging level", "integer" },
		POPT_TABLEEND
	};

	pc = poptGetContext(argv[0], argc, argv, popt_options, POPT_CONTEXT_POSIXMEHARDER);
	if ((res = poptGetNextOpt(pc)) < -1) {
		fprintf(stderr, "Failed to parse option : %s %s\n",
			poptBadOption(pc, 0), poptStrerror(res));
		exit(10);
	}
	extra_argv = poptGetArgs(pc);
	if (extra_argv) {
		url = *extra_argv;
		extra_argv++;
		while (extra_argv[extra_argc]) {
			extra_argc++;
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

	poptFreeContext(pc);

	iscsi = iscsi_create_context(initiator);
	if (iscsi == NULL) {
		fprintf(stderr, "Failed to create context\n");
		exit(10);
	}

    if (debug > 0) {
        iscsi_set_debug(iscsi, debug);
    }

	if (url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		exit(10);
	}
	iscsi_url = iscsi_parse_full_url(iscsi, url);
	
	if (url) free(url);
	
	if (iscsi_url == NULL) {
		fprintf(stderr, "Failed to parse URL: %s\n", 
			iscsi_get_error(iscsi));
		exit(10);
	}

	iscsi_set_targetname(iscsi, iscsi_url->target);
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

	if (iscsi_url->user != NULL) {
		if (iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
			fprintf(stderr, "Failed to set initiator username and password\n");
			exit(10);
		}
	}

	if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
		fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_url(iscsi_url);
		iscsi_destroy_context(iscsi);
		exit(10);
	}

	do_inquiry(iscsi, iscsi_url->lun, evpd, pagecode);
	iscsi_destroy_url(iscsi_url);

	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);
	return 0;
}

