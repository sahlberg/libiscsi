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

struct iscsi_context *iscsi_context_login(const char *initiatorname, const char *url, int *lun);

struct iscsi_async_state {
	struct scsi_task *task;
	int status;
	int finished;
};
void wait_until_test_finished(struct iscsi_context *iscsi, struct iscsi_async_state *test_state);

struct iscsi_pdu;
void (*local_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

int T0100_read10_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0101_read10_beyond_eol(const char *initiator, const char *url, int data_loss, int show_info);
int T0102_read10_0blocks(const char *initiator, const char *url, int data_loss, int show_info);
int T0103_read10_rdprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0104_read10_flags(const char *initiator, const char *url, int data_loss, int show_info);
int T0105_read10_invalid(const char *initiator, const char *url, int data_loss, int show_info);

int T0110_readcapacity10_simple(const char *initiator, const char *url, int data_loss, int show_info);

int T0120_read6_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0121_read6_beyond_eol(const char *initiator, const char *url, int data_loss, int show_info);
int T0122_read6_invalid(const char *initiator, const char *url, int data_loss, int show_info);

int T0130_verify10_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0131_verify10_mismatch(const char *initiator, const char *url, int data_loss, int show_info);
int T0132_verify10_mismatch_no_cmp(const char *initiator, const char *url, int data_loss, int show_info);
int T0133_verify10_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0160_readcapacity16_simple(const char *initiator, const char *url, int data_loss, int show_info);

int T0170_unmap_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0171_unmap_zero(const char *initiator, const char *url, int data_loss, int show_info);

int T0180_writesame10_unmap(const char *initiator, const char *url, int data_loss, int show_info);
int T0181_writesame10_unmap_unaligned(const char *initiator, const char *url, int data_loss, int show_info);
int T0182_writesame10_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);
int T0183_writesame10_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0184_writesame10_0blocks(const char *initiator, const char *url, int data_loss, int show_info);

int T0190_writesame16_unmap(const char *initiator, const char *url, int data_loss, int show_info);
int T0191_writesame16_unmap_unaligned(const char *initiator, const char *url, int data_loss, int show_info);
int T0192_writesame16_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);
int T0193_writesame16_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0194_writesame16_0blocks(const char *initiator, const char *url, int data_loss, int show_info);

int T0200_read16_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0201_read16_rdprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0202_read16_flags(const char *initiator, const char *url, int data_loss, int show_info);
int T0203_read16_0blocks(const char *initiator, const char *url, int data_loss, int show_info);
int T0204_read16_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0210_read12_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0211_read12_rdprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0212_read12_flags(const char *initiator, const char *url, int data_loss, int show_info);
int T0213_read12_0blocks(const char *initiator, const char *url, int data_loss, int show_info);
int T0214_read12_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0220_write16_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0221_write16_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0222_write16_flags(const char *initiator, const char *url, int data_loss, int show_info);
int T0223_write16_0blocks(const char *initiator, const char *url, int data_loss, int show_info);
int T0224_write16_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0230_write12_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0231_write12_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0232_write12_flags(const char *initiator, const char *url, int data_loss, int show_info);
int T0233_write12_0blocks(const char *initiator, const char *url, int data_loss, int show_info);
int T0234_write12_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0240_prefetch10_simple(const char *initiator, const char *url, int data_loss, int show_info);

int T0250_prefetch16_simple(const char *initiator, const char *url, int data_loss, int show_info);

int T0260_get_lba_status_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0264_get_lba_status_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0270_verify16_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0271_verify16_mismatch(const char *initiator, const char *url, int data_loss, int show_info);
int T0272_verify16_mismatch_no_cmp(const char *initiator, const char *url, int data_loss, int show_info);
int T0273_verify16_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0280_verify12_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0281_verify12_mismatch(const char *initiator, const char *url, int data_loss, int show_info);
int T0282_verify12_mismatch_no_cmp(const char *initiator, const char *url, int data_loss, int show_info);
int T0283_verify12_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0290_write10_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0291_write10_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0292_write10_flags(const char *initiator, const char *url, int data_loss, int show_info);
int T0293_write10_0blocks(const char *initiator, const char *url, int data_loss, int show_info);
int T0294_write10_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0300_readonly(const char *initiator, const char *url, int data_loss, int show_info);

int T0310_writeverify10_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0311_writeverify10_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0314_writeverify10_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0320_writeverify12_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0321_writeverify12_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0324_writeverify12_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0330_writeverify16_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0331_writeverify16_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0334_writeverify16_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0340_compareandwrite_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0341_compareandwrite_mismatch(const char *initiator, const char *url, int data_loss, int show_info);
int T0343_compareandwrite_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0350_orwrite_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0351_orwrite_wrprotect(const char *initiator, const char *url, int data_loss, int show_info);
int T0354_orwrite_beyondeol(const char *initiator, const char *url, int data_loss, int show_info);

int T0360_startstopunit_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0361_startstopunit_pwrcnd(const char *initiator, const char *url, int data_loss, int show_info);

int T0370_nomedia(const char *initiator, const char *url, int data_loss, int show_info);

int T0380_preventallow_simple(const char *initiator, const char *url, int data_loss, int show_info);
int T0381_preventallow_eject(const char *initiator, const char *url, int data_loss, int show_info);
int T0382_preventallow_itnexus_loss(const char *initiator, const char *url, int data_loss, int show_info);
int T0383_preventallow_target_warm_reset(const char *initiator, const char *url, int data_loss, int show_info);
int T0384_preventallow_target_cold_reset(const char *initiator, const char *url, int data_loss, int show_info);
int T0385_preventallow_lun_reset(const char *initiator, const char *url, int data_loss, int show_info);
int T0386_preventallow_2_itl_nexuses(const char *initiator, const char *url, int data_loss, int show_info);

int T0390_mandatory_opcodes_sbc(const char *initiator, const char *url, int data_loss, int show_info);

int T1000_cmdsn_invalid(const char *initiator, const char *url, int data_loss, int show_info);
int T1010_datasn_invalid(const char *initiator, const char *url, int data_loss, int show_info);
int T1020_bufferoffset_invalid(const char *initiator, const char *url, int data_loss, int show_info);
