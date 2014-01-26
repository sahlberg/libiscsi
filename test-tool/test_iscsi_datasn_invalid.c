/* 
   Copyright (C) 2014 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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

#include <stdio.h>
#include <arpa/inet.h>
#include <CUnit/CUnit.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include "iscsi-test-cu.h"

static int change_datasn;

static int my_iscsi_queue_pdu(struct iscsi_context *iscsi _U_, struct iscsi_pdu *pdu _U_)
{
	uint32_t datasn;

	if (pdu->outdata.data[0] != ISCSI_PDU_DATA_OUT) {
		return 0;
	}
	switch (change_datasn) {
	case 1:
		/* change datasn to 0 */
		scsi_set_uint32(&pdu->outdata.data[36], 0);
		break;
	case 2:
		/* change datasn to 27 */
		scsi_set_uint32(&pdu->outdata.data[36], 27);
		break;
	case 3:
		/* change datasn to -1 */
		scsi_set_uint32(&pdu->outdata.data[36], -1);
		break;
	case 4:
		/* change datasn from (0,1) to (1,0) */
		datasn = scsi_get_uint32(&pdu->outdata.data[36]);
		scsi_set_uint32(&pdu->outdata.data[36], 1 - datasn);
		break;
	}
	return 0;
}

void test_iscsi_datasn_invalid(void)
{ 
	int ret;
	unsigned char *buf = alloca(2 * block_size);

	CHECK_FOR_DATALOSS;

	logging(LOG_VERBOSE, LOG_BLANK_LINE);
	logging(LOG_VERBOSE, "Test sending invalid iSCSI DATASN");


	logging(LOG_VERBOSE, "Send 2 DATAIN with DATASN==0. Should fail.");
	change_datasn = 1;

	iscsic->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
	iscsic->target_max_recv_data_segment_length = block_size;
	local_iscsi_queue_pdu = my_iscsi_queue_pdu;
	iscsi_set_noautoreconnect(iscsic, 1);
	iscsi_set_timeout(iscsic, 3);

	ret = write10(iscsic, tgt_lun, 100, 2 * block_size,
		      block_size, 0, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		CU_PASS("WRITE10 is not implemented.");
		return;
	}	
	CU_ASSERT_NOT_EQUAL(ret, 0);

	iscsi_set_noautoreconnect(iscsic, 0);


	logging(LOG_VERBOSE, "Send DATAIN with DATASN==27. Should fail");
	change_datasn = 2;

	iscsic->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
	iscsic->target_max_recv_data_segment_length = block_size;
	local_iscsi_queue_pdu = my_iscsi_queue_pdu;
	iscsi_set_noautoreconnect(iscsic, 1);
	iscsi_set_timeout(iscsic, 3);

	ret = write10(iscsic, tgt_lun, 100, block_size,
		      block_size, 0, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		CU_PASS("WRITE10 is not implemented.");
		return;
	}	
	CU_ASSERT_NOT_EQUAL(ret, 0);

	iscsi_set_noautoreconnect(iscsic, 0);


	logging(LOG_VERBOSE, "Send DATAIN with DATASN==-1. Should fail");
	change_datasn = 3;

	iscsic->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
	iscsic->target_max_recv_data_segment_length = block_size;
	local_iscsi_queue_pdu = my_iscsi_queue_pdu;
	iscsi_set_noautoreconnect(iscsic, 1);
	iscsi_set_timeout(iscsic, 3);

	ret = write10(iscsic, tgt_lun, 100, block_size,
		      block_size, 0, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		CU_PASS("WRITE10 is not implemented.");
		return;
	}	
	CU_ASSERT_NOT_EQUAL(ret, 0);

	iscsi_set_noautoreconnect(iscsic, 0);



	logging(LOG_VERBOSE, "Send DATAIN in reverse order (datasn == 1,0). Should fail");
	change_datasn = 4;

	iscsic->use_immediate_data = ISCSI_IMMEDIATE_DATA_NO;
	iscsic->target_max_recv_data_segment_length = block_size;
	local_iscsi_queue_pdu = my_iscsi_queue_pdu;
	iscsi_set_noautoreconnect(iscsic, 1);
	iscsi_set_timeout(iscsic, 3);

	ret = write10(iscsic, tgt_lun, 100, 2 * block_size,
		      block_size, 0, 0, 0, 0, 0, buf);
	if (ret == -2) {
		logging(LOG_NORMAL, "[SKIPPED] WRITE10 is not implemented.");
		CU_PASS("WRITE10 is not implemented.");
		return;
	}	
	CU_ASSERT_NOT_EQUAL(ret, 0);

	iscsi_set_noautoreconnect(iscsic, 0);
}
