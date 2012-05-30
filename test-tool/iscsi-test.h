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
extern int data_loss;


int T0100_read10_simple(const char *initiator, const char *url);
int T0101_read10_beyond_eol(const char *initiator, const char *url);
int T0102_read10_0blocks(const char *initiator, const char *url);
int T0103_read10_rdprotect(const char *initiator, const char *url);
int T0104_read10_flags(const char *initiator, const char *url);
int T0105_read10_invalid(const char *initiator, const char *url);

int T0110_readcapacity10_simple(const char *initiator, const char *url);
int T0111_readcapacity10_pmi(const char *initiator, const char *url);

int T0120_read6_simple(const char *initiator, const char *url);
int T0121_read6_beyond_eol(const char *initiator, const char *url);
int T0122_read6_invalid(const char *initiator, const char *url);

int T0130_verify10_simple(const char *initiator, const char *url);
int T0131_verify10_mismatch(const char *initiator, const char *url);
int T0132_verify10_mismatch_no_cmp(const char *initiator, const char *url);

int T0160_readcapacity16_simple(const char *initiator, const char *url);

int T0170_unmap_simple(const char *initiator, const char *url);
int T0171_unmap_zero(const char *initiator, const char *url);

int T0180_writesame10_unmap(const char *initiator, const char *url);
int T0181_writesame10_unmap_unaligned(const char *initiator, const char *url);

int T0190_writesame16_unmap(const char *initiator, const char *url);
int T0191_writesame16_unmap_unaligned(const char *initiator, const char *url);

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

int T0250_prefetch16_simple(const char *initiator, const char *url);

int T0260_get_lba_status_simple(const char *initiator, const char *url);
