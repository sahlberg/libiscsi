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

#ifndef        _ISCSI_TEST_CU_H_
#define        _ISCSI_TEST_CU_H_

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "iscsi-support.h"

/* globals between setup, tests, and teardown */
extern struct scsi_task *task;
extern unsigned char *read_write_buf;

#ifndef HAVE_CU_SUITEINFO_PSETUPFUNC
/* libcunit version 1 */
typedef void (*CU_SetUpFunc)(void);
typedef void (*CU_TearDownFunc)(void);
#endif

int suite_init(void);
int suite_cleanup(void);
int suite_init_pgr(void);
int suite_cleanup_pgr(void);
void test_setup(void);
void test_teardown(void);

void test_compareandwrite_simple(void);
void test_compareandwrite_dpofua(void);
void test_compareandwrite_miscompare(void);
void test_compareandwrite_unwritten(void);
void test_compareandwrite_invalid_dataout_size(void);

void test_extendedcopy_simple(void);
void test_extendedcopy_param(void);
void test_extendedcopy_descr_limits(void);
void test_extendedcopy_descr_type(void);
void test_extendedcopy_validate_tgt_descr(void);
void test_extendedcopy_validate_seg_descr(void);

void test_get_lba_status_simple(void);
void test_get_lba_status_beyond_eol(void);
void test_get_lba_status_unmap_single(void);

void test_inquiry_alloc_length(void);
void test_inquiry_block_limits(void);
void test_inquiry_evpd(void);
void test_inquiry_mandatory_vpd_sbc(void);
void test_inquiry_standard(void);
void test_inquiry_supported_vpd(void);
void test_inquiry_version_descriptors(void);

void test_iscsi_cmdsn_toohigh(void);
void test_iscsi_cmdsn_toolow(void);

void test_iscsi_datasn_invalid(void);

void test_mandatory_sbc(void);

void test_modesense6_all_pages(void);
void test_modesense6_control(void);
void test_modesense6_control_d_sense(void);
void test_modesense6_control_swp(void);
void test_modesense6_residuals(void);

void test_nomedia_sbc(void);

void test_orwrite_simple(void);
void test_orwrite_beyond_eol(void);
void test_orwrite_0blocks(void);
void test_orwrite_wrprotect(void);
void test_orwrite_dpofua(void);
void test_orwrite_verify(void);

void test_prefetch10_simple(void);
void test_prefetch10_beyond_eol(void);
void test_prefetch10_0blocks(void);
void test_prefetch10_flags(void);

void test_prefetch16_simple(void);
void test_prefetch16_beyond_eol(void);
void test_prefetch16_0blocks(void);
void test_prefetch16_flags(void);

void test_preventallow_simple(void);
void test_preventallow_eject(void);
void test_preventallow_itnexus_loss(void);
void test_preventallow_logout(void);
void test_preventallow_warm_reset(void);
void test_preventallow_cold_reset(void);
void test_preventallow_lun_reset(void);
void test_preventallow_2_itnexuses(void);

void test_prin_read_keys_simple(void);
void test_prin_read_keys_truncate(void);
void test_prin_serviceaction_range(void);
void test_prin_report_caps_simple(void);

void test_prout_register_simple(void);
void test_prout_reserve_simple(void);
void test_prout_reserve_access_ea(void);
void test_prout_reserve_access_we(void);
void test_prout_reserve_access_earo(void);
void test_prout_reserve_access_wero(void);
void test_prout_reserve_access_eaar(void);
void test_prout_reserve_access_wear(void);
void test_prout_reserve_ownership_ea(void);
void test_prout_reserve_ownership_we(void);
void test_prout_reserve_ownership_earo(void);
void test_prout_reserve_ownership_wero(void);
void test_prout_reserve_ownership_eaar(void);
void test_prout_reserve_ownership_wear(void);
void test_prout_clear_simple(void);
void test_prout_preempt_rm_reg(void);

void test_read6_simple(void);
void test_read6_beyond_eol(void);
void test_read6_rdprotect(void);
void test_read6_flags(void);

void test_read10_simple(void);
void test_read10_beyond_eol(void);
void test_read10_0blocks(void);
void test_read10_rdprotect(void);
void test_read10_dpofua(void);
void test_read10_residuals(void);
void test_read10_invalid(void);
void test_async_read(void);

void test_read12_simple(void);
void test_read12_beyond_eol(void);
void test_read12_0blocks(void);
void test_read12_rdprotect(void);
void test_read12_dpofua(void);
void test_read12_residuals(void);

void test_read16_simple(void);
void test_read16_beyond_eol(void);
void test_read16_0blocks(void);
void test_read16_rdprotect(void);
void test_read16_dpofua(void);
void test_read16_residuals(void);

void test_readcapacity10_simple(void);

void test_readcapacity16_alloclen(void);
void test_readcapacity16_protection(void);
void test_readcapacity16_simple(void);
void test_readcapacity16_support(void);

void test_readdefectdata10_simple(void);

void test_readdefectdata12_simple(void);

void test_readonly_sbc(void);

void test_receive_copy_results_copy_status(void);
void test_receive_copy_results_op_params(void);

void test_report_supported_opcodes_one_command(void);
void test_report_supported_opcodes_rctd(void);
void test_report_supported_opcodes_servactv(void);
void test_report_supported_opcodes_simple(void);

void test_reserve6_simple(void);
void test_reserve6_2initiators(void);
void test_reserve6_logout(void);
void test_reserve6_itnexus_loss(void);
void test_reserve6_target_cold_reset(void);
void test_reserve6_target_warm_reset(void);
void test_reserve6_lun_reset(void);

void test_sanitize_block_erase(void);
void test_sanitize_block_erase_reserved(void);
void test_sanitize_crypto_erase(void);
void test_sanitize_crypto_erase_reserved(void);
void test_sanitize_exit_failure_mode(void);
void test_sanitize_invalid_serviceaction(void);
void test_sanitize_overwrite(void);
void test_sanitize_overwrite_reserved(void);
void test_sanitize_readonly(void);
void test_sanitize_reservations(void);
void test_sanitize_reset(void);

void test_startstopunit_simple(void);
void test_startstopunit_pwrcnd(void);
void test_startstopunit_noloej(void);

void test_testunitready_simple(void);

void test_unmap_simple(void);
void test_unmap_0blocks(void);
void test_unmap_vpd(void);

void test_verify10_simple(void);
void test_verify10_beyond_eol(void);
void test_verify10_0blocks(void);
void test_verify10_vrprotect(void);
void test_verify10_flags(void);
void test_verify10_dpo(void);
void test_verify10_mismatch(void);
void test_verify10_mismatch_no_cmp(void);

void test_verify12_simple(void);
void test_verify12_beyond_eol(void);
void test_verify12_0blocks(void);
void test_verify12_vrprotect(void);
void test_verify12_flags(void);
void test_verify12_dpo(void);
void test_verify12_mismatch(void);
void test_verify12_mismatch_no_cmp(void);

void test_verify16_simple(void);
void test_verify16_beyond_eol(void);
void test_verify16_0blocks(void);
void test_verify16_vrprotect(void);
void test_verify16_flags(void);
void test_verify16_dpo(void);
void test_verify16_mismatch(void);
void test_verify16_mismatch_no_cmp(void);

void test_write10_simple(void);
void test_write10_beyond_eol(void);
void test_write10_0blocks(void);
void test_write10_wrprotect(void);
void test_write10_dpofua(void);
void test_write10_residuals(void);
void test_async_write(void);

void test_write12_simple(void);
void test_write12_beyond_eol(void);
void test_write12_0blocks(void);
void test_write12_wrprotect(void);
void test_write12_dpofua(void);
void test_write12_residuals(void);

void test_write16_simple(void);
void test_write16_beyond_eol(void);
void test_write16_0blocks(void);
void test_write16_wrprotect(void);
void test_write16_dpofua(void);
void test_write16_residuals(void);

void test_writeatomic16_simple(void);
void test_writeatomic16_beyond_eol(void);
void test_writeatomic16_0blocks(void);
void test_writeatomic16_wrprotect(void);
void test_writeatomic16_dpofua(void);
void test_writeatomic16_vpd(void);

void test_writesame10_simple(void);
void test_writesame10_beyond_eol(void);
void test_writesame10_0blocks(void);
void test_writesame10_wrprotect(void);
void test_writesame10_unmap(void);
void test_writesame10_unmap_unaligned(void);
void test_writesame10_unmap_until_end(void);
void test_writesame10_unmap_vpd(void);
void test_writesame10_check(void);
void test_writesame10_invalid_dataout_size(void);

void test_writesame16_simple(void);
void test_writesame16_beyond_eol(void);
void test_writesame16_0blocks(void);
void test_writesame16_wrprotect(void);
void test_writesame16_unmap(void);
void test_writesame16_unmap_unaligned(void);
void test_writesame16_unmap_until_end(void);
void test_writesame16_unmap_vpd(void);
void test_writesame16_check(void);
void test_writesame16_invalid_dataout_size(void);

void test_writeverify10_simple(void);
void test_writeverify10_beyond_eol(void);
void test_writeverify10_0blocks(void);
void test_writeverify10_wrprotect(void);
void test_writeverify10_flags(void);
void test_writeverify10_dpo(void);
void test_writeverify10_residuals(void);

void test_writeverify12_simple(void);
void test_writeverify12_beyond_eol(void);
void test_writeverify12_0blocks(void);
void test_writeverify12_wrprotect(void);
void test_writeverify12_flags(void);
void test_writeverify12_dpo(void);
void test_writeverify12_residuals(void);

void test_writeverify16_simple(void);
void test_writeverify16_beyond_eol(void);
void test_writeverify16_0blocks(void);
void test_writeverify16_wrprotect(void);
void test_writeverify16_flags(void);
void test_writeverify16_dpo(void);
void test_writeverify16_residuals(void);

void test_multipathio_simple(void);
void test_multipathio_reset(void);
void test_multipathio_compareandwrite(void);
void test_mpio_async_caw(void);

void test_async_abort_simple(void);
void test_async_lu_reset_simple(void);

#endif        /* _ISCSI_TEST_CU_H_ */
