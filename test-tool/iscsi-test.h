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
