/*
   iscsi-test tool

   Copyright (C) 2012 by Lee Duncan <lee@gonzoleeman.net>

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

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <popt.h>
#include <fnmatch.h>
#include "slist.h"
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"
// #include "iscsi-test.h"
#include "iscsi-support.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

#define	PROG	"iscsi-test-cu"


int (*real_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);


	
static void
print_usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-?] [-?|--help] [--usage] [-t|--test=<test>] [-s|--skip=<test>]\n"
	    "\t\t[-l|--list] [--info] [-i|--initiator-name=<iqn-name>]\n"
	    "\t\t<iscsi-url>\n", PROG);
}

static void
print_help(void)
{
	fprintf(stderr,\
"Usage: %s [OPTIONS] <iscsi-url>\n"
"  -i, --initiator-name=iqn-name     Initiatorname to use\n"
"  -I, --initiator-name-2=iqn-name   Second initiatorname to use\n"
"  -t, --test=test-name              Which test to run. Default is to run all tests.\n"
"  -s, --skip=test-name              Which test to skip. Default is to run all tests.\n"
"  -l, --list                        List all tests.\n"
"  --info,                           Print extra info about a test.\n"
"  --dataloss                        Allow destructive tests.\n"
"\n"
"Help options:\n"
"  -?, --help                        Show this help message\n"
"      --usage                       Display brief usage message\n"
"\n"
"iSCSI URL format : %s\n"
"\n"
"<host> is either of:\n"
"  \"hostname\"       iscsi.example\n"
"  \"ipv4-address\"   10.1.1.27\n"
"  \"ipv6-address\"   [fce0::1]\n",
	    PROG, ISCSI_URL_SYNTAX);
}


int
main(int argc, const char *argv[])
{
	poptContext pc;
	const char **extra_argv;
	int extra_argc = 0;
	const char *url = NULL;
	int show_help = 0, show_usage = 0, list_tests = 0;
	int res;
	char *testname = NULL;
	char *skipname = NULL;
	int lun;
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_readcapacity10 *rc10;
	struct scsi_readcapacity16 *rc16;
	struct scsi_inquiry_standard *inq;
	int full_size;

	struct poptOption popt_options[] = {
		{ "help", '?', POPT_ARG_NONE,
		  &show_help, 0, "Show this help message", NULL },
		{ "usage", 0, POPT_ARG_NONE,
		  &show_usage, 0, "Display brief usage message", NULL },
		{ "list", 'l', POPT_ARG_NONE,
		  &list_tests, 0, "List all tests", NULL },
		{ "initiator-name", 'i', POPT_ARG_STRING,
		  &initiatorname1, 0, "Initiatorname to use", "iqn-name" },
		{ "initiator-name-2", 'I', POPT_ARG_STRING,
		  &initiatorname2, 0, "Second initiatorname to use for tests using more than one session", "iqn-name" },
		{ "test", 't', POPT_ARG_STRING,
		  &testname, 0, "Which test to run", "testname" },
		{ "skip", 's', POPT_ARG_STRING,
		  &skipname, 0, "Which test to skip", "skipname" },
		{ "info", 0, POPT_ARG_NONE,
		  &show_info, 0, "Show information about the test", "testname" },
		{ "dataloss", 0, POPT_ARG_NONE,
		  &data_loss, 0, "Allow destructuve tests", NULL },
		POPT_TABLEEND
	};

	real_iscsi_queue_pdu = dlsym(RTLD_NEXT, "iscsi_queue_pdu");

	pc = poptGetContext(argv[0], argc, argv, popt_options,
	    POPT_CONTEXT_POSIXMEHARDER);
	if ((res = poptGetNextOpt(pc)) < -1) {
		fprintf(stderr, "Failed to parse option : %s %s\n",
			poptBadOption(pc, 0), poptStrerror(res));
		return 10;
	}
	extra_argv = poptGetArgs(pc);
	if (extra_argv) {
		url = strdup(*extra_argv);
		extra_argv++;
		while (extra_argv[extra_argc]) {
			extra_argc++;
		}
	}

	if (show_help != 0) {
		print_help();
		return 0;
	}

	if (show_usage != 0) {
		print_usage();
		return 0;
	}

	if (list_tests != 0) {
		/*
		 * XXX list tests here ...
		 */
		return 0;
	}
	poptFreeContext(pc);

	if (url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		free(skipname);
		free(testname);
		return 10;
	}

	iscsi = iscsi_context_login(initiatorname1, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/*
	 * find the size of the LUN
	 * All devices support readcapacity10 but only some support
	 * readcapacity16
	 */
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
		printf("Failed to send READCAPACITY10 command: %s\n",
		    iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY10 command: failed with sense. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		iscsi_destroy_context(iscsi);
		return -1;
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall READCAPACITY10 data. %s\n",
		    iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		iscsi_destroy_context(iscsi);
		return -1;
	}
	block_size = rc10->block_size;
	num_blocks = rc10->lba;
	scsi_free_scsi_task(task);

	task = iscsi_readcapacity16_sync(iscsi, lun);
	if (task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n",
		    iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		rc16 = scsi_datain_unmarshall(task);
		if (rc16 == NULL) {
			printf("failed to unmarshall READCAPACITY16 data. %s\n",
			    iscsi_get_error(iscsi));
			scsi_free_scsi_task(task);
			iscsi_destroy_context(iscsi);
			return -1;
		}
		block_size = rc16->block_length;
		num_blocks = rc16->returned_lba;
		lbpme = rc16->lbpme;
		lbppb = 1 << rc16->lbppbe;
		lbpme = rc16->lbpme;

		scsi_free_scsi_task(task);
	}


	task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		task = iscsi_inquiry_sync(iscsi, lun, 0, 0, full_size);
		if (task == NULL) {
			printf("Inquiry command failed : %s\n",
			    iscsi_get_error(iscsi));
			return -1;
		}
	}
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		return -1;
	}
	removable = inq->rmb;
	device_type = inq->device_type;
	sccs = inq->sccs;
	encserv = inq->encserv;
	scsi_free_scsi_task(task);

	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);

	/*
	 * XXX do tests here
	 */

	free(skipname);
	free(testname);
	free(discard_const(url));

	return 0;				/* XXX 0??? */
}
