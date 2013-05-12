/*
   iscsi-test tool

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

#ifndef	_ISCSI_TEST_H_
#define	_ISCSI_TEST_H_

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "iscsi-support.h"

extern int show_info;

int T0000_testunitready_simple(const char *initiator, const char *url);

int T0100_read10_simple(const char *initiator, const char *url);
int T0101_read10_beyond_eol(const char *initiator, const char *url);
int T0102_read10_0blocks(const char *initiator, const char *url);
int T0103_read10_rdprotect(const char *initiator, const char *url);
int T0104_read10_flags(const char *initiator, const char *url);
int T0105_read10_invalid(const char *initiator, const char *url);

int T0110_readcapacity10_simple(const char *initiator, const char *url);

int T0120_read6_simple(const char *initiator, const char *url);
int T0121_read6_beyond_eol(const char *initiator, const char *url);
int T0122_read6_invalid(const char *initiator, const char *url);

int T0130_verify10_simple(const char *initiator, const char *url);
int T0131_verify10_mismatch(const char *initiator, const char *url);
int T0132_verify10_mismatch_no_cmp(const char *initiator, const char *url);
int T0133_verify10_beyondeol(const char *initiator, const char *url);

int T0160_readcapacity16_simple(const char *initiator, const char *url);
int T0161_readcapacity16_alloclen(const char *initiator, const char *url);

int T0170_unmap_simple(const char *initiator, const char *url);
int T0171_unmap_zero(const char *initiator, const char *url);

int T0180_writesame10_unmap(const char *initiator, const char *url);
int T0181_writesame10_unmap_unaligned(const char *initiator, const char *url);
int T0182_writesame10_beyondeol(const char *initiator, const char *url);
int T0183_writesame10_wrprotect(const char *initiator, const char *url);
int T0184_writesame10_0blocks(const char *initiator, const char *url);

int T0190_writesame16_unmap(const char *initiator, const char *url);
int T0191_writesame16_unmap_unaligned(const char *initiator, const char *url);
int T0192_writesame16_beyondeol(const char *initiator, const char *url);
int T0193_writesame16_wrprotect(const char *initiator, const char *url);
int T0194_writesame16_0blocks(const char *initiator, const char *url);

int T0200_read16_simple(const char *initiator, const char *url);
int T0201_read16_rdprotect(const char *initiator, const char *url);
int T0202_read16_flags(const char *initiator, const char *url);
int T0203_read16_0blocks(const char *initiator, const char *url);
int T0204_read16_beyondeol(const char *initiator, const char *url);

int T0210_read12_simple(const char *initiator, const char *url);
int T0211_read12_rdprotect(const char *initiator, const char *url);
int T0212_read12_flags(const char *initiator, const char *url);
int T0213_read12_0blocks(const char *initiator, const char *url);
int T0214_read12_beyondeol(const char *initiator, const char *url);

int T0220_write16_simple(const char *initiator, const char *url);
int T0221_write16_wrprotect(const char *initiator, const char *url);
int T0222_write16_flags(const char *initiator, const char *url);
int T0223_write16_0blocks(const char *initiator, const char *url);
int T0224_write16_beyondeol(const char *initiator, const char *url);

int T0230_write12_simple(const char *initiator, const char *url);
int T0231_write12_wrprotect(const char *initiator, const char *url);
int T0232_write12_flags(const char *initiator, const char *url);
int T0233_write12_0blocks(const char *initiator, const char *url);
int T0234_write12_beyondeol(const char *initiator, const char *url);

int T0240_prefetch10_simple(const char *initiator, const char *url);
int T0241_prefetch10_flags(const char *initiator, const char *url);
int T0242_prefetch10_beyondeol(const char *initiator, const char *url);
int T0243_prefetch10_0blocks(const char *initiator, const char *url);

int T0250_prefetch16_simple(const char *initiator, const char *url);
int T0251_prefetch16_flags(const char *initiator, const char *url);
int T0252_prefetch16_beyondeol(const char *initiator, const char *url);
int T0253_prefetch16_0blocks(const char *initiator, const char *url);

int T0260_get_lba_status_simple(const char *initiator, const char *url);
int T0264_get_lba_status_beyondeol(const char *initiator, const char *url);

int T0270_verify16_simple(const char *initiator, const char *url);
int T0271_verify16_mismatch(const char *initiator, const char *url);
int T0272_verify16_mismatch_no_cmp(const char *initiator, const char *url);
int T0273_verify16_beyondeol(const char *initiator, const char *url);

int T0280_verify12_simple(const char *initiator, const char *url);
int T0281_verify12_mismatch(const char *initiator, const char *url);
int T0282_verify12_mismatch_no_cmp(const char *initiator, const char *url);
int T0283_verify12_beyondeol(const char *initiator, const char *url);

int T0290_write10_simple(const char *initiator, const char *url);
int T0291_write10_wrprotect(const char *initiator, const char *url);
int T0292_write10_flags(const char *initiator, const char *url);
int T0293_write10_0blocks(const char *initiator, const char *url);
int T0294_write10_beyondeol(const char *initiator, const char *url);

int T0300_readonly(const char *initiator, const char *url);

int T0310_writeverify10_simple(const char *initiator, const char *url);
int T0311_writeverify10_wrprotect(const char *initiator, const char *url);
int T0314_writeverify10_beyondeol(const char *initiator, const char *url);

int T0320_writeverify12_simple(const char *initiator, const char *url);
int T0321_writeverify12_wrprotect(const char *initiator, const char *url);
int T0324_writeverify12_beyondeol(const char *initiator, const char *url);

int T0330_writeverify16_simple(const char *initiator, const char *url);
int T0331_writeverify16_wrprotect(const char *initiator, const char *url);
int T0334_writeverify16_beyondeol(const char *initiator, const char *url);

int T0340_compareandwrite_simple(const char *initiator, const char *url);
int T0341_compareandwrite_mismatch(const char *initiator, const char *url);
int T0343_compareandwrite_beyondeol(const char *initiator, const char *url);

int T0350_orwrite_simple(const char *initiator, const char *url);
int T0351_orwrite_wrprotect(const char *initiator, const char *url);
int T0354_orwrite_beyondeol(const char *initiator, const char *url);

int T0360_startstopunit_simple(const char *initiator, const char *url);
int T0361_startstopunit_pwrcnd(const char *initiator, const char *url);
int T0362_startstopunit_noloej(const char *initiator, const char *url);

int T0370_nomedia(const char *initiator, const char *url);

int T0380_preventallow_simple(const char *initiator, const char *url);
int T0381_preventallow_eject(const char *initiator, const char *url);
int T0382_preventallow_itnexus_loss(const char *initiator, const char *url);
int T0383_preventallow_target_warm_reset(const char *initiator, const char *url);
int T0384_preventallow_target_cold_reset(const char *initiator, const char *url);
int T0385_preventallow_lun_reset(const char *initiator, const char *url);
int T0386_preventallow_2_itl_nexuses(const char *initiator, const char *url);

int T0390_mandatory_opcodes_sbc(const char *initiator, const char *url);

int T0400_inquiry_basic(const char *initiator, const char *url);
int T0401_inquiry_alloclen(const char *initiator, const char *url);
int T0402_inquiry_evpd(const char *initiator, const char *url);
int T0403_inquiry_supported_vpd(const char *initiator, const char *url);
int T0404_inquiry_all_reported_vpd(const char *initiator, const char *url);

int T0410_readtoc_basic(const char *initiator, const char *url);

int T0420_reserve6_simple(const char *initiator, const char *url);
int T0421_reserve6_lun_reset(const char *initiator, const char *url);
int T0422_reserve6_logout(const char *initiator, const char *url);
int T0423_reserve6_sessionloss(const char *initiator, const char *url);
int T0424_reserve6_target_reset(const char *initiator, const char *url);

int T0430_report_all_supported_ops(const char *initiator, const char *url);

int T1000_cmdsn_invalid(const char *initiator, const char *url);
int T1010_datasn_invalid(const char *initiator, const char *url);
int T1020_bufferoffset_invalid(const char *initiator, const char *url);
int T1030_unsolicited_data_overflow(const char *initiator, const char *url);
int T1031_unsolicited_data_out(const char *initiator, const char *url);
int T1040_saturate_maxcmdsn(const char *initiator, const char *url);
int T1041_unsolicited_immediate_data(const char *initiator, const char *url);
int T1042_unsolicited_nonimmediate_data(const char *initiator, const char *url);
int T1100_persistent_reserve_in_read_keys_simple(const char *initiator, const char *url);
int T1110_persistent_reserve_in_serviceaction_range(const char *initiator, const char *url);
int T1120_persistent_register_simple(const char *initiator, const char *url);
int T1130_persistent_reserve_simple(const char *initiator, const char *url);
int T1140_persistent_reserve_access_check_ea(const char *initiator, const char *url);
int T1141_persistent_reserve_access_check_we(const char *initiator, const char *url);
int T1142_persistent_reserve_access_check_earo(const char *initiator, const char *url);
int T1143_persistent_reserve_access_check_wero(const char *initiator, const char *url);
int T1144_persistent_reserve_access_check_eaar(const char *initiator, const char *url);
int T1145_persistent_reserve_access_check_wear(const char *initiator, const char *url);

#endif	/* _ISCSI_TEST_H_ */
