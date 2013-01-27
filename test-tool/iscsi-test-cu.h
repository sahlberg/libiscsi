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

#ifndef	_ISCSI_TEST_CU_H_
#define	_ISCSI_TEST_CU_H_

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "iscsi-support.h"

/* globals between setup, tests, and teardown */
extern struct iscsi_context *iscsic;
extern int tgt_lun;
extern struct scsi_task *task;

int test_setup(void);
int test_teardown(void);

void test_get_lba_status_simple(void);

void test_prefetch10_simple(void);
void test_prefetch10_beyond_eol(void);
void test_prefetch10_0blocks(void);
void test_prefetch10_flags(void);

void test_prefetch16_simple(void);
void test_prefetch16_beyond_eol(void);
void test_prefetch16_0blocks(void);
void test_prefetch16_flags(void);

void test_read6_simple(void);
void test_read6_beyond_eol(void);
void test_read6_0blocks(void);
void test_read6_rdprotect(void);
void test_read6_flags(void);

void test_read10_simple(void);
void test_read10_beyond_eol(void);
void test_read10_0blocks(void);
void test_read10_rdprotect(void);
void test_read10_flags(void);
void test_read10_invalid(void);

void test_read12_simple(void);
void test_read12_beyond_eol(void);
void test_read12_0blocks(void);
void test_read12_rdprotect(void);
void test_read12_flags(void);

void test_read16_simple(void);
void test_read16_beyond_eol(void);
void test_read16_0blocks(void);
void test_read16_rdprotect(void);
void test_read16_flags(void);

void test_readcapacity10_simple(void);

void test_readcapacity16_simple(void);
void test_readcapacity16_alloclen(void);

void test_testunitready_simple(void);

void test_unmap_simple(void);
void test_unmap_0blocks(void);

void test_verify10_simple(void);
void test_verify10_beyond_eol(void);
void test_verify10_0blocks(void);
void test_verify10_vrprotect(void);
void test_verify10_flags(void);
void test_verify10_mismatch(void);
void test_verify10_mismatch_no_cmp(void);

void test_verify12_simple(void);
void test_verify12_beyond_eol(void);
void test_verify12_0blocks(void);
void test_verify12_vrprotect(void);
void test_verify12_flags(void);
void test_verify12_mismatch(void);
void test_verify12_mismatch_no_cmp(void);

void test_verify16_simple(void);
void test_verify16_beyond_eol(void);
void test_verify16_0blocks(void);
void test_verify16_vrprotect(void);
void test_verify16_flags(void);
void test_verify16_mismatch(void);
void test_verify16_mismatch_no_cmp(void);

void test_write10_simple(void);
void test_write10_beyond_eol(void);
void test_write10_0blocks(void);
void test_write10_wrprotect(void);
void test_write10_flags(void);

void test_write12_simple(void);
void test_write12_beyond_eol(void);
void test_write12_0blocks(void);
void test_write12_wrprotect(void);
void test_write12_flags(void);

void test_write16_simple(void);
void test_write16_beyond_eol(void);
void test_write16_0blocks(void);
void test_write16_wrprotect(void);
void test_write16_flags(void);

void test_writesame10_simple(void);
void test_writesame10_beyond_eol(void);
void test_writesame10_0blocks(void);
void test_writesame10_wrprotect(void);
void test_writesame10_unmap(void);
void test_writesame10_unmap_unaligned(void);
void test_writesame10_unmap_until_end(void);

void test_writesame16_simple(void);
void test_writesame16_beyond_eol(void);
void test_writesame16_0blocks(void);
void test_writesame16_wrprotect(void);
void test_writesame16_unmap(void);
void test_writesame16_unmap_unaligned(void);
void test_writesame16_unmap_until_end(void);

#endif	/* _ISCSI_TEST_CU_H_ */
