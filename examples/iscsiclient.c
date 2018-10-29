/* 
   Copyright (C) 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>
   
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



/* This is an example of using libiscsi.
 * It basically logs in to the the target and performs a discovery.
 * It then selects the last target in the returned list and
 * starts a normal login to that target.
 * Once logged in it issues a REPORTLUNS call and selects the last returned lun in the list.
 * This LUN is then used to send INQUIRY, READCAPACITY10 and READ10 test calls to.
 */
/* The reason why we have to specify an allocation length and sometimes probe, starting with a small value, probing how big the buffer 
 * should be, and asking again with a bigger buffer.
 * Why not just always ask with a buffer that is big enough?
 * The reason is that a lot of scsi targets are "sensitive" and ""buggy""
 * many targets will just fail the operation completely if they thing alloc len is unreasonably big.
 */

/* This is the host/port we connect to.*/
#define TARGET "127.0.0.1:3260"

#if defined(_WIN32)
#include <winsock2.h>
#include "win32/win32_compat.h"
#pragma comment(lib, "ws2_32.lib")
WSADATA wsaData;
#else
#include <poll.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

struct client_state {
       int finished;
       const char *message;
       int has_discovered_target;
       char *target_name;
       char *target_address;
       int lun;
       int block_size;
};

unsigned char small_buffer[512];

void tm_at_cb(struct iscsi_context *iscsi _U_, int status _U_, void *command_data _U_, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;

	printf("tm at cb !\n");
	printf("response : %d\n", *((uint32_t *)command_data));

	clnt->finished = 1;
}


void synccache10_cb(struct iscsi_context *iscsi _U_, int status, void *command_data _U_, void *private_data _U_)
{
	printf("SYNCCACHE10 status:%d\n", status);
}

void nop_out_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct iscsi_data *data = command_data;
	struct scsi_task *task;

	printf("NOP-IN status:%d\n", status);
	if (data->size > 0) {
		printf("NOP-IN (%zu) data:%.*s\n",
		       data->size, (int)data->size, data->data);
	}
	printf("Send SYNCHRONIZECACHE10\n");
	task = iscsi_synchronizecache10_task(iscsi, 2, 0, 0, 0, 0, synccache10_cb, private_data);
	if (task == NULL) {
		printf("failed to send sync cache10\n");
		exit(10);
	}
	printf("send task management to try to abort the sync10 task\n");
	if (iscsi_task_mgmt_abort_task_async(iscsi, task, tm_at_cb, private_data) != 0) {
		printf("failed to send task management to abort the sync10 task\n");
		exit(10);
	}
}


void write10_1_cb(struct iscsi_context *iscsi _U_, int status, void *command_data, void *private_data _U_)
{
	struct scsi_task *task = command_data;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Write10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}
	if (status != SCSI_STATUS_GOOD) {
		printf("Write10 failed with %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("Write successful :%d\n", status);
	scsi_free_scsi_task(task);
	exit(10);
}

void write10_cb(struct iscsi_context *iscsi _U_, int status, void *command_data, void *private_data _U_)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int i;
	static unsigned char wb[512];
	static struct scsi_iovec iov[3];

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Write10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}
	if (status != SCSI_STATUS_GOOD) {
		printf("Write10 failed with %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("Write successful :%d\n", status);
	scsi_free_scsi_task(task);

	printf("write the block using an iovector\n");
	for (i = 0;i < 512; i++) {
		wb[i] = (511 - i) & 0xff;
	}
	task = iscsi_write10_task(iscsi, clnt->lun, 0, NULL, 512, 512,
			0, 0, 0, 0, 0,
			write10_1_cb, private_data);
	if (task == NULL) {
		printf("failed to send write10 command\n");
		exit(10);
	}
	/* provide iovectors where to read the data.
	 */
	iov[0].iov_base = &wb[0];
	iov[0].iov_len  = 4;
	iov[1].iov_base = &wb[4];
	iov[1].iov_len  = 11;
	iov[2].iov_base = &wb[15];
	iov[2].iov_len  = 512 - 15;
	scsi_task_set_iov_out(task, &iov[0], 3);
}

void read10_1_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int i;
	static unsigned char wb[512];

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Read10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("READ10 using scsi_task_set_iov_in() successful. Block content:\n");
	for (i=0;i<512;i++) {
		printf("%02x ", small_buffer[i]);
		if (i%16==15)
			printf("\n");
		if (i==69)
			break;
	}
	printf("...\n");
	scsi_free_scsi_task(task);

#if 0
	printf("Finished,   wont try to write data since that will likely destroy your LUN :-(\n");
	printf("Send NOP-OUT\n");
	if (iscsi_nop_out_async(iscsi, nop_out_cb, (unsigned char *)"Ping!", 6, private_data) != 0) {
		printf("failed to send nop-out\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
#else
	printf("write the block normally\n");
	for (i = 0;i < 512; i++) {
		wb[i] = i & 0xff;
	}
	task = iscsi_write10_task(iscsi, clnt->lun, 0, wb, 512, 512,
			0, 0, 0, 0, 0,
			write10_cb, private_data);
	if (task == NULL) {
		printf("failed to send write10 command\n");
		exit(10);
	}
#endif
}

void read10_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int i;
	static struct scsi_iovec iov[3];

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Read10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("READ10 using scsi_task_add_data_in_buffer() successful. Block content:\n");
	for (i=0;i<512;i++) {
		printf("%02x ", small_buffer[i]);
		if (i%16==15)
			printf("\n");
		if (i==69)
			break;
	}
	printf("...\n");
	scsi_free_scsi_task(task);

	memset(&small_buffer[0], 0, 512);

	if ((task = iscsi_read10_task(iscsi, clnt->lun, 0, clnt->block_size, clnt->block_size, 0, 0, 0, 0, 0, read10_1_cb, private_data)) == NULL) {
		printf("failed to send read10 command\n");
		exit(10);
	}
	/* provide iovectors where to read the data.
	 */
	iov[0].iov_base = &small_buffer[0];
	iov[0].iov_len  = 7;
	iov[1].iov_base = &small_buffer[7];
	iov[1].iov_len  = 8;
	iov[2].iov_base = &small_buffer[15];
	iov[2].iov_len  = 512 - 15;
	scsi_task_set_iov_in(task, &iov[0], 3);
}

void read6_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	int i;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Read6 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("READ6 successful. Block content:\n");
	for (i=0;i<task->datain.size;i++) {
		printf("%02x ", task->datain.data[i]);
		if (i%16==15)
			printf("\n");
		if (i==69)
			break;
	}
	printf("...\n");

	scsi_free_scsi_task(task);

	if ((task = iscsi_read10_task(iscsi, clnt->lun, 0, clnt->block_size, clnt->block_size, 0, 0, 0, 0, 0, read10_cb, private_data)) == NULL) {
		printf("failed to send read10 command\n");
		exit(10);
	}
	/* provide a buffer from the application to read into instead
	 * of copying and linearizing the data. This saves two copies
	 * of the data. One in libiscsi and one in the application
	 * callback.
	 */
	scsi_task_add_data_in_buffer(task, 7, &small_buffer[0]);
	scsi_task_add_data_in_buffer(task, 8, &small_buffer[7]);
	scsi_task_add_data_in_buffer(task, 512-15, &small_buffer[15]);
}

void readcapacity10_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_readcapacity10 *rc10;
	int full_size;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Readcapacity10 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	full_size = scsi_datain_getfullsize(task);
	if (full_size < task->datain.size) {
		printf("not enough data for full size readcapacity10\n");
		scsi_free_scsi_task(task);
		exit(10);
	}

	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall readcapacity10 data\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
	clnt->block_size = rc10->block_size;
	printf("READCAPACITY10 successful. Size:%d blocks  blocksize:%d. Read first block\n", rc10->lba, rc10->block_size);

	if (iscsi_read6_task(iscsi, clnt->lun, 0, clnt->block_size, clnt->block_size, read6_cb, private_data) == NULL) {
		printf("failed to send read6 command\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
	scsi_free_scsi_task(task);
}

void modesense6_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_mode_sense *ms;
	int full_size;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Modesense6 failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		exit(10);
	} else {
		full_size = scsi_datain_getfullsize(task);
		if (full_size > task->datain.size) {
			printf("did not get enough data for mode sense, sening modesense again asking for bigger buffer\n");
			if (iscsi_modesense6_task(iscsi, clnt->lun, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, full_size, modesense6_cb, private_data) == NULL) {
				printf("failed to send modesense6 command\n");
				scsi_free_scsi_task(task);
				exit(10);
			}
			scsi_free_scsi_task(task);
			return;
		}
	
	}
	printf("MODESENSE6 successful.\n");
	ms = scsi_datain_unmarshall(task);
	if (ms == NULL) {
		printf("failed to unmarshall mode sense datain blob\n");
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("Send READCAPACITY10\n");
	if (iscsi_readcapacity10_task(iscsi, clnt->lun, 0, 0, readcapacity10_cb, private_data) == NULL) {
		printf("failed to send readcapacity command\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
	scsi_free_scsi_task(task);
}

void inquiry_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_inquiry_standard *inq;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("Inquiry failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("INQUIRY successful for standard data.\n");
	inq = scsi_datain_unmarshall(task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(task);
		exit(10);
	}

	printf("Device Type is %d. VendorId:%s ProductId:%s\n", inq->device_type, inq->vendor_identification, inq->product_identification);
	printf("Send MODESENSE6\n");
	if (iscsi_modesense6_task(iscsi, clnt->lun, 0, SCSI_MODESENSE_PC_CURRENT, SCSI_MODEPAGE_RETURN_ALL_PAGES, 0, 4, modesense6_cb, private_data) == NULL) {
		printf("failed to send modesense6 command\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
	scsi_free_scsi_task(task);
}

void testunitready_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;

	if (status == SCSI_STATUS_CHECK_CONDITION) {
		printf("First testunitready failed with sense key:%d ascq:%04x\n", task->sense.key, task->sense.ascq);
		if (task->sense.key == SCSI_SENSE_UNIT_ATTENTION && task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET) {
			printf("target device just came online, try again\n");

			if (iscsi_testunitready_task(iscsi, clnt->lun, testunitready_cb, private_data) == NULL) {
				printf("failed to send testunitready command\n");
				scsi_free_scsi_task(task);
				exit(10);
			}
		}
		scsi_free_scsi_task(task);
		return;
	}

	printf("TESTUNITREADY successful, do an inquiry on lun:%d\n", clnt->lun);
	if (iscsi_inquiry_task(iscsi, clnt->lun, 0, 0, 64, inquiry_cb, private_data) == NULL) {
		printf("failed to send inquiry command : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		exit(10);
	}
	scsi_free_scsi_task(task);
}


void reportluns_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct scsi_task *task = command_data;
	struct scsi_reportluns_list *list;
	int full_report_size;
	int i;

	if (status != SCSI_STATUS_GOOD) {
		printf("Reportluns failed with : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		return;
	}

	full_report_size = scsi_datain_getfullsize(task);

	printf("REPORTLUNS status:%d   data size:%d,   full reports luns data size:%d\n", status, task->datain.size, full_report_size);
	if (full_report_size > task->datain.size) {
		printf("We did not get all the data we need in reportluns, ask again\n");
		if (iscsi_reportluns_task(iscsi, 0, full_report_size, reportluns_cb, private_data) == NULL) {
			printf("failed to send reportluns command\n");
			scsi_free_scsi_task(task);
			exit(10);
		}
		scsi_free_scsi_task(task);
		return;
	}

	
	list = scsi_datain_unmarshall(task);
	if (list == NULL) {
		printf("failed to unmarshall reportluns datain blob\n");
		scsi_free_scsi_task(task);
		exit(10);
	}
	for (i=0; i < (int)list->num; i++) {
		printf("LUN:%d found\n", list->luns[i]);
		clnt->lun = list->luns[i];
	}

	printf("Will use LUN:%d\n", clnt->lun);
	printf("Send testunitready to lun %d\n", clnt->lun);
	if (iscsi_testunitready_task(iscsi, clnt->lun, testunitready_cb, private_data) == NULL) {
		printf("failed to send testunitready command : %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		exit(10);
	}
	scsi_free_scsi_task(task);
}


void normallogin_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	if (status != 0) {
		printf("Failed to log in to target : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	printf("Logged in normal session, send reportluns\n");
	if (iscsi_reportluns_task(iscsi, 0, 16, reportluns_cb, private_data) == NULL) {
		printf("failed to send reportluns command : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}


void normalconnect_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	printf("Connected to iscsi socket\n");

	if (status != 0) {
		printf("normalconnect_cb: connection  failed status:%d\n", status);
		exit(10);
	}

	printf("connected, send login command\n");
	iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
	iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_CRC32C_NONE);
	if (iscsi_login_async(iscsi, normallogin_cb, private_data) != 0) {
		printf("iscsi_login_async failed\n");
		exit(10);
	}
}



void discoverylogout_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	
	printf("discovery session logged out, Message from main() was:[%s]\n", clnt->message);

	if (status != 0) {
		printf("Failed to logout from target. : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	printf("disconnect socket\n");
	if (iscsi_disconnect(iscsi) != 0) {
		printf("Failed to disconnect old socket\n");
		exit(10);
	}

	printf("reconnect with normal login to [%s]\n", clnt->target_address);
	printf("Use targetname [%s] when connecting\n", clnt->target_name);
	if (iscsi_set_targetname(iscsi, clnt->target_name)) {
		printf("Failed to set target name\n");
		exit(10);
	}
	if (iscsi_set_alias(iscsi, "ronnie") != 0) {
		printf("Failed to add alias\n");
		exit(10);
	}
	if (iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL) != 0) {
		printf("Failed to set settion type to normal\n");
		exit(10);
	}

	if (iscsi_connect_async(iscsi, clnt->target_address, normalconnect_cb, clnt) != 0) {
		printf("iscsi_connect failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}

void discovery_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data)
{
	struct client_state *clnt = (struct client_state *)private_data;
	struct iscsi_discovery_address *addr;

	printf("discovery callback   status:%04x\n", status);

	if (status != 0 || command_data == NULL) {
		printf("Failed to do discovery on target. : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	for(addr = command_data; addr; addr = addr->next) {	
		printf("Target:%s Address:%s\n", addr->target_name, addr->portals->portal);
	}

	addr=command_data;
	clnt->has_discovered_target = 1;
	clnt->target_name    = strdup(addr->target_name);
	clnt->target_address = strdup(addr->portals->portal);


	printf("discovery complete, send logout command\n");

	if (iscsi_logout_async(iscsi, discoverylogout_cb, private_data) != 0) {
		printf("iscsi_logout_async failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}


void discoverylogin_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	if (status != 0) {
		printf("Failed to log in to target. : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	printf("Logged in to target, send discovery command\n");
	if (iscsi_discovery_async(iscsi, discovery_cb, private_data) != 0) {
		printf("failed to send discovery command : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

}

void discoveryconnect_cb(struct iscsi_context *iscsi, int status, void *command_data _U_, void *private_data)
{
	printf("Connected to iscsi socket status:0x%08x\n", status);

	if (status != 0) {
		printf("discoveryconnect_cb: connection failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	printf("connected, send login command\n");
	iscsi_set_session_type(iscsi, ISCSI_SESSION_DISCOVERY);
	if (iscsi_login_async(iscsi, discoverylogin_cb, private_data) != 0) {
		printf("iscsi_login_async failed : %s\n", iscsi_get_error(iscsi));
		exit(10);
	}
}


int main(int argc _U_, char *argv[] _U_)
{
	struct iscsi_context *iscsi;
	struct pollfd pfd;
	struct client_state clnt;

	printf("iscsi client\n");
#if defined(_WIN32)
	if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
		printf("Failed to start Winsock2\n");
		exit(10);
	}
#endif


	memset(&clnt, 0, sizeof(clnt));

	iscsi = iscsi_create_context("iqn.2002-10.com.ronnie:client");
	if (iscsi == NULL) {
		printf("Failed to create context\n");
		exit(10);
	}

	if (iscsi_set_alias(iscsi, "ronnie") != 0) {
		printf("Failed to add alias\n");
		exit(10);
	}

	clnt.message = "Hello iSCSI";
	clnt.has_discovered_target = 0;
	if (iscsi_connect_async(iscsi, TARGET, discoveryconnect_cb, &clnt) != 0) {
		printf("iscsi_connect failed. %s\n", iscsi_get_error(iscsi));
		exit(10);
	}

	while (clnt.finished == 0) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);

		if (poll(&pfd, 1, -1) < 0) {
			printf("Poll failed");
			exit(10);
		}
		if (iscsi_service(iscsi, pfd.revents) < 0) {
			printf("iscsi_service failed with : %s\n", iscsi_get_error(iscsi));
			break;
		}
	}


	iscsi_destroy_context(iscsi);

	if (clnt.target_name != NULL) {
		free(clnt.target_name);
	}
	if (clnt.target_address != NULL) {
		free(clnt.target_address);
	}

	printf("ok\n");
	return 0;
}

