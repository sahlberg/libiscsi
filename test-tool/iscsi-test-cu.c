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
	{ (char *)"GetLBAStatusSimple", test_get_lba_status_simple },
	{ (char *)"GetLBAStatusBeyondEol", test_get_lba_status_beyond_eol },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_inquiry[] = {
	{ (char *)"InquiryStandard", test_inquiry_standard },
	{ (char *)"InquiryAllocLength", test_inquiry_alloc_length},
	{ (char *)"InquiryEVPD", test_inquiry_evpd},
	{ (char *)"InquiryBlockLimits", test_inquiry_block_limits},
	{ (char *)"InquiryMandatoryVPDSBC", test_inquiry_mandatory_vpd_sbc},
	{ (char *)"InquirySupportedVPD", test_inquiry_supported_vpd},
	{ (char *)"InquiryVersionDescriptors", test_inquiry_version_descriptors},
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_mandatory[] = {
	{ (char *)"MandatorySBC", test_mandatory_sbc },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_nomedia[] = {
	{ (char *)"NoMediaSBC", test_nomedia_sbc },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_orwrite[] = {
	{ (char *)"OrWriteSimple", test_orwrite_simple },
	{ (char *)"OrWriteBeyondEol", test_orwrite_beyond_eol },
	{ (char *)"OrWriteZeroBlocks", test_orwrite_0blocks },
	{ (char *)"OrWriteProtect", test_orwrite_wrprotect },
	{ (char *)"OrWriteFlags", test_orwrite_flags },
	{ (char *)"OrWriteVerify", test_orwrite_verify },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prefetch10[] = {
	{ (char *)"Prefetch10Simple", test_prefetch10_simple },
	{ (char *)"Prefetch10BeyondEol", test_prefetch10_beyond_eol },
	{ (char *)"Prefetch10ZeroBlocks", test_prefetch10_0blocks },
	{ (char *)"Prefetch10Flags", test_prefetch10_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prefetch16[] = {
	{ (char *)"Prefetch16Simple", test_prefetch16_simple },
	{ (char *)"Prefetch16BeyondEol", test_prefetch16_beyond_eol },
	{ (char *)"Prefetch16ZeroBlocks", test_prefetch16_0blocks },
	{ (char *)"Prefetch16Flags", test_prefetch16_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_preventallow[] = {
	{ (char *)"PreventAllowSimple", test_preventallow_simple },
	{ (char *)"PreventAllowEject", test_preventallow_eject },
	{ (char *)"PreventAllowITNexusLoss", test_preventallow_itnexus_loss },
	{ (char *)"PreventAllowLogout", test_preventallow_logout },
	{ (char *)"PreventAllowWarmReset", test_preventallow_warm_reset },
	{ (char *)"PreventAllowColdReset", test_preventallow_cold_reset },
	{ (char *)"PreventAllowLUNReset", test_preventallow_lun_reset },
	{ (char *)"PreventAllow2ITNexuses", test_preventallow_2_itnexuses },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_read_keys[] = {
	{ (char *)"PrinReadKeysSimple", test_prin_read_keys_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_register[] = {
	{ (char *)"ProutRegisterSimple", test_prout_register_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_reserve[] = {
	{ (char *)"ProutReserveSimple",
	  test_prout_reserve_simple },
	{ (char *)"ProutReserveAccessEA",
	  test_prout_reserve_access_ea },
	{ (char *)"ProutReserveAccessWE",
	  test_prout_reserve_access_we },
	{ (char *)"ProutReserveAccessEARO",
	  test_prout_reserve_access_earo },
	{ (char *)"ProutReserveAccessWERO",
	  test_prout_reserve_access_wero },
	{ (char *)"ProutReserveAccessEAAR",
	  test_prout_reserve_access_eaar },
	{ (char *)"ProutReserveAccessWEAR",
	  test_prout_reserve_access_wear },
	{ (char *)"ProutReserveOwnershipEA",
	  test_prout_reserve_ownership_ea },
	{ (char *)"ProutReserveOwnershipWE",
	  test_prout_reserve_ownership_we },
	{ (char *)"ProutReserveOwnershipEARO",
	  test_prout_reserve_ownership_earo },
	{ (char *)"ProutReserveOwnershipWERO",
	  test_prout_reserve_ownership_wero },
	{ (char *)"ProutReserveOwnershipEAAR",
	  test_prout_reserve_ownership_eaar },
	{ (char *)"ProutReserveOwnershipWEAR",
	  test_prout_reserve_ownership_wear },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_serviceaction_range[] = {
	{ (char *)"PrinServiceactionRange", test_prin_serviceaction_range },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read6[] = {
	{ (char *)"Read6Simple", test_read6_simple },
	{ (char *)"Read6BeyondEol", test_read6_beyond_eol },
	{ (char *)"Read6ZeroBlocks", test_read6_0blocks },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read10[] = {
	{ (char *)"Read10Simple", test_read10_simple },
	{ (char *)"Read10BeyondEol", test_read10_beyond_eol },
	{ (char *)"Read10ZeroBlocks", test_read10_0blocks },
	{ (char *)"Read10ReadProtect", test_read10_rdprotect },
	{ (char *)"Read10Flags", test_read10_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read12[] = {
	{ (char *)"Read12Simple", test_read12_simple },
	{ (char *)"Read12BeyondEol", test_read12_beyond_eol },
	{ (char *)"Read12ZeroBlocks", test_read12_0blocks },
	{ (char *)"Read12ReadProtect", test_read12_rdprotect },
	{ (char *)"Read12Flags", test_read12_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read16[] = {
	{ (char *)"Read16Simple", test_read16_simple },
	{ (char *)"Read16BeyondEol", test_read16_beyond_eol },
	{ (char *)"Read16ZeroBlocks", test_read16_0blocks },
	{ (char *)"Read16ReadProtect", test_read16_rdprotect },
	{ (char *)"Read16Flags", test_read16_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readcapacity10[] = {
	{ (char *)"ReadCapacity10Simple", test_readcapacity10_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readcapacity16[] = {
	{ (char *)"ReadCapacity16Simple", test_readcapacity16_simple },
	{ (char *)"ReadCapacity16Alloclen", test_readcapacity16_alloclen },
	{ (char *)"ReadCapacity16PI", test_readcapacity16_protection },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readonly[] = {
	{ (char *)"ReadOnlySBC", test_readonly_sbc },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_reserve6[] = {
	{ (char *)"Reserve6Simple", test_reserve6_simple },
	{ (char *)"Reserve6_2Initiators", test_reserve6_2initiators },
	{ (char *)"Reserve6Logout", test_reserve6_logout },
	{ (char *)"Reserve6ITNexusLoss", test_reserve6_itnexus_loss },
	{ (char *)"Reserve6TargetColdReset", test_reserve6_target_cold_reset },
	{ (char *)"Reserve6TargetWarmReset", test_reserve6_target_warm_reset },
	{ (char *)"Reserve6LUNReset", test_reserve6_lun_reset },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_testunitready[] = {
	{ (char *)"TurSimple", test_testunitready_simple },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_startstopunit[] = {
	{ (char *)"StartStopUnitSimple", test_startstopunit_simple },
	{ (char *)"StartStopUnitPwrCnd", test_startstopunit_pwrcnd },
	{ (char *)"StartStopUnitNoLoej", test_startstopunit_noloej },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_unmap[] = {
	{ (char *)"UnmapSimple", test_unmap_simple },
	{ (char *)"UnmapVPD", test_unmap_vpd },
	{ (char *)"UnmapZeroBlocks", test_unmap_0blocks },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify10[] = {
	{ (char *)"Verify10Simple", test_verify10_simple },
	{ (char *)"Verify10BeyondEol", test_verify10_beyond_eol },
	{ (char *)"Verify10ZeroBlocks", test_verify10_0blocks },
	{ (char *)"Verify10VerifyProtect", test_verify10_vrprotect },
	{ (char *)"Verify10Flags", test_verify10_flags },
	{ (char *)"Verify10mismatch", test_verify10_mismatch },
	{ (char *)"Verify10mismatch_no_cmp", test_verify10_mismatch_no_cmp },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify12[] = {
	{ (char *)"Verify12Simple", test_verify12_simple },
	{ (char *)"Verify12BeyondEol", test_verify12_beyond_eol },
	{ (char *)"Verify12ZeroBlocks", test_verify12_0blocks },
	{ (char *)"Verify12VerifyProtect", test_verify12_vrprotect },
	{ (char *)"Verify12Flags", test_verify12_flags },
	{ (char *)"Verify12mismatch", test_verify12_mismatch },
	{ (char *)"Verify12mismatch_no_cmp", test_verify12_mismatch_no_cmp },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify16[] = {
	{ (char *)"Verify16Simple", test_verify16_simple },
	{ (char *)"Verify16BeyondEol", test_verify16_beyond_eol },
	{ (char *)"Verify16ZeroBlocks", test_verify16_0blocks },
	{ (char *)"Verify16VerifyProtect", test_verify16_vrprotect },
	{ (char *)"Verify16Flags", test_verify16_flags },
	{ (char *)"Verify16mismatch", test_verify16_mismatch },
	{ (char *)"Verify16mismatch_no_cmp", test_verify16_mismatch_no_cmp },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write10[] = {
	{ (char *)"Write10Simple", test_write10_simple },
	{ (char *)"Write10BeyondEol", test_write10_beyond_eol },
	{ (char *)"Write10ZeroBlocks", test_write10_0blocks },
	{ (char *)"Write10WriteProtect", test_write10_wrprotect },
	{ (char *)"Write10Flags", test_write10_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write12[] = {
	{ (char *)"Write12Simple", test_write12_simple },
	{ (char *)"Write12BeyondEol", test_write12_beyond_eol },
	{ (char *)"Write12ZeroBlocks", test_write12_0blocks },
	{ (char *)"Write12WriteProtect", test_write12_wrprotect },
	{ (char *)"Write12Flags", test_write12_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write16[] = {
	{ (char *)"Write16Simple", test_write16_simple },
	{ (char *)"Write16BeyondEol", test_write16_beyond_eol },
	{ (char *)"Write16ZeroBlocks", test_write16_0blocks },
	{ (char *)"Write16WriteProtect", test_write16_wrprotect },
	{ (char *)"Write16Flags", test_write16_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writesame10[] = {
	{ (char *)"WriteSame10Simple", test_writesame10_simple },
	{ (char *)"WriteSame10BeyondEol", test_writesame10_beyond_eol },
	{ (char *)"WriteSame10ZeroBlocks", test_writesame10_0blocks },
	{ (char *)"WriteSame10WriteProtect", test_writesame10_wrprotect },
	{ (char *)"WriteSame10Unmap", test_writesame10_unmap },
	{ (char *)"WriteSame10UnmapUnaligned", test_writesame10_unmap_unaligned },
	{ (char *)"WriteSame10UnmapUntilEnd", test_writesame10_unmap_until_end },
	{ (char *)"WriteSame10UnmapVPD", test_writesame10_unmap_vpd },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writesame16[] = {
	{ (char *)"WriteSame16Simple", test_writesame16_simple },
	{ (char *)"WriteSame16BeyondEol", test_writesame16_beyond_eol },
	{ (char *)"WriteSame16ZeroBlocks", test_writesame16_0blocks },
	{ (char *)"WriteSame16WriteProtect", test_writesame16_wrprotect },
	{ (char *)"WriteSame16Unmap", test_writesame16_unmap },
	{ (char *)"WriteSame16UnmapUnaligned", test_writesame16_unmap_unaligned },
	{ (char *)"WriteSame16UnmapUntilEnd", test_writesame16_unmap_until_end },
	{ (char *)"WriteSame16UnmapVPD", test_writesame16_unmap_vpd },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify10[] = {
	{ (char *)"WriteVerify10Simple", test_writeverify10_simple },
	{ (char *)"WriteVerify10BeyondEol", test_writeverify10_beyond_eol },
	{ (char *)"WriteVerify10ZeroBlocks", test_writeverify10_0blocks },
	{ (char *)"WriteVerify10WriteProtect", test_writeverify10_wrprotect },
	{ (char *)"WriteVerify10Flags", test_writeverify10_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify12[] = {
	{ (char *)"WriteVerify12Simple", test_writeverify12_simple },
	{ (char *)"WriteVerify12BeyondEol", test_writeverify12_beyond_eol },
	{ (char *)"WriteVerify12ZeroBlocks", test_writeverify12_0blocks },
	{ (char *)"WriteVerify12WriteProtect", test_writeverify12_wrprotect },
	{ (char *)"WriteVerify12Flags", test_writeverify12_flags },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify16[] = {
	{ (char *)"WriteVerify16Simple", test_writeverify16_simple },
	{ (char *)"WriteVerify16BeyondEol", test_writeverify16_beyond_eol },
	{ (char *)"WriteVerify16ZeroBlocks", test_writeverify16_0blocks },
	{ (char *)"WriteVerify16WriteProtect", test_writeverify16_wrprotect },
	{ (char *)"WriteVerify16Flags", test_writeverify16_flags },
	CU_TEST_INFO_NULL
};


/* SCSI protocol tests */
static CU_SuiteInfo scsi_suites[] = {
	{ (char *)"GetLBAStatus", test_setup, test_teardown,
	  tests_get_lba_status },
	{ (char *)"Inquiry", test_setup, test_teardown,
	  tests_inquiry },
	{ (char *)"Mandatory", test_setup, test_teardown,
	  tests_mandatory },
	{ (char *)"NoMedia", test_setup, test_teardown,
	  tests_nomedia },
	{ (char *)"OrWrite", test_setup, test_teardown,
	  tests_orwrite },
	{ (char *)"Prefetch10", test_setup, test_teardown,
	  tests_prefetch10 },
	{ (char *)"Prefetch16", test_setup, test_teardown,
	  tests_prefetch16 },
	{ (char *)"PreventAllow", test_setup, test_teardown,
	  tests_preventallow },
	{ (char *)"PrinReadKeys", test_setup, test_teardown,
	  tests_prin_read_keys },
	{ (char *)"PrinServiceactionRange", test_setup, test_teardown,
	  tests_prin_serviceaction_range },
	{ (char *)"ProutRegister", test_setup, test_teardown,
	  tests_prout_register },
	{ (char *)"ProutReserve", test_setup_pgr, test_teardown_pgr,
	  tests_prout_reserve },
	{ (char *)"Read6", test_setup, test_teardown,
	  tests_read6 },
	{ (char *)"Read10", test_setup, test_teardown,
	  tests_read10 },
	{ (char *)"Read12", test_setup, test_teardown,
	  tests_read12 },
	{ (char *)"Read16", test_setup, test_teardown,
	  tests_read16 },
	{ (char *)"ReadCapacity10", test_setup, test_teardown,
	  tests_readcapacity10 },
	{ (char *)"ReadCapacity16", test_setup, test_teardown,
	  tests_readcapacity16 },
	{ (char *)"ReadOnly", test_setup, test_teardown,
	  tests_readonly },
	{ (char *)"Reserve6", test_setup, test_teardown,
	  tests_reserve6 },
	{ (char *)"StartStopUnit", test_setup, test_teardown,
	  tests_startstopunit },
	{ (char *)"UnitReady", test_setup, test_teardown,
	  tests_testunitready },
	{ (char *)"Unmap", test_setup, test_teardown,
	  tests_unmap },
	{ (char *)"Verify10", test_setup, test_teardown,
	  tests_verify10 },
	{ (char *)"Verify12", test_setup, test_teardown,
	  tests_verify12 },
	{ (char *)"Verify16", test_setup, test_teardown,
	  tests_verify16 },
	{ (char *)"Write10", test_setup, test_teardown,
	  tests_write10 },
	{ (char *)"Write12", test_setup, test_teardown,
	  tests_write12 },
	{ (char *)"Write16", test_setup, test_teardown,
	  tests_write16 },
	{ (char *)"WriteSame10", test_setup, test_teardown,
	  tests_writesame10 },
	{ (char *)"WriteSame16", test_setup, test_teardown,
	  tests_writesame16 },
	{ (char *)"WriteVerify10", test_setup, test_teardown,
	  tests_writeverify10 },
	{ (char *)"WriteVerify12", test_setup, test_teardown,
	  tests_writeverify12 },
	{ (char *)"WriteVerify16", test_setup, test_teardown,
	  tests_writeverify16 },
	CU_SUITE_INFO_NULL
};

static CU_TestInfo tests_iscsi_cmdsn[] = {
	{ (char *)"iSCSICmdSnTooHigh", test_iscsi_cmdsn_toohigh },
	{ (char *)"iSCSICmdSnTooLow", test_iscsi_cmdsn_toolow },
	CU_TEST_INFO_NULL
};

static CU_TestInfo tests_iscsi_residuals[] = {
	{ (char *)"Read10Invalid", test_read10_invalid },
	{ (char *)"Read10Residuals", test_read10_residuals },
	{ (char *)"Read12Residuals", test_read12_residuals },
	{ (char *)"Read16Residuals", test_read16_residuals },
	{ (char *)"Write10Residuals", test_write10_residuals },
	{ (char *)"Write12Residuals", test_write12_residuals },
	{ (char *)"Write16Residuals", test_write16_residuals },
	{ (char *)"WriteVerify10Residuals", test_writeverify10_residuals },
	{ (char *)"WriteVerify12Residuals", test_writeverify12_residuals },
	{ (char *)"WriteVerify16Residuals", test_writeverify16_residuals },
	CU_TEST_INFO_NULL
};

/* iSCSI protocol tests */
static CU_SuiteInfo iscsi_suites[] = {
	{ (char *)"iSCSIcmdsn", test_setup, test_teardown,
	  tests_iscsi_cmdsn },
	{ (char *)"iSCSIResiduals", test_setup, test_teardown,
	  tests_iscsi_residuals },
	CU_SUITE_INFO_NULL
};

/* All tests */
static CU_SuiteInfo all_suites[] = {
	{ (char *)"GetLBAStatus", test_setup, test_teardown,
	  tests_get_lba_status },
	{ (char *)"Inquiry", test_setup, test_teardown,
	  tests_inquiry },
	{ (char *)"Mandatory", test_setup, test_teardown,
	  tests_mandatory },
	{ (char *)"NoMedia", test_setup, test_teardown,
	  tests_nomedia },
	{ (char *)"OrWrite", test_setup, test_teardown,
	  tests_orwrite },
	{ (char *)"Prefetch10", test_setup, test_teardown,
	  tests_prefetch10 },
	{ (char *)"Prefetch16", test_setup, test_teardown,
	  tests_prefetch16 },
	{ (char *)"PreventAllow", test_setup, test_teardown,
	  tests_preventallow },
	{ (char *)"PrinReadKeys", test_setup, test_teardown,
	  tests_prin_read_keys },
	{ (char *)"PrinServiceactionRange", test_setup, test_teardown,
	  tests_prin_serviceaction_range },
	{ (char *)"ProutRegister", test_setup, test_teardown,
	  tests_prout_register },
	{ (char *)"ProutReserve", test_setup_pgr, test_teardown_pgr,
	  tests_prout_reserve },
	{ (char *)"Read6", test_setup, test_teardown,
	  tests_read6 },
	{ (char *)"Read10", test_setup, test_teardown,
	  tests_read10 },
	{ (char *)"Read12", test_setup, test_teardown,
	  tests_read12 },
	{ (char *)"Read16", test_setup, test_teardown,
	  tests_read16 },
	{ (char *)"ReadCapacity10", test_setup, test_teardown,
	  tests_readcapacity10 },
	{ (char *)"ReadCapacity16", test_setup, test_teardown,
	  tests_readcapacity16 },
	{ (char *)"ReadOnly", test_setup, test_teardown,
	  tests_readonly },
	{ (char *)"Reserve6", test_setup, test_teardown,
	  tests_reserve6 },
	{ (char *)"StartStopUnit", test_setup, test_teardown,
	  tests_startstopunit },
	{ (char *)"TestUnitReady", test_setup, test_teardown,
	  tests_testunitready },
	{ (char *)"Unmap", test_setup, test_teardown,
	  tests_unmap },
	{ (char *)"Verify10", test_setup, test_teardown,
	  tests_verify10 },
	{ (char *)"Verify12", test_setup, test_teardown,
	  tests_verify12 },
	{ (char *)"Verify16", test_setup, test_teardown,
	  tests_verify16 },
	{ (char *)"Write10", test_setup, test_teardown,
	  tests_write10 },
	{ (char *)"Write12", test_setup, test_teardown,
	  tests_write12 },
	{ (char *)"Write16", test_setup, test_teardown,
	  tests_write16 },
	{ (char *)"WriteSame10", test_setup, test_teardown,
	  tests_writesame10 },
	{ (char *)"WriteSame16", test_setup, test_teardown,
	  tests_writesame16 },
	{ (char *)"WriteVerify10", test_setup, test_teardown,
	  tests_writeverify10 },
	{ (char *)"WriteVerify12", test_setup, test_teardown,
	  tests_writeverify12 },
	{ (char *)"WriteVerify16", test_setup, test_teardown,
	  tests_writeverify16 },
	{ (char *)"iSCSIcmdsn", test_setup, test_teardown,
	  tests_iscsi_cmdsn },
	{ (char *)"iSCSIResiduals", test_setup, test_teardown,
	  tests_iscsi_residuals },
	CU_SUITE_INFO_NULL
};

static CU_SuiteInfo scsi_usb_sbc_suites[] = {
	{ (char *)"GetLBAStatus", test_setup, test_teardown,
	  tests_get_lba_status },
	{ (char *)"Inquiry", test_setup, test_teardown,
	  tests_inquiry },
	{ (char *)"Mandatory", test_setup, test_teardown,
	  tests_mandatory },
	{ (char *)"OrWrite", test_setup, test_teardown,
	  tests_orwrite },
	{ (char *)"Prefetch10", test_setup, test_teardown,
	  tests_prefetch10 },
	{ (char *)"Prefetch16", test_setup, test_teardown,
	  tests_prefetch16 },
	{ (char *)"PrinReadKeys", test_setup, test_teardown,
	  tests_prin_read_keys },
	{ (char *)"PrinServiceactionRange", test_setup, test_teardown,
	  tests_prin_serviceaction_range },
	{ (char *)"ProutRegister", test_setup, test_teardown,
	  tests_prout_register },
	{ (char *)"ProutReserve", test_setup_pgr, test_teardown_pgr,
	  tests_prout_reserve },
	{ (char *)"Read6", test_setup, test_teardown,
	  tests_read6 },
	{ (char *)"Read10", test_setup, test_teardown,
	  tests_read10 },
	{ (char *)"Read12", test_setup, test_teardown,
	  tests_read12 },
	{ (char *)"Read16", test_setup, test_teardown,
	  tests_read16 },
	{ (char *)"ReadCapacity10", test_setup, test_teardown,
	  tests_readcapacity10 },
	{ (char *)"ReadCapacity16", test_setup, test_teardown,
	  tests_readcapacity16 },
	{ (char *)"ReadOnly", test_setup, test_teardown,
	  tests_readonly },
	{ (char *)"Reserve6", test_setup, test_teardown,
	  tests_reserve6 },
	{ (char *)"TestUnitReady", test_setup, test_teardown,
	  tests_testunitready },
	{ (char *)"Unmap", test_setup, test_teardown,
	  tests_unmap },
	{ (char *)"Verify10", test_setup, test_teardown,
	  tests_verify10 },
	{ (char *)"Verify12", test_setup, test_teardown,
	  tests_verify12 },
	{ (char *)"Verify16", test_setup, test_teardown,
	  tests_verify16 },
	{ (char *)"Write10", test_setup, test_teardown,
	  tests_write10 },
	{ (char *)"Write12", test_setup, test_teardown,
	  tests_write12 },
	{ (char *)"Write16", test_setup, test_teardown,
	  tests_write16 },
	{ (char *)"WriteSame10", test_setup, test_teardown,
	  tests_writesame10 },
	{ (char *)"WriteSame16", test_setup, test_teardown,
	  tests_writesame16 },
	{ (char *)"WriteVerify10", test_setup, test_teardown,
	  tests_writeverify10 },
	{ (char *)"WriteVerify12", test_setup, test_teardown,
	  tests_writeverify12 },
	{ (char *)"WriteVerify16", test_setup, test_teardown,
	  tests_writeverify16 },
	CU_SUITE_INFO_NULL
};

struct test_family {
       const char *name;
       CU_SuiteInfo *suites;
};

static struct test_family families[] = {
       { "ALL",		     all_suites },
       { "SCSI",	     scsi_suites },
       { "iSCSI",	     iscsi_suites },
       { "SCSI-USB-SBC",     scsi_usb_sbc_suites },
       { NULL, NULL}
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
	    "  -u|--usb                         The device is attached to a USB bus.\n"
	    "                                   Additional restrictions apply, such as maximum transfer length 120kb.\n");
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
	struct test_family *fp;
	CU_SuiteInfo *sp;
	CU_TestInfo *tp;

	for (fp = families; fp->name; fp++) {
		printf("%s\n", fp->name);
		for (sp = fp->suites; sp->pName != NULL; sp++) {
			printf("%s.%s\n", fp->name,sp->pName);
			for (tp = sp->pTests; tp->pName != NULL; tp++) {
				printf("%s.%s.%s\n", fp->name,sp->pName,
					tp->pName);
			}
		}
	}
}


static CU_ErrorCode
add_tests(const char *testname_re)
{
	char *family_re = NULL;
	const char *suite_re = NULL;
	const char *test_re = NULL;
	char *cp;
	struct test_family *fp;
	CU_SuiteInfo *sp;
	CU_TestInfo *tp;


	/* if not testname(s) register all tests and return */
	if (!testname_re) {
		return CU_register_suites(all_suites);
	}

	/*
	 * break testname_re into family/suite/test
	 *
	 * syntax is:  FAMILY[.SUITE[.TEST]]
	 */
	family_re = strdup(testname_re);
	if ((cp = strchr(family_re, '.')) != NULL) {
		*cp++ = 0;
		suite_re = cp;
		if ((cp = strchr(suite_re, '.')) != NULL) {
			*cp++ = 0;
			test_re = cp;
		}
	}
	if (suite_re == NULL) {
		suite_re = "*";
	}
	if (test_re == NULL) {
		test_re = "*";
	}

	if (!family_re) {
		fprintf(stderr, "error: can't parse test family name: %s\n",
		    family_re);
		return CUE_NOTEST;
	}

	/*
	 * cycle through the test families/suites/tests, adding
	 * ones that match
	 */
	for (fp = families; fp->name; fp++) {
		if (fnmatch(family_re, fp->name, 0) != 0)
		    continue;

		for (sp = fp->suites; sp->pName != NULL; sp++) {
			int suite_added = 0;
			CU_pSuite pSuite = NULL;

			if (fnmatch(suite_re, sp->pName, 0) != 0)
				continue;

			for (tp = sp->pTests; tp->pName != NULL; tp++) {
				if (fnmatch(test_re, tp->pName, 0) != 0) {
					continue;
				}
				if (!suite_added) {
					suite_added++;
					pSuite = CU_add_suite(sp->pName,
						sp->pInitFunc, sp->pCleanupFunc);
				}
				CU_add_test(pSuite, tp->pName, tp->pTestFunc);
			}
		}
	}

	/* all done -- clean up */
	if (family_re)
		free(family_re);

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
	struct scsi_task *inq_task;
	struct scsi_task *inq_lbp_task = NULL;
	struct scsi_task *rc16_task;
	int full_size;
	int is_usb;
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
		{ "usb", no_argument, 0, 'u' },
		{ "verbose", no_argument, 0, 'v' },
		{ "Verbose-scsi", no_argument, 0, 'V' },
		{ NULL, 0, 0, 0 }
	};
	int i, c;
	int opt_idx = 0;

	while ((c = getopt_long(argc, argv, "?hli:I:t:sdgfAsnuvV", long_opts,
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
		case 'u':
			is_usb = 1;
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

	rc16_task = iscsi_readcapacity16_sync(iscsic, lun);
	if (rc16_task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n",
		    iscsi_get_error(iscsic));
		iscsi_destroy_context(iscsic);
		return -1;
	}
	if (rc16_task->status == SCSI_STATUS_GOOD) {
		rc16 = scsi_datain_unmarshall(rc16_task);
		if (rc16 == NULL) {
			printf("failed to unmarshall READCAPACITY16 data. %s\n",
			    iscsi_get_error(iscsic));
			scsi_free_scsi_task(rc16_task);
			iscsi_destroy_context(iscsic);
			return -1;
		}
		block_size = rc16->block_length;
		num_blocks = rc16->returned_lba + 1;
		lbppb = 1 << rc16->lbppbe;
	}

	inq_task = iscsi_inquiry_sync(iscsic, lun, 0, 0, 64);
	if (inq_task == NULL || inq_task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsic));
		return -1;
	}
	full_size = scsi_datain_getfullsize(inq_task);
	if (full_size > inq_task->datain.size) {
		scsi_free_scsi_task(inq_task);

		/* we need more data for the full list */
		inq_task = iscsi_inquiry_sync(iscsic, lun, 0, 0, full_size);
		if (inq_task == NULL) {
			printf("Inquiry command failed : %s\n",
			    iscsi_get_error(iscsic));
			return -1;
		}
	}
	inq = scsi_datain_unmarshall(inq_task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(inq_task);
		return -1;
	}

	sbc3_support = 0;
	for (i = 0; i < 8; i++) {
		if (inq->version_descriptor[i] == 0x04C0) {
			sbc3_support = 1;
		}
	}

	/* if thin provisioned we also need to read the VPD page for it */
	if (rc16 && rc16->lbpme != 0){
		inq_lbp_task = iscsi_inquiry_sync(iscsic, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, 64);
		if (inq_lbp_task == NULL || inq_lbp_task->status != SCSI_STATUS_GOOD) {
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsic));
			return -1;
		}
		full_size = scsi_datain_getfullsize(inq_lbp_task);
		if (full_size > inq_lbp_task->datain.size) {
			scsi_free_scsi_task(inq_lbp_task);

			/* we need more data for the full list */
			if ((inq_lbp_task = iscsi_inquiry_sync(iscsic, lun, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, full_size)) == NULL) {
				printf("Inquiry command failed : %s\n", iscsi_get_error(iscsic));
				return -1;
			}
		}

		inq_lbp = scsi_datain_unmarshall(inq_lbp_task);
		if (inq_lbp == NULL) {
			printf("failed to unmarshall inquiry datain blob\n");
			return -1;
		}
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

	if (is_usb) {
		printf("USB device. Clamping maximum transfer length to 120k\n");
		maximum_transfer_length = 120 *1024 / block_size;
	}

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

	scsi_free_scsi_task(inq_task);
	//scsi_free_scsi_task(inq_lbp_task);
	scsi_free_scsi_task(rc16_task);

	return 0;
}
