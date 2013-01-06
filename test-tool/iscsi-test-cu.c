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
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fnmatch.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"

#include "iscsi-support.h"
#include "iscsi-test-cu.h"



#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

#define	PROG	"iscsi-test-cu"

int loglevel = LOG_NORMAL;

/* XXX what is this for? */
int (*real_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

/*****************************************************************
 *
 * list of tests and test suites
 *
 *****************************************************************/
static CU_TestInfo tests_testunitready[] = {
	{ (char *)"testTurSimple", test_testunitready_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read10[] = {
	{ (char *)"testRead10Simple", test_read10_simple },
	{ (char *)"testRead10BeyondEol", test_read10_beyond_eol },
	{ (char *)"testRead10ZeroBlocks", test_read10_0blocks },
	{ (char *)"testRead10ReadProtect", test_read10_rdprotect },
	{ (char *)"testRead10Flags", test_read10_flags },
	{ (char *)"testRead10Invalid", test_read10_invalid },
	CU_TEST_INFO_NULL
};

static CU_SuiteInfo suites[] = {
	{ (char *)"TestTestUnitReady", test_setup, test_teardown,
	  tests_testunitready },
	{ (char *)"TestRead10", test_setup, test_teardown,
	  tests_read10 },
	CU_SUITE_INFO_NULL
};

/*
 * globals for test setup and teardown
 */
int tgt_lun;
struct iscsi_context *iscsic;
struct scsi_task *task;


static void
print_usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-?|--help]    print this message and exit\n",
	    PROG);
	fprintf(stderr,
	    "or     %s [OPTIONS] <iscsi-url>\n", PROG);
	fprintf(stderr,
	    "Where OPTIONS are from:\n");
	fprintf(stderr,
	    "  -i|--initiator-name=iqn-name     Initiatorname to use [%s]\n",
		initiatorname1);
	fprintf(stderr,
	    "  -I|--initiator-name-2=iqn-name   2nd Initiatorname to use [%s]\n",
		initiatorname2);
	fprintf(stderr,
	    "  -t|--test=test-name-reg-exp      Test(s) to run [ALL] <NOT YET IMPLEMENTED>\n");
	fprintf(stderr,
	    "  -l|--list                        List all tests and exit\n");
	fprintf(stderr,
	    "  -X|--dataloss                    Allow destructive tests\n");
	fprintf(stderr,
	    "  -g|--ignore                      Error Action: Ignore test errors [DEFAULT]\n");
	fprintf(stderr,
	    "  -f|--fail                        Error Action: FAIL if any tests fail\n");
	fprintf(stderr,
	    "  -A|--abort                       Error Action: ABORT if any tests fail\n");
	fprintf(stderr,
	    "  -s|--silent                      Test Mode: Silent\n");
	fprintf(stderr,
	    "  -n|--normal                      Test Mode: Normal\n");
	fprintf(stderr,
	    "  -v|--verbose                     Test Mode: Verbose [DEFAULT]\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
	    "Where <iscsi-url> iSCSI URL format is: %s\n", ISCSI_URL_SYNTAX);
	fprintf(stderr, "\n");
	fprintf(stderr,
	    "<host> is one of:\n");
	fprintf(stderr,
	    "  \"hostname\"       e.g. iscsi.example\n");
	fprintf(stderr,
	    "  \"ipv4-address\"   e.g. 10.1.1.27\n");
	fprintf(stderr,
	    "  \"ipv6-address\"   e.g. [fce0::1]\n");
	fprintf(stderr, "\n");
	fprintf(stderr,
	    "and <test-name-reg-exp> is of the form: SUITENAME_RE[.[SUBTESTNAME_RE]]\n");
	fprintf(stderr, "\n");
}


int
test_setup(void)
{
	iscsic = iscsi_context_login(initiatorname1, tgt_url, &tgt_lun);
	if (iscsic == NULL) {
		fprintf(stderr,
		    "error: Failed to login to target for test set-up\n");
		return 1;
	}
	task = NULL;
	return 0;
}


int
test_teardown(void)
{
	if (task)
		scsi_free_scsi_task(task);
	iscsi_logout_sync(iscsic);
	iscsi_destroy_context(iscsic);
	return 0;
}


static void
list_all_tests(void)
{
	CU_SuiteInfo *sp;
	CU_TestInfo *tp;

	for (sp = suites; sp->pName != NULL; sp++)
		for (tp = sp->pTests; tp->pName != NULL; tp++)
			printf("%s.%s\n", sp->pName, tp->pName);
}


static CU_ErrorCode
add_tests(const char *testname_re)
{
	char *suitename_re = NULL;
	char *subtestname_re = NULL;
	char *cp;
	CU_SuiteInfo *sp;
	CU_TestInfo *tp;


	/* if not testname(s) register all tests and return */
	if (!testname_re)
		return CU_register_suites(suites);

	/*
	 * break testname_re into suitename_re and subtestname_re
	 *
	 * syntax is:  SUITE_RE[.[SUBTEST_RE]]
	 */

	/* is there a subtest name? */
	if ((cp = strchr(testname_re, '.')) == NULL) {
		suitename_re = strdup(testname_re);
	} else {
		size_t suitename_sz;
		size_t subtestname_sz;

		suitename_sz = cp - testname_re;
		suitename_re = malloc(suitename_sz+1);
		memset(suitename_re, 0, suitename_sz+1);
		strncpy(suitename_re, testname_re, suitename_sz);

		subtestname_sz = strlen(testname_re) - (suitename_sz+1);
		if (subtestname_sz) {
			subtestname_re = malloc(subtestname_sz+1);
			memset(subtestname_re, 0, subtestname_sz+1);
			strncpy(subtestname_re, cp+1, subtestname_sz);
		}
	}
	if (!suitename_re) {
		fprintf(stderr, "error: can't parse testsuite name: %s\n",
		    testname_re);
		return CUE_NOTEST;
	}

	/*
	 * cycle through the test suites and tests, adding
	 * ones that match
	 */
	for (sp = suites; sp->pName != NULL; sp++) {
		int suite_added = 0;
		CU_pSuite pSuite = NULL;

		if (fnmatch(suitename_re, sp->pName, 0) != 0)
		    continue;

		for (tp = sp->pTests; tp->pName != NULL; tp++) {
			if (subtestname_re)
				if (fnmatch(subtestname_re, tp->pName, 0) != 0)
					continue;
			if (!suite_added) {
				suite_added++;
				pSuite = CU_add_suite(sp->pName,
				    sp->pInitFunc, sp->pCleanupFunc);
			}
			CU_add_test(pSuite, tp->pName, tp->pTestFunc);
		}
	}

	/* all done -- clean up */
	if (suitename_re)
		free(suitename_re);
	if (subtestname_re)
		free(subtestname_re);

	return CUE_SUCCESS;
}


int
main(int argc, char *argv[])
{
	char *testname_re = NULL;
	int lun;
	CU_BasicRunMode mode = CU_BRM_VERBOSE;
	CU_ErrorAction error_action = CUEA_IGNORE;
	int res;
	struct scsi_readcapacity10 *rc10;
	struct scsi_readcapacity16 *rc16;
	struct scsi_inquiry_standard *inq;
	int full_size;
	static struct option long_opts[] = {
		{ "help", no_argument, 0, '?' },
		{ "list", no_argument, 0, 'l' },
		{ "initiator-name", required_argument, 0, 'i' },
		{ "initiator-name-2", required_argument, 0, 'I' },
		{ "test", required_argument, 0, 't' },
		{ "dataloss", no_argument, 0, 'd' },
		{ "ignore", no_argument, 0, 'g' },
		{ "fail", no_argument, 0, 'f' },
		{ "abort", no_argument, 0, 'A' },
		{ "silent", no_argument, 0, 's' },
		{ "normal", no_argument, 0, 'n' },
		{ "verbose", no_argument, 0, 'v' },
		{ NULL, 0, 0, 0 }
	};
	int c;
	int opt_idx = 0;

	while ((c = getopt_long(argc, argv, "?hli:I:t:sdgfAsnv", long_opts,
		    &opt_idx)) > 0) {
		switch (c) {
		case 'h':
		case '?':
			print_usage();
			return 0;
		case 'l':
			list_all_tests();
			return 0;
		case 'i':
			initiatorname1 = strdup(optarg);
			break;
		case 'I':
			initiatorname2 = strdup(optarg);
			break;
		case 't':
			testname_re = strdup(optarg);
			break;
		case 'd':
			data_loss++;
			break;
		case 'g':
			error_action = CUEA_IGNORE; /* default */
			break;
		case 'f':
			error_action = CUEA_FAIL;
			break;
		case 'A':
			error_action = CUEA_ABORT;
			break;
		case 's':
			mode = CU_BRM_SILENT;
			loglevel = LOG_SILENT;
			break;
		case 'n':
			mode = CU_BRM_NORMAL;
			break;
		case 'v':
			mode = CU_BRM_VERBOSE;	/* default */
			loglevel = LOG_VERBOSE;
			break;
		default:
			fprintf(stderr,
			    "error: unknown option return: %c (option %s)\n",
			    c, argv[optind]);
			return 1;
		}
	}

	if (optind < argc) {
		tgt_url = strdup(argv[optind++]);
	}
	if (optind < argc) {
		fprintf(stderr, "error: too many arguments\n");
		print_usage();
		return 1;
	}

	/* XXX why is this done? */
	real_iscsi_queue_pdu = dlsym(RTLD_NEXT, "iscsi_queue_pdu");

	if (tgt_url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		if (testname_re)
			free(testname_re);
		return 10;
	}

	iscsic = iscsi_context_login(initiatorname1, tgt_url, &lun);
	if (iscsic == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/*
	 * find the size of the LUN
	 * All devices support readcapacity10 but only some support
	 * readcapacity16
	 */
	task = iscsi_readcapacity10_sync(iscsic, lun, 0, 0);
	if (task == NULL) {
		printf("Failed to send READCAPACITY10 command: %s\n",
		    iscsi_get_error(iscsic));
		iscsi_destroy_context(iscsic);
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY10 command: failed with sense. %s\n",
		    iscsi_get_error(iscsic));
		scsi_free_scsi_task(task);
		iscsi_destroy_context(iscsic);
		return -1;
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall READCAPACITY10 data. %s\n",
		    iscsi_get_error(iscsic));
		scsi_free_scsi_task(task);
		iscsi_destroy_context(iscsic);
		return -1;
	}
	block_size = rc10->block_size;
	num_blocks = rc10->lba;
	scsi_free_scsi_task(task);

	task = iscsi_readcapacity16_sync(iscsic, lun);
	if (task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n",
		    iscsi_get_error(iscsic));
		iscsi_destroy_context(iscsic);
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		rc16 = scsi_datain_unmarshall(task);
		if (rc16 == NULL) {
			printf("failed to unmarshall READCAPACITY16 data. %s\n",
			    iscsi_get_error(iscsic));
			scsi_free_scsi_task(task);
			iscsi_destroy_context(iscsic);
			return -1;
		}
		block_size = rc16->block_length;
		num_blocks = rc16->returned_lba;
		lbpme = rc16->lbpme;
		lbppb = 1 << rc16->lbppbe;
		lbpme = rc16->lbpme;

		scsi_free_scsi_task(task);
	}

	task = iscsi_inquiry_sync(iscsic, lun, 0, 0, 64);
	if (task == NULL || task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsic));
		return -1;
	}
	full_size = scsi_datain_getfullsize(task);
	if (full_size > task->datain.size) {
		scsi_free_scsi_task(task);

		/* we need more data for the full list */
		task = iscsi_inquiry_sync(iscsic, lun, 0, 0, full_size);
		if (task == NULL) {
			printf("Inquiry command failed : %s\n",
			    iscsi_get_error(iscsic));
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

	iscsi_logout_sync(iscsic);
	iscsi_destroy_context(iscsic);

	if (CU_initialize_registry() != 0) {
		fprintf(stderr, "error: unable to initialize test registry\n");
		return 1;
	}
	if (CU_is_test_running()) {
		fprintf(stderr, "error: test suite(s) already running!?\n");
		exit(1);
	}

	if (add_tests(testname_re) != CUE_SUCCESS) {
		fprintf(stderr, "error: suite registration failed: %s\n",
		    CU_get_error_msg());
		exit(1);
	}
	CU_basic_set_mode(mode);
	CU_set_error_action(error_action);
	printf("\n");

	/*
	 * this actually runs the tests ...
	 */
	res = CU_basic_run_tests();

	printf("Tests completed with return value: %d\n", res);

	CU_cleanup_registry();
	if (testname_re)
		free(testname_re);
	free(discard_const(tgt_url));

	return 0;
}
