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

#include "config.h"

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <fnmatch.h>
#include <ctype.h>

#ifdef HAVE_SG_IO
#include <fcntl.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <sys/mount.h>
#endif

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/Automated.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"

#include "iscsi-support.h"
#include "iscsi-test-cu.h"
#include "iscsi-support.h"
#include "iscsi-multipath.h"

#define        PROG        "iscsi-test-cu"

int loglevel = LOG_NORMAL;
struct scsi_device *sd = NULL;        /* mp_sds[0] alias */
static unsigned int maxsectors;

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
static CU_TestInfo tests_compareandwrite[] = {
        { (char *)"Simple", test_compareandwrite_simple },
        { (char *)"DpoFua", test_compareandwrite_dpofua },
        { (char *)"Miscompare", test_compareandwrite_miscompare },
        { (char *)"Unwritten", test_compareandwrite_unwritten },
        { (char *)"InvalidDataOutSize",
          test_compareandwrite_invalid_dataout_size },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_get_lba_status[] = {
        { (char *)"Simple", test_get_lba_status_simple },
        { (char *)"BeyondEol", test_get_lba_status_beyond_eol },
        { (char *)"UnmapSingle", test_get_lba_status_unmap_single },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_inquiry[] = {
        { (char *)"Standard", test_inquiry_standard },
        { (char *)"AllocLength", test_inquiry_alloc_length},
        { (char *)"EVPD", test_inquiry_evpd},
        { (char *)"BlockLimits", test_inquiry_block_limits},
        { (char *)"MandatoryVPDSBC", test_inquiry_mandatory_vpd_sbc},
        { (char *)"SupportedVPD", test_inquiry_supported_vpd},
        { (char *)"VersionDescriptors", test_inquiry_version_descriptors},
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_mandatory[] = {
        { (char *)"MandatorySBC", test_mandatory_sbc },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_modesense6[] = {
        { (char *)"AllPages", test_modesense6_all_pages },
        { (char *)"Control", test_modesense6_control },
        { (char *)"Control-D_SENSE", test_modesense6_control_d_sense },
        { (char *)"Control-SWP", test_modesense6_control_swp },
        { (char *)"Residuals", test_modesense6_residuals },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_nomedia[] = {
        { (char *)"NoMediaSBC", test_nomedia_sbc },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_orwrite[] = {
        { (char *)"Simple", test_orwrite_simple },
        { (char *)"BeyondEol", test_orwrite_beyond_eol },
        { (char *)"ZeroBlocks", test_orwrite_0blocks },
        { (char *)"Protect", test_orwrite_wrprotect },
        { (char *)"DpoFua", test_orwrite_dpofua },
        { (char *)"Verify", test_orwrite_verify },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prefetch10[] = {
        { (char *)"Simple", test_prefetch10_simple },
        { (char *)"BeyondEol", test_prefetch10_beyond_eol },
        { (char *)"ZeroBlocks", test_prefetch10_0blocks },
        { (char *)"Flags", test_prefetch10_flags },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prefetch16[] = {
        { (char *)"Simple", test_prefetch16_simple },
        { (char *)"BeyondEol", test_prefetch16_beyond_eol },
        { (char *)"ZeroBlocks", test_prefetch16_0blocks },
        { (char *)"Flags", test_prefetch16_flags },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_preventallow[] = {
        { (char *)"Simple", test_preventallow_simple },
        { (char *)"Eject", test_preventallow_eject },
        { (char *)"ITNexusLoss", test_preventallow_itnexus_loss },
        { (char *)"Logout", test_preventallow_logout },
        { (char *)"WarmReset", test_preventallow_warm_reset },
        { (char *)"ColdReset", test_preventallow_cold_reset },
        { (char *)"LUNReset", test_preventallow_lun_reset },
        { (char *)"2ITNexuses", test_preventallow_2_itnexuses },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_read_keys[] = {
        { (char *)"Simple", test_prin_read_keys_simple },
        { (char *)"Truncate", test_prin_read_keys_truncate },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_report_caps[] = {
        { (char *)"Simple", test_prin_report_caps_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_register[] = {
        { (char *)"Simple", test_prout_register_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_reserve[] = {
        { (char *)"Simple",
          test_prout_reserve_simple },
        { (char *)"AccessEA",
          test_prout_reserve_access_ea },
        { (char *)"AccessWE",
          test_prout_reserve_access_we },
        { (char *)"AccessEARO",
          test_prout_reserve_access_earo },
        { (char *)"AccessWERO",
          test_prout_reserve_access_wero },
        { (char *)"AccessEAAR",
          test_prout_reserve_access_eaar },
        { (char *)"AccessWEAR",
          test_prout_reserve_access_wear },
        { (char *)"OwnershipEA",
          test_prout_reserve_ownership_ea },
        { (char *)"OwnershipWE",
          test_prout_reserve_ownership_we },
        { (char *)"OwnershipEARO",
          test_prout_reserve_ownership_earo },
        { (char *)"OwnershipWERO",
          test_prout_reserve_ownership_wero },
        { (char *)"OwnershipEAAR",
          test_prout_reserve_ownership_eaar },
        { (char *)"OwnershipWEAR",
          test_prout_reserve_ownership_wear },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_clear[] = {
        { (char *)"Simple",
          test_prout_clear_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prout_preempt[] = {
        { (char *)"RemoveRegistration",
          test_prout_preempt_rm_reg },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_prin_serviceaction_range[] = {
        { (char *)"Range", test_prin_serviceaction_range },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read6[] = {
        { (char *)"Simple", test_read6_simple },
        { (char *)"BeyondEol", test_read6_beyond_eol },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read10[] = {
        { (char *)"Simple", test_read10_simple },
        { (char *)"BeyondEol", test_read10_beyond_eol },
        { (char *)"ZeroBlocks", test_read10_0blocks },
        { (char *)"ReadProtect", test_read10_rdprotect },
        { (char *)"DpoFua", test_read10_dpofua },
        { (char *)"Async", test_async_read },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read12[] = {
        { (char *)"Simple", test_read12_simple },
        { (char *)"BeyondEol", test_read12_beyond_eol },
        { (char *)"ZeroBlocks", test_read12_0blocks },
        { (char *)"ReadProtect", test_read12_rdprotect },
        { (char *)"DpoFua", test_read12_dpofua },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_read16[] = {
        { (char *)"Simple", test_read16_simple },
        { (char *)"BeyondEol", test_read16_beyond_eol },
        { (char *)"ZeroBlocks", test_read16_0blocks },
        { (char *)"ReadProtect", test_read16_rdprotect },
        { (char *)"DpoFua", test_read16_dpofua },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readcapacity10[] = {
        { (char *)"Simple", test_readcapacity10_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readcapacity16[] = {
        { (char *)"Simple", test_readcapacity16_simple },
        { (char *)"Alloclen", test_readcapacity16_alloclen },
        { (char *)"PI", test_readcapacity16_protection },
        { (char *)"Support", test_readcapacity16_support },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readdefectdata10[] = {
        { (char *)"Simple", test_readdefectdata10_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readdefectdata12[] = {
        { (char *)"Simple", test_readdefectdata12_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_readonly[] = {
        { (char *)"ReadOnlySBC", test_readonly_sbc },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_sanitize[] = {
        { (char *)"BlockErase", test_sanitize_block_erase },
        { (char *)"BlockEraseReserved", test_sanitize_block_erase_reserved },
        { (char *)"CryptoErase", test_sanitize_crypto_erase },
        { (char *)"CryptoEraseReserved", test_sanitize_crypto_erase_reserved },
        { (char *)"ExitFailureMode", test_sanitize_exit_failure_mode },
        { (char *)"InvalidServiceAction", test_sanitize_invalid_serviceaction },
        { (char *)"Overwrite", test_sanitize_overwrite },
        { (char *)"OverwriteReserved", test_sanitize_overwrite_reserved },
        { (char *)"Readonly", test_sanitize_readonly },
        { (char *)"Reservations", test_sanitize_reservations },
        { (char *)"Reset", test_sanitize_reset },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_extended_copy[] = {
        { (char *)"Simple", test_extendedcopy_simple },
        { (char *)"ParamHdr", test_extendedcopy_param },
        { (char *)"DescrLimits", test_extendedcopy_descr_limits },
        { (char *)"DescrType", test_extendedcopy_descr_type },
        { (char *)"ValidTgtDescr", test_extendedcopy_validate_tgt_descr },
        { (char *)"ValidSegDescr", test_extendedcopy_validate_seg_descr },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_receive_copy_results[] = {
        { (char *)"CopyStatus", test_receive_copy_results_copy_status },
        { (char *)"OpParams", test_receive_copy_results_op_params },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_report_supported_opcodes[] = {
        { (char *)"Simple", test_report_supported_opcodes_simple },
        { (char *)"OneCommand", test_report_supported_opcodes_one_command },
        { (char *)"RCTD", test_report_supported_opcodes_rctd },
        { (char *)"SERVACTV", test_report_supported_opcodes_servactv },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_reserve6[] = {
        { (char *)"Simple", test_reserve6_simple },
        { (char *)"2Initiators", test_reserve6_2initiators },
        { (char *)"Logout", test_reserve6_logout },
        { (char *)"ITNexusLoss", test_reserve6_itnexus_loss },
        { (char *)"TargetColdReset", test_reserve6_target_cold_reset },
        { (char *)"TargetWarmReset", test_reserve6_target_warm_reset },
        { (char *)"LUNReset", test_reserve6_lun_reset },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_testunitready[] = {
        { (char *)"Simple", test_testunitready_simple },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_startstopunit[] = {
        { (char *)"Simple", test_startstopunit_simple },
        { (char *)"PwrCnd", test_startstopunit_pwrcnd },
        { (char *)"NoLoej", test_startstopunit_noloej },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_unmap[] = {
        { (char *)"Simple", test_unmap_simple },
        { (char *)"VPD", test_unmap_vpd },
        { (char *)"ZeroBlocks", test_unmap_0blocks },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify10[] = {
        { (char *)"Simple", test_verify10_simple },
        { (char *)"BeyondEol", test_verify10_beyond_eol },
        { (char *)"ZeroBlocks", test_verify10_0blocks },
        { (char *)"VerifyProtect", test_verify10_vrprotect },
        { (char *)"Flags", test_verify10_flags },
        { (char *)"Dpo", test_verify10_dpo },
        { (char *)"Mismatch", test_verify10_mismatch },
        { (char *)"MismatchNoCmp", test_verify10_mismatch_no_cmp },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify12[] = {
        { (char *)"Simple", test_verify12_simple },
        { (char *)"BeyondEol", test_verify12_beyond_eol },
        { (char *)"ZeroBlocks", test_verify12_0blocks },
        { (char *)"VerifyProtect", test_verify12_vrprotect },
        { (char *)"Flags", test_verify12_flags },
        { (char *)"Dpo", test_verify12_dpo },
        { (char *)"Mismatch", test_verify12_mismatch },
        { (char *)"MismatchNoCmp", test_verify12_mismatch_no_cmp },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_verify16[] = {
        { (char *)"Simple", test_verify16_simple },
        { (char *)"BeyondEol", test_verify16_beyond_eol },
        { (char *)"ZeroBlocks", test_verify16_0blocks },
        { (char *)"VerifyProtect", test_verify16_vrprotect },
        { (char *)"Flags", test_verify16_flags },
        { (char *)"Dpo", test_verify16_dpo },
        { (char *)"Mismatch", test_verify16_mismatch },
        { (char *)"MismatchNoCmp", test_verify16_mismatch_no_cmp },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write10[] = {
        { (char *)"Simple", test_write10_simple },
        { (char *)"BeyondEol", test_write10_beyond_eol },
        { (char *)"ZeroBlocks", test_write10_0blocks },
        { (char *)"WriteProtect", test_write10_wrprotect },
        { (char *)"DpoFua", test_write10_dpofua },
        { (char *)"Async", test_async_write },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write12[] = {
        { (char *)"Simple", test_write12_simple },
        { (char *)"BeyondEol", test_write12_beyond_eol },
        { (char *)"ZeroBlocks", test_write12_0blocks },
        { (char *)"WriteProtect", test_write12_wrprotect },
        { (char *)"DpoFua", test_write12_dpofua },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_write16[] = {
        { (char *)"Simple", test_write16_simple },
        { (char *)"BeyondEol", test_write16_beyond_eol },
        { (char *)"ZeroBlocks", test_write16_0blocks },
        { (char *)"WriteProtect", test_write16_wrprotect },
        { (char *)"DpoFua", test_write16_dpofua },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeatomic16[] = {
        { (char *)"Simple", test_writeatomic16_simple },
        { (char *)"BeyondEol", test_writeatomic16_beyond_eol },
        { (char *)"ZeroBlocks", test_writeatomic16_0blocks },
        { (char *)"WriteProtect", test_writeatomic16_wrprotect },
        { (char *)"DpoFua", test_writeatomic16_dpofua },
        { (char *)"VPD", test_writeatomic16_vpd },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writesame10[] = {
        { (char *)"Simple", test_writesame10_simple },
        { (char *)"BeyondEol", test_writesame10_beyond_eol },
        { (char *)"ZeroBlocks", test_writesame10_0blocks },
        { (char *)"WriteProtect", test_writesame10_wrprotect },
        { (char *)"Unmap", test_writesame10_unmap },
        { (char *)"UnmapUnaligned", test_writesame10_unmap_unaligned },
        { (char *)"UnmapUntilEnd", test_writesame10_unmap_until_end },
        { (char *)"UnmapVPD", test_writesame10_unmap_vpd },
        { (char *)"Check", test_writesame10_check },
        { (char *)"InvalidDataOutSize", test_writesame10_invalid_dataout_size },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writesame16[] = {
        { (char *)"Simple", test_writesame16_simple },
        { (char *)"BeyondEol", test_writesame16_beyond_eol },
        { (char *)"ZeroBlocks", test_writesame16_0blocks },
        { (char *)"WriteProtect", test_writesame16_wrprotect },
        { (char *)"Unmap", test_writesame16_unmap },
        { (char *)"UnmapUnaligned", test_writesame16_unmap_unaligned },
        { (char *)"UnmapUntilEnd", test_writesame16_unmap_until_end },
        { (char *)"UnmapVPD", test_writesame16_unmap_vpd },
        { (char *)"Check", test_writesame16_check },
        { (char *)"InvalidDataOutSize", test_writesame16_invalid_dataout_size },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify10[] = {
        { (char *)"Simple", test_writeverify10_simple },
        { (char *)"BeyondEol", test_writeverify10_beyond_eol },
        { (char *)"ZeroBlocks", test_writeverify10_0blocks },
        { (char *)"WriteProtect", test_writeverify10_wrprotect },
        { (char *)"Flags", test_writeverify10_flags },
        { (char *)"Dpo", test_writeverify10_dpo },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify12[] = {
        { (char *)"Simple", test_writeverify12_simple },
        { (char *)"BeyondEol", test_writeverify12_beyond_eol },
        { (char *)"ZeroBlocks", test_writeverify12_0blocks },
        { (char *)"WriteProtect", test_writeverify12_wrprotect },
        { (char *)"Flags", test_writeverify12_flags },
        { (char *)"Dpo", test_writeverify12_dpo },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_writeverify16[] = {
        { (char *)"Simple", test_writeverify16_simple },
        { (char *)"BeyondEol", test_writeverify16_beyond_eol },
        { (char *)"ZeroBlocks", test_writeverify16_0blocks },
        { (char *)"WriteProtect", test_writeverify16_wrprotect },
        { (char *)"Flags", test_writeverify16_flags },
        { (char *)"Dpo", test_writeverify16_dpo },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_multipathio[] = {
        { (char *)"Simple", test_multipathio_simple },
        { (char *)"Reset", test_multipathio_reset },
        { (char *)"CompareAndWrite", test_multipathio_compareandwrite },
        { (char *)"CompareAndWriteAsync", test_mpio_async_caw },
        CU_TEST_INFO_NULL
};

typedef struct libiscsi_suite_info {
        const char       *pName;         /**< Suite name. */
        CU_InitializeFunc pInitFunc;     /**< Suite initialization function. */
        CU_CleanupFunc    pCleanupFunc;  /**< Suite cleanup function */
        CU_SetUpFunc      pSetUpFunc;    /**< Test setup function. */
        CU_TearDownFunc   pTearDownFunc; /**< Test tear down function */
        CU_TestInfo      *pTests;        /**< Test case array - must be NULL terminated. */
} libiscsi_suite_info;

#define NON_PGR_FUNCS suite_init, suite_cleanup, test_setup, test_teardown

/* SCSI protocol tests */
static libiscsi_suite_info scsi_suites[] = {
        { "CompareAndWrite", NON_PGR_FUNCS, tests_compareandwrite },
        { "ExtendedCopy", NON_PGR_FUNCS, tests_extended_copy },
        { "GetLBAStatus", NON_PGR_FUNCS, tests_get_lba_status },
        { "Inquiry", NON_PGR_FUNCS, tests_inquiry },
        { "Mandatory", NON_PGR_FUNCS, tests_mandatory },
        { "ModeSense6", NON_PGR_FUNCS, tests_modesense6 },
        { "NoMedia", NON_PGR_FUNCS, tests_nomedia },
        { "OrWrite", NON_PGR_FUNCS, tests_orwrite },
        { "Prefetch10", NON_PGR_FUNCS, tests_prefetch10 },
        { "Prefetch16", NON_PGR_FUNCS, tests_prefetch16 },
        { "PreventAllow", NON_PGR_FUNCS, tests_preventallow },
        { "PrinReadKeys", NON_PGR_FUNCS, tests_prin_read_keys },
        { "PrinServiceactionRange", NON_PGR_FUNCS, tests_prin_serviceaction_range },
        { "PrinReportCapabilities", NON_PGR_FUNCS, tests_prin_report_caps },
        { "ProutRegister", NON_PGR_FUNCS, tests_prout_register },
        { "ProutReserve", NON_PGR_FUNCS, tests_prout_reserve },
        { "ProutClear", NON_PGR_FUNCS, tests_prout_clear },
        { "ProutPreempt", NON_PGR_FUNCS, tests_prout_preempt },
        { "Read6", NON_PGR_FUNCS, tests_read6 },
        { "Read10", NON_PGR_FUNCS, tests_read10 },
        { "Read12", NON_PGR_FUNCS, tests_read12 },
        { "Read16", NON_PGR_FUNCS, tests_read16 },
        { "ReadCapacity10", NON_PGR_FUNCS, tests_readcapacity10 },
        { "ReadCapacity16", NON_PGR_FUNCS, tests_readcapacity16 },
        { "ReadDefectData10", NON_PGR_FUNCS, tests_readdefectdata10 },
        { "ReadDefectData12", NON_PGR_FUNCS, tests_readdefectdata12 },
        { "ReadOnly", NON_PGR_FUNCS, tests_readonly },
        { "ReceiveCopyResults", NON_PGR_FUNCS, tests_receive_copy_results },
        { "ReportSupportedOpcodes", NON_PGR_FUNCS,
          tests_report_supported_opcodes },
        { "Reserve6", NON_PGR_FUNCS, tests_reserve6 },
        { "Sanitize", NON_PGR_FUNCS, tests_sanitize },
        { "StartStopUnit", NON_PGR_FUNCS, tests_startstopunit },
        { "TestUnitReady", NON_PGR_FUNCS, tests_testunitready },
        { "Unmap", NON_PGR_FUNCS, tests_unmap },
        { "Verify10", NON_PGR_FUNCS, tests_verify10 },
        { "Verify12", NON_PGR_FUNCS, tests_verify12 },
        { "Verify16", NON_PGR_FUNCS, tests_verify16 },
        { "Write10", NON_PGR_FUNCS, tests_write10 },
        { "Write12", NON_PGR_FUNCS, tests_write12 },
        { "Write16", NON_PGR_FUNCS, tests_write16 },
        { "WriteAtomic16", NON_PGR_FUNCS, tests_writeatomic16 },
        { "WriteSame10", NON_PGR_FUNCS, tests_writesame10 },
        { "WriteSame16", NON_PGR_FUNCS, tests_writesame16 },
        { "WriteVerify10", NON_PGR_FUNCS, tests_writeverify10 },
        { "WriteVerify12", NON_PGR_FUNCS, tests_writeverify12 },
        { "WriteVerify16", NON_PGR_FUNCS, tests_writeverify16 },
        { "MultipathIO", NON_PGR_FUNCS, tests_multipathio },
        { NULL, NULL, NULL, NULL, NULL, NULL }
};

static CU_TestInfo tests_iscsi_cmdsn[] = {
        { (char *)"iSCSICmdSnTooHigh", test_iscsi_cmdsn_toohigh },
        { (char *)"iSCSICmdSnTooLow", test_iscsi_cmdsn_toolow },
        CU_TEST_INFO_NULL
};

static CU_TestInfo tests_iscsi_datasn[] = {
        { (char *)"iSCSIDataSnInvalid", test_iscsi_datasn_invalid },
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

static CU_TestInfo tests_iscsi_tmf[] = {
        { (char *)"AbortTaskSimpleAsync", test_async_abort_simple },
        { (char *)"LUNResetSimpleAsync", test_async_lu_reset_simple },
        CU_TEST_INFO_NULL
};

/* iSCSI protocol tests */
static libiscsi_suite_info iscsi_suites[] = {
        { "iSCSIcmdsn", NON_PGR_FUNCS,
          tests_iscsi_cmdsn },
        { "iSCSIdatasn", NON_PGR_FUNCS,
          tests_iscsi_datasn },
        { "iSCSIResiduals", NON_PGR_FUNCS,
          tests_iscsi_residuals },
	{ "iSCSITMF", NON_PGR_FUNCS,
	  tests_iscsi_tmf },
        { NULL, NULL, NULL, NULL, NULL, NULL }
};

/* All tests */
static libiscsi_suite_info all_suites[] = {
        { "CompareAndWrite", NON_PGR_FUNCS, tests_compareandwrite },
        { "ExtendedCopy", NON_PGR_FUNCS, tests_extended_copy },
        { "GetLBAStatus", NON_PGR_FUNCS, tests_get_lba_status },
        { "Inquiry", NON_PGR_FUNCS, tests_inquiry },
        { "Mandatory", NON_PGR_FUNCS, tests_mandatory },
        { "ModeSense6", NON_PGR_FUNCS, tests_modesense6 },
        { "NoMedia", NON_PGR_FUNCS, tests_nomedia },
        { "OrWrite", NON_PGR_FUNCS, tests_orwrite },
        { "Prefetch10", NON_PGR_FUNCS, tests_prefetch10 },
        { "Prefetch16", NON_PGR_FUNCS, tests_prefetch16 },
        { "PreventAllow", NON_PGR_FUNCS, tests_preventallow },
        { "PrinReadKeys", NON_PGR_FUNCS, tests_prin_read_keys },
        { "PrinServiceactionRange", NON_PGR_FUNCS,
          tests_prin_serviceaction_range },
        { "PrinReportCapabilities", NON_PGR_FUNCS, tests_prin_report_caps },
        { "ProutRegister", NON_PGR_FUNCS, tests_prout_register },
        { "ProutReserve", NON_PGR_FUNCS, tests_prout_reserve },
        { "ProutClear", NON_PGR_FUNCS, tests_prout_clear },
        { "ProutPreempt", NON_PGR_FUNCS, tests_prout_preempt },
        { "Read6", NON_PGR_FUNCS, tests_read6 },
        { "Read10", NON_PGR_FUNCS, tests_read10 },
        { "Read12", NON_PGR_FUNCS, tests_read12 },
        { "Read16", NON_PGR_FUNCS, tests_read16 },
        { "ReadCapacity10", NON_PGR_FUNCS, tests_readcapacity10 },
        { "ReadCapacity16", NON_PGR_FUNCS, tests_readcapacity16 },
        { "ReadDefectData10", NON_PGR_FUNCS, tests_readdefectdata10 },
        { "ReadDefectData12", NON_PGR_FUNCS, tests_readdefectdata12 },
        { "ReadOnly", NON_PGR_FUNCS, tests_readonly },
        { "ReceiveCopyResults", NON_PGR_FUNCS, tests_receive_copy_results },
        { "ReportSupportedOpcodes", NON_PGR_FUNCS,
          tests_report_supported_opcodes },
        { "Reserve6", NON_PGR_FUNCS, tests_reserve6 },
        { "Sanitize", NON_PGR_FUNCS, tests_sanitize },
        { "StartStopUnit", NON_PGR_FUNCS, tests_startstopunit },
        { "TestUnitReady", NON_PGR_FUNCS, tests_testunitready },
        { "Unmap", NON_PGR_FUNCS, tests_unmap },
        { "Verify10", NON_PGR_FUNCS, tests_verify10 },
        { "Verify12", NON_PGR_FUNCS, tests_verify12 },
        { "Verify16", NON_PGR_FUNCS, tests_verify16 },
        { "Write10", NON_PGR_FUNCS, tests_write10 },
        { "Write12", NON_PGR_FUNCS, tests_write12 },
        { "Write16", NON_PGR_FUNCS, tests_write16 },
        { "WriteAtomic16", NON_PGR_FUNCS, tests_writeatomic16 },
        { "WriteSame10", NON_PGR_FUNCS, tests_writesame10 },
        { "WriteSame16", NON_PGR_FUNCS, tests_writesame16 },
        { "WriteVerify10", NON_PGR_FUNCS, tests_writeverify10 },
        { "WriteVerify12", NON_PGR_FUNCS, tests_writeverify12 },
        { "WriteVerify16", NON_PGR_FUNCS, tests_writeverify16 },
        { "iSCSIcmdsn", NON_PGR_FUNCS, tests_iscsi_cmdsn },
        { "iSCSIdatasn", NON_PGR_FUNCS, tests_iscsi_datasn },
        { "iSCSIResiduals", NON_PGR_FUNCS, tests_iscsi_residuals },
	{ "iSCSITMF", NON_PGR_FUNCS, tests_iscsi_tmf },
        { "MultipathIO", NON_PGR_FUNCS, tests_multipathio },
        { NULL, NULL, NULL, NULL, NULL, NULL },
};

static libiscsi_suite_info linux_suites[] = {
        { "CompareAndWrite", NON_PGR_FUNCS, tests_compareandwrite },
        { "GetLBAStatus", NON_PGR_FUNCS, tests_get_lba_status },
        { "Inquiry", NON_PGR_FUNCS, tests_inquiry },
        { "Mandatory", NON_PGR_FUNCS, tests_mandatory },
        { "ModeSense6", NON_PGR_FUNCS, tests_modesense6 },
        { "OrWrite", NON_PGR_FUNCS, tests_orwrite },
        { "Prefetch10", NON_PGR_FUNCS, tests_prefetch10 },
        { "Prefetch16", NON_PGR_FUNCS, tests_prefetch16 },
        { "Read10", NON_PGR_FUNCS, tests_read10 },
        { "Read12", NON_PGR_FUNCS, tests_read12 },
        { "Read16", NON_PGR_FUNCS, tests_read16 },
        { "ReadCapacity10", NON_PGR_FUNCS, tests_readcapacity10 },
        { "ReadCapacity16", NON_PGR_FUNCS, tests_readcapacity16 },
        { "ReadDefectData10", NON_PGR_FUNCS, tests_readdefectdata10 },
        { "ReadDefectData12", NON_PGR_FUNCS, tests_readdefectdata12 },
        { "ReadOnly", NON_PGR_FUNCS, tests_readonly },
        { "ReportSupportedOpcodes", NON_PGR_FUNCS,
          tests_report_supported_opcodes },
        { "TestUnitReady", NON_PGR_FUNCS, tests_testunitready },
        { "Unmap", NON_PGR_FUNCS, tests_unmap },
        { "Verify10", NON_PGR_FUNCS, tests_verify10 },
        { "Verify12", NON_PGR_FUNCS, tests_verify12 },
        { "Verify16", NON_PGR_FUNCS, tests_verify16 },
        { "Write10", NON_PGR_FUNCS, tests_write10 },
        { "Write12", NON_PGR_FUNCS, tests_write12 },
        { "Write16", NON_PGR_FUNCS, tests_write16 },
        { "WriteAtomic16", NON_PGR_FUNCS, tests_writeatomic16 },
        { "WriteSame10", NON_PGR_FUNCS, tests_writesame10 },
        { "WriteSame16", NON_PGR_FUNCS, tests_writesame16 },
        { "WriteVerify10", NON_PGR_FUNCS, tests_writeverify10 },
        { "WriteVerify12", NON_PGR_FUNCS, tests_writeverify12 },
        { "WriteVerify16", NON_PGR_FUNCS, tests_writeverify16 },
        { "MultipathIO", NON_PGR_FUNCS, tests_multipathio },
        { NULL, NULL, NULL, NULL, NULL, NULL },
};

struct test_family {
       const char *name;
       libiscsi_suite_info *suites;
};

static struct test_family families[] = {
        { "ALL",                all_suites },
        { "SCSI",                scsi_suites },
        { "iSCSI",                iscsi_suites },
        { "LINUX",                linux_suites },
        { NULL, NULL}
};

/*
 * globals for test setup and teardown
 */
struct scsi_task *task;
unsigned char *read_write_buf;

static void
print_usage(void)
{
        fprintf(stderr,
            "Usage: %s [-?|--help]    print this message and exit\n",
            PROG);
        fprintf(stderr,
            "or     %s [OPTIONS] <iscsi-url> [multipath-iscsi-url]\n", PROG);
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
            "  -S|--allow-sanitize              Allow sanitize-opcode tests\n");
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
            "  -x|--xml                         Test Mode: XML\n");
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
            "and <test-name-reg-exp> is of the form: FAMILY[.SUITE[.TEST]]\n");
        fprintf(stderr, "\n");
}

/* Clear persistent reservations and reservation keys left by a previous run */
static int clear_pr(struct scsi_device *sdev)
{
        int i, res;
        struct scsi_task *pr_task;
        struct scsi_persistent_reserve_in_read_keys *rk;

        res = 0;
        if (prin_read_keys(sdev, &pr_task, &rk, 16384) != 0)
                goto out;

        res = -1;
        if (rk->num_keys && data_loss == 0)
                goto out;

        res = 0;
        for (i = 0; i < rk->num_keys; i++) {
                res = prout_register_and_ignore(sdev, rk->keys[i]);
                if (res)
                        break;
                res = prout_register_key(sdev, 0, rk->keys[i]);
                if (res)
                        break;
        }

        scsi_free_scsi_task(pr_task);

out:
        return res;
}

void
test_setup(void)
{
        task = NULL;
        read_write_buf = NULL;
        local_iscsi_queue_pdu = NULL;
}

void
test_teardown(void)
{
        free(read_write_buf);
        read_write_buf = NULL;
        scsi_free_scsi_task(task);
        task = NULL;
}

int
suite_init(void)
{
        int i;
        char const *initiatornames[MPATH_MAX_DEVS] = { initiatorname1, initiatorname2 };

        for (i = 0; i < mp_num_sds; i++) {
                if (!mp_sds[i]->iscsi_url) {
                        continue;
                }
                if (mp_sds[i]->iscsi_ctx) {
                        iscsi_logout_sync(mp_sds[i]->iscsi_ctx);
                        iscsi_destroy_context(mp_sds[i]->iscsi_ctx);
                }
                mp_sds[i]->iscsi_ctx = iscsi_context_login(initiatornames[i],
                                                        mp_sds[i]->iscsi_url,
                                                        &mp_sds[i]->iscsi_lun);
                if (mp_sds[i]->iscsi_ctx == NULL) {
                        fprintf(stderr,
                                "error: Failed to login to target for test set-up\n");
                        return 1;
                }
                iscsi_set_no_ua_on_reconnect(mp_sds[i]->iscsi_ctx, 1);
        }
#ifndef HAVE_CU_SUITEINFO_PSETUPFUNC
        /* libcunit version 1 */
        test_setup();
#endif
        return 0;
}

int
suite_cleanup(void)
{
        int i;

#ifndef HAVE_CU_SUITEINFO_PSETUPFUNC
        /* libcunit version 1 */
        test_teardown();
#endif
        for (i = 0; i < mp_num_sds; i++) {
                if (mp_sds[i]->iscsi_url) {
                        if (mp_sds[i]->iscsi_ctx) {
                                clear_pr(mp_sds[i]);
                                iscsi_logout_sync(mp_sds[i]->iscsi_ctx);
                                iscsi_destroy_context(mp_sds[i]->iscsi_ctx);
                                mp_sds[i]->iscsi_ctx = NULL;
                        }
                }
        }
        return 0;
}

static void
list_all_tests(void)
{
        struct test_family *fp;
        libiscsi_suite_info *sp;
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
        char *suite_re = NULL;
        char *test_re = NULL;
        char *cp;
        struct test_family *fp;
        libiscsi_suite_info *sp;
        CU_TestInfo *tp;


        /* if not testname(s) register all tests */
        if (!testname_re) {
                family_re = strdup("*");
                suite_re = strdup("*");
                test_re = strdup("*");
        } else {
                /*
                 * break testname_re into family/suite/test
                 *
                 * syntax is:  FAMILY[.SUITE[.TEST]]
                 */
                family_re = strdup(testname_re);
                if ((cp = strchr(family_re, '.')) != NULL) {
                        *cp++ = 0;
                        suite_re = strdup(cp);
                        if ((cp = strchr(suite_re, '.')) != NULL) {
                                *cp++ = 0;
                                test_re = strdup(cp);
                        }
                }
                if (!suite_re)
                        suite_re = strdup("*");
                if (!test_re)
                        test_re = strdup("*");
                if (!family_re) {
                        fprintf(stderr,
                                "error: can't parse test family name: %s\n",
                                family_re);
                        return CUE_NOTEST;
                }
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
#ifdef HAVE_CU_SUITEINFO_PSETUPFUNC
                pSuite = CU_add_suite_with_setup_and_teardown(sp->pName,
                                        sp->pInitFunc, sp->pCleanupFunc,
                                        sp->pSetUpFunc, sp->pTearDownFunc);
#else
                                        pSuite = CU_add_suite(sp->pName,
                                                sp->pInitFunc, sp->pCleanupFunc);
#endif
                                }
                                CU_add_test(pSuite, tp->pName, tp->pTestFunc);
                        }
                }
        }

        /* all done -- clean up */
        free(family_re);
        free(suite_re);
        free(test_re);

        return CUE_SUCCESS;
}

static void parse_and_add_tests(char *testname_re);

static void parse_and_add_test(const char *test)
{
        if (test && access(test, F_OK) == 0) {
                FILE *fh;
                char t[256];

                if ((fh = fopen(test, "r")) == NULL) {
                        printf("Failed to open test-list file %s\n", test);
                        exit(10);
                }
                while (fgets(t, sizeof(t), fh) != NULL) {
                        while (1) {
                                int len = strlen(t);
                                if (len == 0) {
                                        break;
                                }
                                if (!isprint(t[--len])) {
                                        t[len] = 0;
                                        continue;
                                }
                                break;        
                        }
                        parse_and_add_tests(t);
                }
                fclose(fh);
                return;
        }

        if (add_tests(test) != CUE_SUCCESS) {
                fprintf(stderr, "error: suite registration failed: %s\n",
                    CU_get_error_msg());
                exit(1);
        }
}

static void parse_and_add_tests(char *testname_re)
{
        if (testname_re != NULL) {
                char *testname;
                while ((testname = strrchr(testname_re, ',')) != NULL) {
                        parse_and_add_test(testname + 1);
                        *testname = 0;
                }
        }
        parse_and_add_test(testname_re);
}

static int connect_scsi_device(struct scsi_device *sdev, const char *initiatorname)
{
        if (sdev->iscsi_url) {
                sdev->iscsi_ctx = iscsi_context_login(initiatorname, sdev->iscsi_url, &sdev->iscsi_lun);
                if (sdev->iscsi_ctx == NULL) {
                        return -1;
                }
                iscsi_set_no_ua_on_reconnect(sdev->iscsi_ctx, 1);
                return 0;
        }
#ifdef HAVE_SG_IO
        if (sdev->sgio_dev) {
                int version;

                if ((sdev->sgio_fd = open(sdev->sgio_dev, O_RDWR|O_NONBLOCK)) == -1) {
                        fprintf(stderr, "Failed to open SG_IO device %s. Error:%s\n", sdev->sgio_dev,
                                strerror(errno));
                        return -1;
                }
                if ((ioctl(sdev->sgio_fd, SG_GET_VERSION_NUM, &version) < 0) || (version < 30000)) {
                        fprintf(stderr, "%s is not a SCSI device node\n", sdev->sgio_dev);
                        close(sdev->sgio_fd);
                        return -1;
                }
                if (!strncmp(sdev->sgio_dev, "/dev/sg", 7)) {
                        /* We can not set this until we have the block-size */
                        printf("Can not use BLKSECTGET for /dev/sg devices\n");
                        return 0;
                }
                if (ioctl(sdev->sgio_fd, BLKSECTGET, &maxsectors) < 0) {
                        fprintf(stderr, "%s failed to read BLKSECTGET\n", sdev->sgio_dev);
                        close(sdev->sgio_fd);
                        return -1;
                }
                return 0;
        }
#endif
        return -1;
}

static void free_scsi_device(struct scsi_device *sdev)
{
        if (sdev->error_str) {
                free(sdev->error_str);
                sdev->error_str = NULL;
        }
        if (sdev->iscsi_url) {
                free(sdev->iscsi_url);
                sdev->iscsi_url = NULL;
        }
        if (sdev->iscsi_ctx) {
                iscsi_logout_sync(sdev->iscsi_ctx);
                iscsi_destroy_context(sdev->iscsi_ctx);
                sdev->iscsi_ctx = NULL;
        }

        if (sdev->sgio_dev) {
                free(sdev->sgio_dev);
                sdev->sgio_dev = NULL;
        }
        if (sdev->sgio_fd != -1) {
                close(sdev->sgio_fd);
                sdev->sgio_fd = -1;
        }
        free(sdev);
}

int
main(int argc, char *argv[])
{
        char *testname_re = NULL;
        CU_BasicRunMode mode = CU_BRM_VERBOSE;
        CU_ErrorAction error_action = CUEA_IGNORE;
        int res;
        struct scsi_readcapacity10 *rc10;
        struct scsi_task *inq_task = NULL;
        struct scsi_task *inq_lbp_task = NULL;
        struct scsi_task *inq_bdc_task = NULL;
        struct scsi_task *inq_bl_task = NULL;
        struct scsi_task *rc16_task = NULL;
        struct scsi_task *rsop_task = NULL;
        int full_size;
        int xml_mode = 0;
        static struct option long_opts[] = {
                { "help", no_argument, 0, '?' },
                { "list", no_argument, 0, 'l' },
                { "initiator-name", required_argument, 0, 'i' },
                { "initiator-name-2", required_argument, 0, 'I' },
                { "test", required_argument, 0, 't' },
                { "dataloss", no_argument, 0, 'd' },
                { "allow-sanitize", no_argument, 0, 'S' },
                { "ignore", no_argument, 0, 'g' },
                { "fail", no_argument, 0, 'f' },
                { "abort", no_argument, 0, 'A' },
                { "silent", no_argument, 0, 's' },
                { "normal", no_argument, 0, 'n' },
                { "verbose", no_argument, 0, 'v' },
                { "xml", no_argument, 0, 'x' },
                { "Verbose-scsi", no_argument, 0, 'V' },
                { NULL, 0, 0, 0 }
        };
        int i, c;
        int opt_idx = 0;
        unsigned int failures = 0;
        int ret;

        while ((c = getopt_long(argc, argv, "?hli:I:t:sdgfAsSnvxV", long_opts,
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
                case 'S':
                        allow_sanitize = 1;
                        break;
                case 'n':
                        mode = CU_BRM_NORMAL;
                        break;
                case 'v':
                        mode = CU_BRM_VERBOSE;        /* default */
                        break;
                case 'x':
                        xml_mode = 1;
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

        /* parse all trailing arguments as device paths */
        mp_num_sds = 0;
        while (optind < argc) {
                if (mp_num_sds >= MPATH_MAX_DEVS) {
                        fprintf(stderr, "Too many multipath device URLs\n");
                        print_usage();
                        free(testname_re);
                        return 10;
                }

                mp_sds[mp_num_sds] = malloc(sizeof(struct scsi_device));
                memset(mp_sds[mp_num_sds], '\0', sizeof(struct scsi_device));
                mp_sds[mp_num_sds]->sgio_fd = -1;

                if (!strncmp(argv[optind], "iscsi://", 8) ||
                    !strncmp(argv[optind], "iser://", 7)) {
                        mp_sds[mp_num_sds]->iscsi_url = strdup(argv[optind++]);
#ifdef HAVE_SG_IO
                } else {
                        mp_sds[mp_num_sds]->sgio_dev = strdup(argv[optind++]);
#endif
                }
                mp_num_sds++;
        }

        /* So that we can override iscsi_queue_pdu in tests
         * and replace or mutate the blob that we are about to write to the
         * wire.
         * This allows such tests to do their mutates and then call out
         * to the real queueing function once they have modified the data.
         */
        real_iscsi_queue_pdu = dlsym(RTLD_NEXT, "iscsi_queue_pdu");

        if ((mp_num_sds == 0) || (mp_sds[0]->iscsi_url == NULL
                                        && mp_sds[0]->sgio_dev == NULL)) {
#ifdef HAVE_SG_IO
                fprintf(stderr, "You must specify either an iSCSI URL or a device file\n");
#else
                fprintf(stderr, "You must specify an iSCSI URL\n");
#endif
                print_usage();
                if (testname_re)
                        free(testname_re);
                return 10;
        }

        /* sd remains an alias for the first device */
        sd = mp_sds[0];

        for (i = 0; i < mp_num_sds; i++) {
                res = connect_scsi_device(mp_sds[i], initiatorname1);
                if (res < 0) {
                        fprintf(stderr,
                                "Failed to connect to SCSI device %d\n", i);
                        goto err_sds_free;
                }
        }

	if (clear_pr(sd) < 0) {
		printf("One or more persistent reservations keys have been registered\n");
		goto err_sds_free;
	}

        if (mp_num_sds > 1) {
                /* check that all multipath sds identify as the same LU */
                res = mpath_check_matching_ids(mp_num_sds, mp_sds);
                if (res < 0) {
                        fprintf(stderr, "multipath devices don't match\n");
                        goto err_sds_free;
                }
        }

        /*
         * find the size of the LUN
         * All devices support readcapacity10 but only some support
         * readcapacity16
         */
        task = NULL;
        readcapacity10(sd, &task, 0, 0, EXPECT_STATUS_GOOD);
        if (task == NULL) {
                printf("Failed to send READCAPACITY10 command: %s\n", sd->error_str);
                goto err_sds_free;
        }
        if (task->status != SCSI_STATUS_GOOD) {
                printf("READCAPACITY10 command: failed with sense. %s\n", sd->error_str);
                scsi_free_scsi_task(task);
                goto err_sds_free;
        }
        rc10 = scsi_datain_unmarshall(task);
        if (rc10 == NULL) {
                printf("failed to unmarshall READCAPACITY10 data.\n");
                scsi_free_scsi_task(task);
                goto err_sds_free;
        }
        block_size = rc10->block_size;
        num_blocks = rc10->lba + 1;
        scsi_free_scsi_task(task);

        rc16_task = NULL;
        readcapacity16(sd, &rc16_task, 96, EXPECT_STATUS_GOOD);
        if (rc16_task == NULL) {
                printf("Failed to send READCAPACITY16 command: %s\n", sd->error_str);
                goto err_sds_free;
        }
        if (rc16_task->status == SCSI_STATUS_GOOD) {
                rc16 = scsi_datain_unmarshall(rc16_task);
                if (rc16 == NULL) {
                        printf("failed to unmarshall READCAPACITY16 data. %s\n", sd->error_str);
                        scsi_free_scsi_task(rc16_task);
                        goto err_sds_free;
                }
                block_size = rc16->block_length;
                num_blocks = rc16->returned_lba + 1;
                lbppb = 1 << rc16->lbppbe;
        }

        /* create a really big buffer we can use in the tests */
        scratch = malloc(65536 * block_size);
        
        inq_task = NULL;
        inquiry(sd, &inq_task, 0, 0, 64, EXPECT_STATUS_GOOD);
        if (inq_task == NULL || inq_task->status != SCSI_STATUS_GOOD) {
                printf("Inquiry command failed : %s\n", sd->error_str);
                goto err_sds_free;
        }
        full_size = scsi_datain_getfullsize(inq_task);
        if (full_size > inq_task->datain.size) {
                scsi_free_scsi_task(inq_task);

                /* we need more data for the full list */
                inq_task = NULL;
                inquiry(sd, &inq_task, 0, 0, full_size, EXPECT_STATUS_GOOD);
                if (inq_task == NULL) {
                        printf("Inquiry command failed : %s\n", sd->error_str);
                        goto err_sds_free;
                }
        }
        inq = scsi_datain_unmarshall(inq_task);
        if (inq == NULL) {
                printf("failed to unmarshall inquiry datain blob\n");
                scsi_free_scsi_task(inq_task);
                goto err_sds_free;
        }

        sbc3_support = 0;
        for (i = 0; i < 8; i++) {
                if (inq->version_descriptor[i] == 0x04C0) {
                        sbc3_support = 1;
                }
        }

        /* try reading block limits vpd */
        inq_bl_task = NULL;
        inquiry(sd, &inq_bl_task, 1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, 64, EXPECT_STATUS_GOOD);
        if (inq_bl_task && inq_bl_task->status != SCSI_STATUS_GOOD) {
                scsi_free_scsi_task(inq_bl_task);
                inq_bl_task = NULL;
        }
        if (inq_bl_task) {
                full_size = scsi_datain_getfullsize(inq_bl_task);
                if (full_size > inq_bl_task->datain.size) {
                        scsi_free_scsi_task(inq_bl_task);

                        inq_bl_task = NULL;
                        inquiry(sd, &inq_bl_task, 1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, full_size,
                                EXPECT_STATUS_GOOD);
                        if (inq_bl_task == NULL) {
                                printf("Inquiry command failed : %s\n", sd->error_str);
                                goto err_sds_free;
                        }
                }

                inq_bl = scsi_datain_unmarshall(inq_bl_task);
                if (inq_bl == NULL) {
                        printf("failed to unmarshall inquiry datain blob\n");
                        goto err_sds_free;
                }
        }

        /* try reading block device characteristics vpd */
        inquiry(sd, &inq_bdc_task, 1, SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS, 255,
                EXPECT_STATUS_GOOD);
        if (inq_bdc_task == NULL || inq_bdc_task->status != SCSI_STATUS_GOOD) {
                printf("Failed to read Block Device Characteristics page\n");
        } else {
                inq_bdc = scsi_datain_unmarshall(inq_bdc_task);
                if (inq_bdc == NULL) {
                        printf("failed to unmarshall inquiry datain blob\n");
                        goto err_sds_free;
                }
        }

        /* if thin provisioned we also need to read the VPD page for it */
        if (rc16 && rc16->lbpme != 0){
                inq_lbp_task = NULL;
                inquiry(sd, &inq_lbp_task, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING, 64,
                        EXPECT_STATUS_GOOD);
                if (inq_lbp_task == NULL || inq_lbp_task->status != SCSI_STATUS_GOOD) {
                        printf("Inquiry command failed : %s\n", sd->error_str);
                        goto err_sds_free;
                }
                full_size = scsi_datain_getfullsize(inq_lbp_task);
                if (full_size > inq_lbp_task->datain.size) {
                        scsi_free_scsi_task(inq_lbp_task);

                        /* we need more data for the full list */
                        inq_lbp_task = NULL;
                        inquiry(sd, &inq_lbp_task, 1, SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING,
                                full_size, EXPECT_STATUS_GOOD);
                        if (inq_lbp_task == NULL) {
                                printf("Inquiry command failed : %s\n", sd->error_str);
                                goto err_sds_free;
                        }
                }

                inq_lbp = scsi_datain_unmarshall(inq_lbp_task);
                if (inq_lbp == NULL) {
                        printf("failed to unmarshall inquiry datain blob\n");
                        goto err_sds_free;
                }
        }

        rsop_task = NULL;
        report_supported_opcodes(sd, &rsop_task, 1, SCSI_REPORT_SUPPORTING_OPS_ALL, 0, 0, 65535,
                                 EXPECT_STATUS_GOOD);
        if (rsop_task == NULL) {
                printf("Failed to send REPORT_SUPPORTED_OPCODES command: %s\n", sd->error_str);
                goto err_sds_free;
        }
        if (rsop_task->status == SCSI_STATUS_GOOD) {
                rsop = scsi_datain_unmarshall(rsop_task);
                if (rsop == NULL) {
                        printf("failed to unmarshall REPORT_SUPPORTED_OPCODES data.\n");
                        scsi_free_scsi_task(rsop_task);
                        rsop_task = NULL;
                }
        }

        /* check if the device is write protected or not */
        task = NULL;
        modesense6(sd, &task, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 255,
                   EXPECT_STATUS_GOOD);
        if (task == NULL) {
                printf("Failed to send MODE_SENSE6 command: %s\n", sd->error_str);
                goto err_sds_free;
        }
        if (task->status == SCSI_STATUS_GOOD) {
                struct scsi_mode_sense *ms;

                ms = scsi_datain_unmarshall(task);
                if (ms == NULL) {
                        printf("failed to unmarshall mode sense datain blob\n");
                        scsi_free_scsi_task(task);
                        goto err_sds_free;
                }
                readonly = !!(ms->device_specific_parameter & 0x80);
        }
        scsi_free_scsi_task(task);

        /* BLKSECTGET for /dev/sg* is a shitshow under linux.
         * Even 4.2 kernels return number of bytes instead of number
         * of sectors here. Just force it to 120k and let us get on with
         * our lives.
         */
        if (sd->sgio_dev && !strncmp(sd->sgio_dev, "/dev/sg", 7)) {
                printf("Looks like a /dev/sg device. Force max iosize "
                       "to 120k as BLKSECTGET is just broken and can "
                       "not be used for discovery.\n");
                maxsectors = 120 * 1024 / block_size;
        }
        if (maxsectors) {
                maximum_transfer_length = maxsectors;
                printf("Bus transfer size is limited to %d blocks. Clamping "
                       "max transfers accordingly.\n", maxsectors);
        }

        if (CU_initialize_registry() != 0) {
                fprintf(stderr, "error: unable to initialize test registry\n");
                goto err_sds_free;
        }
        if (CU_is_test_running()) {
                fprintf(stderr, "error: test suite(s) already running!?\n");
                exit(1);
        }

        parse_and_add_tests(testname_re);
        if (testname_re)
                free(testname_re);

        CU_basic_set_mode(mode);
        CU_set_error_action(error_action);
        printf("\n");

        /*
         * this actually runs the tests ...
         */

        if (xml_mode) {
          CU_list_tests_to_file();
          CU_automated_run_tests();
        } else {
          res = CU_basic_run_tests();
          printf("Tests completed with return value: %d\n", res);
        }

        failures = CU_get_number_of_failures();
        CU_cleanup_registry();

        if (inq_task != NULL) {
                scsi_free_scsi_task(inq_task);
        }
        if (inq_bl_task != NULL) {
                scsi_free_scsi_task(inq_bl_task);
        }
        if (inq_lbp_task != NULL) {
                scsi_free_scsi_task(inq_lbp_task);
        }
        if (inq_bdc_task != NULL) {
                scsi_free_scsi_task(inq_bdc_task);
        }
        if (rc16_task != NULL) {
                scsi_free_scsi_task(rc16_task);
        }
        if (rsop_task != NULL) {
                scsi_free_scsi_task(rsop_task);
        }
        for (i = 0; i < mp_num_sds; i++) {
                free_scsi_device(mp_sds[i]);
        }
        free(scratch);
        if (failures > 0) {
            ret = 1;
        } else {
            ret = 0;
        }
        return ret;

err_sds_free:
        for (i = 0; i < mp_num_sds; i++) {
                free_scsi_device(mp_sds[i]);
        }
        free(scratch);
        return -1;
}
