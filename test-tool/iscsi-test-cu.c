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

int loglevel = LOG_SILENT;

/*
 * this allows us to redefine how PDU are queued, at times, for
 * testing purposes
 */
int (*real_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

/*****************************************************************
 *
 * list of tests and test suites
 *
 *****************************************************************/
static CU_TestInfo tests_get_lba_status[] = {
	{ (char *)"testGetLBAStatusSimple", test_get_lba_status_simple },
	{ (char *)"testGetLBAStatusBeyondEol", test_get_lba_status_beyond_eol },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_inquiry[] = {
	{ (char *)"testInquiryStandard", test_inquiry_standard },
	{ (char *)"testInquiryAllocLength", test_inquiry_alloc_length},
	{ (char *)"testInquiryEVPD", test_inquiry_evpd},
	{ (char *)"testInquirySupportedVPD", test_inquiry_supported_vpd},
	{ (char *)"testInquiryMandatoryVPDSBC", test_inquiry_mandatory_vpd_sbc},
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_mandatory[] = {
	{ (char *)"testMandatorySBC", test_mandatory_sbc },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_nomedia[] = {
	{ (char *)"testNoMediaSBC", test_nomedia_sbc },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_orwrite[] = {
	{ (char *)"testOrWriteSimple", test_orwrite_simple },
	{ (char *)"testOrWriteBeyondEol", test_orwrite_beyond_eol },
	{ (char *)"testOrWriteZeroBlocks", test_orwrite_0blocks },
	{ (char *)"testOrWriteProtect", test_orwrite_wrprotect },
	{ (char *)"testOrWriteFlags", test_orwrite_flags },
	{ (char *)"testOrWriteVerify", test_orwrite_verify },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prefetch10[] = {
	{ (char *)"testPrefetch10Simple", test_prefetch10_simple },
	{ (char *)"testPrefetch10BeyondEol", test_prefetch10_beyond_eol },
	{ (char *)"testPrefetch10ZeroBlocks", test_prefetch10_0blocks },
	{ (char *)"testPrefetch10Flags", test_prefetch10_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prefetch16[] = {
	{ (char *)"testPrefetch16Simple", test_prefetch16_simple },
	{ (char *)"testPrefetch16BeyondEol", test_prefetch16_beyond_eol },
	{ (char *)"testPrefetch16ZeroBlocks", test_prefetch16_0blocks },
	{ (char *)"testPrefetch16Flags", test_prefetch16_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_preventallow[] = {
	{ (char *)"testPreventAllowSimple", test_preventallow_simple },
	{ (char *)"testPreventAllowEject", test_preventallow_eject },
	{ (char *)"testPreventAllowITNexusLoss", test_preventallow_itnexus_loss },
	{ (char *)"testPreventAllowLogout", test_preventallow_logout },
	{ (char *)"testPreventAllowWarmReset", test_preventallow_warm_reset },
	{ (char *)"testPreventAllowColdReset", test_preventallow_cold_reset },
	{ (char *)"testPreventAllowLUNReset", test_preventallow_lun_reset },
	{ (char *)"testPreventAllow2ITNexuses", test_preventallow_2_itnexuses },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_read_keys[] = {
	{ (char *)"testPrinReadKeysSimple", test_prin_read_keys_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_register[] = {
	{ (char *)"testProutRegisterSimple", test_prout_register_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_reserve[] = {
	{ (char *)"testProutReserveSimple",
	  test_prout_reserve_simple },
	{ (char *)"testProutReserveAccessEA",
	  test_prout_reserve_access_ea },
	{ (char *)"testProutReserveAccessWE",
	  test_prout_reserve_access_we },
	{ (char *)"testProutReserveAccessEARO",
	  test_prout_reserve_access_earo },
	{ (char *)"testProutReserveAccessWERO",
	  test_prout_reserve_access_wero },
	{ (char *)"testProutReserveAccessEAAR",
	  test_prout_reserve_access_eaar },
	{ (char *)"testProutReserveAccessWEAR",
	  test_prout_reserve_access_wear },
	{ (char *)"testProutReserveOwnershipEA",
	  test_prout_reserve_ownership_ea },
	{ (char *)"testProutReserveOwnershipWE",
	  test_prout_reserve_ownership_we },
	{ (char *)"testProutReserveOwnershipEARO",
	  test_prout_reserve_ownership_earo },
	{ (char *)"testProutReserveOwnershipWERO",
	  test_prout_reserve_ownership_wero },
	{ (char *)"testProutReserveOwnershipEAAR",
	  test_prout_reserve_ownership_eaar },
	{ (char *)"testProutReserveOwnershipWEAR",
	  test_prout_reserve_ownership_wear },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_serviceaction_range[] = {
	{ (char *)"testPrinServiceactionRange", test_prin_serviceaction_range },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read6[] = {
	{ (char *)"testRead6Simple", test_read6_simple },
	{ (char *)"testRead6BeyondEol", test_read6_beyond_eol },
	{ (char *)"testRead6ZeroBlocks", test_read6_0blocks },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read10[] = {
	{ (char *)"testRead10Simple", test_read10_simple },
	{ (char *)"testRead10BeyondEol", test_read10_beyond_eol },
	{ (char *)"testRead10ZeroBlocks", test_read10_0blocks },
	{ (char *)"testRead10ReadProtect", test_read10_rdprotect },
	{ (char *)"testRead10Flags", test_read10_flags },
	{ (char *)"testRead10Residuals", test_read10_residuals },
	{ (char *)"testRead10Invalid", test_read10_invalid },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read12[] = {
	{ (char *)"testRead12Simple", test_read12_simple },
	{ (char *)"testRead12BeyondEol", test_read12_beyond_eol },
	{ (char *)"testRead12ZeroBlocks", test_read12_0blocks },
	{ (char *)"testRead12ReadProtect", test_read12_rdprotect },
	{ (char *)"testRead12Flags", test_read12_flags },
	{ (char *)"testRead12Residuals", test_read12_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read16[] = {
	{ (char *)"testRead16Simple", test_read16_simple },
	{ (char *)"testRead16BeyondEol", test_read16_beyond_eol },
	{ (char *)"testRead16ZeroBlocks", test_read16_0blocks },
	{ (char *)"testRead16ReadProtect", test_read16_rdprotect },
	{ (char *)"testRead16Flags", test_read16_flags },
	{ (char *)"testRead16Residuals", test_read16_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readcapacity10[] = {
	{ (char *)"testReadCapacity10Simple", test_readcapacity10_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readcapacity16[] = {
	{ (char *)"testReadCapacity16Simple", test_readcapacity16_simple },
	{ (char *)"testReadCapacity16Alloclen", test_readcapacity16_alloclen },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readonly[] = {
	{ (char *)"testReadOnlySBC", test_readonly_sbc },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_reserve6[] = {
	{ (char *)"testReserve6Simple", test_reserve6_simple },
	{ (char *)"testReserve6_2Initiators", test_reserve6_2initiators },
	{ (char *)"testReserve6Logout", test_reserve6_logout },
	{ (char *)"testReserve6ITNexusLoss", test_reserve6_itnexus_loss },
	{ (char *)"testReserve6TargetColdReset", test_reserve6_target_cold_reset },
	{ (char *)"testReserve6TargetWarmReset", test_reserve6_target_warm_reset },
	{ (char *)"testReserve6LUNReset", test_reserve6_lun_reset },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_testunitready[] = {
	{ (char *)"testTurSimple", test_testunitready_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_startstopunit[] = {
	{ (char *)"testStartStopUnitSimple", test_startstopunit_simple },
	{ (char *)"testStartStopUnitPwrCnd", test_startstopunit_pwrcnd },
	{ (char *)"testStartStopUnitNoLoej", test_startstopunit_noloej },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_unmap[] = {
	{ (char *)"testUnmapSimple", test_unmap_simple },
	{ (char *)"testUnmapZeroBlocks", test_unmap_0blocks },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify10[] = {
	{ (char *)"testVerify10Simple", test_verify10_simple },
	{ (char *)"testVerify10BeyondEol", test_verify10_beyond_eol },
	{ (char *)"testVerify10ZeroBlocks", test_verify10_0blocks },
	{ (char *)"testVerify10VerifyProtect", test_verify10_vrprotect },
	{ (char *)"testVerify10Flags", test_verify10_flags },
	{ (char *)"testVerify10mismatch", test_verify10_mismatch },
	{ (char *)"testVerify10mismatch_no_cmp", test_verify10_mismatch_no_cmp },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify12[] = {
	{ (char *)"testVerify12Simple", test_verify12_simple },
	{ (char *)"testVerify12BeyondEol", test_verify12_beyond_eol },
	{ (char *)"testVerify12ZeroBlocks", test_verify12_0blocks },
	{ (char *)"testVerify12VerifyProtect", test_verify12_vrprotect },
	{ (char *)"testVerify12Flags", test_verify12_flags },
	{ (char *)"testVerify12mismatch", test_verify12_mismatch },
	{ (char *)"testVerify12mismatch_no_cmp", test_verify12_mismatch_no_cmp },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify16[] = {
	{ (char *)"testVerify16Simple", test_verify16_simple },
	{ (char *)"testVerify16BeyondEol", test_verify16_beyond_eol },
	{ (char *)"testVerify16ZeroBlocks", test_verify16_0blocks },
	{ (char *)"testVerify16VerifyProtect", test_verify16_vrprotect },
	{ (char *)"testVerify16Flags", test_verify16_flags },
	{ (char *)"testVerify16mismatch", test_verify16_mismatch },
	{ (char *)"testVerify16mismatch_no_cmp", test_verify16_mismatch_no_cmp },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write10[] = {
	{ (char *)"testWrite10Simple", test_write10_simple },
	{ (char *)"testWrite10BeyondEol", test_write10_beyond_eol },
	{ (char *)"testWrite10ZeroBlocks", test_write10_0blocks },
	{ (char *)"testWrite10WriteProtect", test_write10_wrprotect },
	{ (char *)"testWrite10Flags", test_write10_flags },
	{ (char *)"testWrite10Residuals", test_write10_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write12[] = {
	{ (char *)"testWrite12Simple", test_write12_simple },
	{ (char *)"testWrite12BeyondEol", test_write12_beyond_eol },
	{ (char *)"testWrite12ZeroBlocks", test_write12_0blocks },
	{ (char *)"testWrite12WriteProtect", test_write12_wrprotect },
	{ (char *)"testWrite12Flags", test_write12_flags },
	{ (char *)"testWrite12Residuals", test_write12_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write16[] = {
	{ (char *)"testWrite16Simple", test_write16_simple },
	{ (char *)"testWrite16BeyondEol", test_write16_beyond_eol },
	{ (char *)"testWrite16ZeroBlocks", test_write16_0blocks },
	{ (char *)"testWrite16WriteProtect", test_write16_wrprotect },
	{ (char *)"testWrite16Flags", test_write16_flags },
	{ (char *)"testWrite16Residuals", test_write16_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writesame10[] = {
	{ (char *)"testWriteSame10Simple", test_writesame10_simple },
	{ (char *)"testWriteSame10BeyondEol", test_writesame10_beyond_eol },
	{ (char *)"testWriteSame10ZeroBlocks", test_writesame10_0blocks },
	{ (char *)"testWriteSame10WriteProtect", test_writesame10_wrprotect },
	{ (char *)"testWriteSame10Unmap", test_writesame10_unmap },
	{ (char *)"testWriteSame10UnmapUnaligned", test_writesame10_unmap_unaligned },
	{ (char *)"testWriteSame10UnmapUntilEnd", test_writesame10_unmap_until_end },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writesame16[] = {
	{ (char *)"testWriteSame16Simple", test_writesame16_simple },
	{ (char *)"testWriteSame16BeyondEol", test_writesame16_beyond_eol },
	{ (char *)"testWriteSame16ZeroBlocks", test_writesame16_0blocks },
	{ (char *)"testWriteSame16WriteProtect", test_writesame16_wrprotect },
	{ (char *)"testWriteSame16Unmap", test_writesame16_unmap },
	{ (char *)"testWriteSame16UnmapUnaligned", test_writesame16_unmap_unaligned },
	{ (char *)"testWriteSame16UnmapUntilEnd", test_writesame16_unmap_until_end },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify10[] = {
	{ (char *)"testWriteVerify10Simple", test_writeverify10_simple },
	{ (char *)"testWriteVerify10BeyondEol", test_writeverify10_beyond_eol },
	{ (char *)"testWriteVerify10ZeroBlocks", test_writeverify10_0blocks },
	{ (char *)"testWriteVerify10WriteProtect", test_writeverify10_wrprotect },
	{ (char *)"testWriteVerify10Flags", test_writeverify10_flags },
	{ (char *)"testWriteVerify10Residuals", test_writeverify10_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify12[] = {
	{ (char *)"testWriteVerify12Simple", test_writeverify12_simple },
	{ (char *)"testWriteVerify12BeyondEol", test_writeverify12_beyond_eol },
	{ (char *)"testWriteVerify12ZeroBlocks", test_writeverify12_0blocks },
	{ (char *)"testWriteVerify12WriteProtect", test_writeverify12_wrprotect },
	{ (char *)"testWriteVerify12Flags", test_writeverify12_flags },
	{ (char *)"testWriteVerify12Residuals", test_writeverify12_residuals },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify16[] = {
	{ (char *)"testWriteVerify16Simple", test_writeverify16_simple },
	{ (char *)"testWriteVerify16BeyondEol", test_writeverify16_beyond_eol },
	{ (char *)"testWriteVerify16ZeroBlocks", test_writeverify16_0blocks },
	{ (char *)"testWriteVerify16WriteProtect", test_writeverify16_wrprotect },
	{ (char *)"testWriteVerify16Flags", test_writeverify16_flags },
	{ (char *)"testWriteVerify16Residuals", test_writeverify16_residuals },
	CU_TEST_INFO_NULL
};

static CU_SuiteInfo suites[] = {
	{ (char *)"TestGetLBAStatus", test_setup, test_teardown,
	  tests_get_lba_status },
	{ (char *)"TestInquiry", test_setup, test_teardown,
	  tests_inquiry },
	{ (char *)"TestMandatory", test_setup, test_teardown,
	  tests_mandatory },
	{ (char *)"TestNoMedia", test_setup, test_teardown,
	  tests_nomedia },
	{ (char *)"TestOrWrite", test_setup, test_teardown,
	  tests_orwrite },
	{ (char *)"TestPrefetch10", test_setup, test_teardown,
	  tests_prefetch10 },
	{ (char *)"TestPrefetch16", test_setup, test_teardown,
	  tests_prefetch16 },
	{ (char *)"TestPreventAllow", test_setup, test_teardown,
	  tests_preventallow },
	{ (char *)"TestPrinReadKeys", test_setup, test_teardown,
	  tests_prin_read_keys },
	{ (char *)"TestPrinServiceactionRange", test_setup, test_teardown,
	  tests_prin_serviceaction_range },
	{ (char *)"TestProutRegister", test_setup, test_teardown,
	  tests_prout_register },
	{ (char *)"TestProutReserve", test_setup_pgr, test_teardown_pgr,
	  tests_prout_reserve },
	{ (char *)"TestRead6", test_setup, test_teardown,
	  tests_read6 },
	{ (char *)"TestRead10", test_setup, test_teardown,
	  tests_read10 },
	{ (char *)"TestRead12", test_setup, test_teardown,
	  tests_read12 },
	{ (char *)"TestRead16", test_setup, test_teardown,
	  tests_read16 },
	{ (char *)"TestReadCapacity10", test_setup, test_teardown,
	  tests_readcapacity10 },
	{ (char *)"TestReadCapacity16", test_setup, test_teardown,
	  tests_readcapacity16 },
	{ (char *)"TestReadOnly", test_setup, test_teardown,
	  tests_readonly },
	{ (char *)"TestReserve6", test_setup, test_teardown,
	  tests_reserve6 },
	{ (char *)"TestStartStopUnit", test_setup, test_teardown,
	  tests_startstopunit },
	{ (char *)"TestTestUnitReady", test_setup, test_teardown,
	  tests_testunitready },
	{ (char *)"TestUnmap", test_setup, test_teardown,
	  tests_unmap },
	{ (char *)"TestVerify10", test_setup, test_teardown,
	  tests_verify10 },
	{ (char *)"TestVerify12", test_setup, test_teardown,
	  tests_verify12 },
	{ (char *)"TestVerify16", test_setup, test_teardown,
	  tests_verify16 },
	{ (char *)"TestWrite10", test_setup, test_teardown,
	  tests_write10 },
	{ (char *)"TestWrite12", test_setup, test_teardown,
	  tests_write12 },
	{ (char *)"TestWrite16", test_setup, test_teardown,
	  tests_write16 },
	{ (char *)"TestWriteSame10", test_setup, test_teardown,
	  tests_writesame10 },
	{ (char *)"TestWriteSame16", test_setup, test_teardown,
	  tests_writesame16 },
	{ (char *)"TestWriteVerify10", test_setup, test_teardown,
	  tests_writeverify10 },
	{ (char *)"TestWriteVerify12", test_setup, test_teardown,
	  tests_writeverify12 },
	{ (char *)"TestWriteVerify16", test_setup, test_teardown,
	  tests_writeverify16 },
	CU_SUITE_INFO_NULL
};

/*
 * globals for test setup and teardown
 */
int tgt_lun;
struct iscsi_context *iscsic;
struct scsi_task *task;
int tgt_lun2;
struct iscsi_context *iscsic2;
unsigned char *read_write_buf;


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
	    "  -d|--dataloss                    Allow destructive tests\n");
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
	fprintf(stderr,
	    "  -V|--Verbose-scsi                Enable verbose SCSI logging [default SILENT]\n");
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
test_setup_pgr(void)
{
	task = NULL;
	read_write_buf = NULL;

	iscsic = iscsi_context_login(initiatorname1, tgt_url, &tgt_lun);
	if (iscsic == NULL) {
		fprintf(stderr,
		    "error: Failed to login to target for test set-up\n");
		return 1;
	}

	iscsic2 = iscsi_context_login(initiatorname2, tgt_url, &tgt_lun2);
	if (iscsic2 == NULL) {
		fprintf(stderr,
		    "error: Failed to login to target for test set-up\n");
		iscsi_logout_sync(iscsic);
		iscsi_destroy_context(iscsic);
		iscsic = NULL;
		return 1;
	}
	return 0;
}

int
test_teardown(void)
{
	if (task) {
		scsi_free_scsi_task(task);
		task = NULL;
	}
	if (iscsic) {
		iscsi_logout_sync(iscsic);
		iscsi_destroy_context(iscsic);
		iscsic = NULL;
	}
	return 0;
}

int
test_teardown_pgr(void)
{
	test_teardown();
	if (read_write_buf != NULL) {
		free(read_write_buf);
		read_write_buf = NULL;
	}
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
		{ "Verbose-scsi", no_argument, 0, 'V' },
		{ NULL, 0, 0, 0 }
	};
	int i, c;
	int opt_idx = 0;

	while ((c = getopt_long(argc, argv, "?hli:I:t:sdgfAsnvV", long_opts,
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
			break;
		case 'n':
			mode = CU_BRM_NORMAL;
			break;
		case 'v':
			mode = CU_BRM_VERBOSE;	/* default */
			break;
		case 'V':
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
	num_blocks = rc10->lba + 1;
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
		num_blocks = rc16->returned_lba + 1;
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

	sbc3_support = 0;
	for (i = 0; i < 8; i++) {
		if (inq->version_descriptor[i] == 0x04C0) {
			sbc3_support = 1;
		}
	}

	/* if thin provisioned we also need to read the VPD page for it */
	if (lbpme != 0) {
		struct scsi_inquiry_logical_block_provisioning *inq_lbp;

		task = iscsi_inquiry_sync(iscsic, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, 64);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsic));
			return -1;
		}
		full_size = scsi_datain_getfullsize(task);
		if (full_size > task->datain.size) {
			scsi_free_scsi_task(task);

			/* we need more data for the full list */
			if ((task = iscsi_inquiry_sync(iscsic, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, full_size)) == NULL) {
				printf("Inquiry command failed : %s\n", iscsi_get_error(iscsic));
				scsi_free_scsi_task(task);
				return -1;
			}
		}

		inq_lbp = scsi_datain_unmarshall(task);
		if (inq_lbp == NULL) {
			printf("failed to unmarshall inquiry datain blob\n");
			scsi_free_scsi_task(task);
			return -1;
		}

		lbpws10 = inq_lbp->lbpws10;
		lbpws   = inq_lbp->lbpws;
		anc_sup = inq_lbp->anc_sup;

		scsi_free_scsi_task(task);
	}


	/* check if the device is write protected or not */
	task = iscsi_modesense6_sync(iscsic, lun, 0, SCSI_MODESENSE_PC_CURRENT,
				     SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES,
				     0, 255);
	if (task == NULL) {
		printf("Failed to send MODE_SENSE6 command: %s\n",
		    iscsi_get_error(iscsic));
		iscsi_destroy_context(iscsic);
		return -1;
	}
	if (task->status == SCSI_STATUS_GOOD) {
		struct scsi_mode_sense *ms;

		ms = scsi_datain_unmarshall(task);
		if (ms == NULL) {
			printf("failed to unmarshall mode sense datain blob\n");
			scsi_free_scsi_task(task);
			return -1;
		}
		readonly = !!(ms->device_specific_parameter & 0x80);
	}
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
