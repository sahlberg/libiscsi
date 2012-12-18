/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
/*
 * would be nice if this could grow into a full blown library to
 * 1, build and unmarshall a CDB
 * 2, check how big a complete data-in structure needs to be
 * 3, unmarshall data-in into a real structure
 * 4, marshall a real structure into a data-out blob
 */

#if defined(WIN32)
#include <winsock2.h>
#else
#include <strings.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "slist.h"
#include "scsi-lowlevel.h"

struct scsi_allocated_memory {
	struct scsi_allocated_memory *next;
	char buf[0];
};

void
scsi_free_scsi_task(struct scsi_task *task)
{
	struct scsi_allocated_memory *mem;

	while ((mem = task->mem)) {
		   SLIST_REMOVE(&task->mem, mem);
		   free(mem);
	}

	free(task->datain.data);
	free(task);
}

void *
scsi_malloc(struct scsi_task *task, size_t size)
{
	struct scsi_allocated_memory *mem;

	mem = malloc(sizeof(struct scsi_allocated_memory) + size);
	if (mem == NULL) {
		return NULL;
	}
	memset(mem, 0, sizeof(struct scsi_allocated_memory) + size);
	SLIST_ADD(&task->mem, mem);
	return &mem->buf[0];
}

struct value_string {
   int value;
   const char *string;
};

static const char *
value_string_find(struct value_string *values, int value)
{
	for (; values->string; values++) {
		if (value == values->value) {
			return values->string;
		}
	}
	return NULL;
}

const char *
scsi_sense_key_str(int key)
{
	struct value_string keys[] = {
		{SCSI_SENSE_NO_SENSE,
		 "NO SENSE"},
		{SCSI_SENSE_RECOVERED_ERROR,
		 "RECOVERED ERROR"},
		{SCSI_SENSE_NOT_READY,
		 "NOT READY"},
		{SCSI_SENSE_HARDWARE_ERROR,
		 "HARDWARE_ERROR"},
		{SCSI_SENSE_ILLEGAL_REQUEST,
		 "ILLEGAL_REQUEST"},
		{SCSI_SENSE_UNIT_ATTENTION,
		 "UNIT_ATTENTION"},
		{SCSI_SENSE_DATA_PROTECTION,
		 "DATA PROTECTION"},
		{SCSI_SENSE_BLANK_CHECK,
		 "BLANK CHECK"},
		{SCSI_SENSE_VENDOR_SPECIFIC,
		 "VENDOR SPECIFIC"},
		{SCSI_SENSE_COPY_ABORTED,
		 "COPY ABORTED"},
		{SCSI_SENSE_COMMAND_ABORTED,
		 "COMMAND ABORTED"},
		{SCSI_SENSE_OBSOLETE_ERROR_CODE,
		 "OBSOLETE_ERROR_CODE"},
		{SCSI_SENSE_OVERFLOW_COMMAND,
		 "OVERFLOW_COMMAND"},
		{SCSI_SENSE_MISCOMPARE,
		 "MISCOMPARE"},
	       {0, NULL}
	};

	return value_string_find(keys, key);
}

const char *
scsi_sense_ascq_str(int ascq)
{
	struct value_string ascqs[] = {
		{SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE,
		 "INVALID_OPERATION_CODE"},
		{SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE,
		 "LBA_OUT_OF_RANGE"},
		{SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB,
		 "INVALID_FIELD_IN_CDB"},
		{SCSI_SENSE_ASCQ_LOGICAL_UNIT_NOT_SUPPORTED,
		 "LOGICAL_UNIT_NOT_SUPPORTED"},
		{SCSI_SENSE_ASCQ_WRITE_PROTECTED,
		 "WRITE_PROTECTED"},
		{SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT,
		 "MEDIUM_NOT_PRESENT"},
		{SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED,
		 "MEDIUM_NOT_PRESENT-TRAY_CLOSED"},
		{SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN,
		 "MEDIUM_NOT_PRESENT-TRAY_OPEN"},
		{SCSI_SENSE_ASCQ_BUS_RESET,
		 "BUS_RESET"},
		{SCSI_SENSE_ASCQ_INTERNAL_TARGET_FAILURE,
		 "INTERNAL_TARGET_FAILURE"},
		{SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY,
		 "MISCOMPARE_DURING_VERIFY"},
		{ SCSI_SENSE_ASCQ_MEDIUM_LOAD_OR_EJECT_FAILED,
		  "MEDIUM_LOAD_OR_EJECT_FAILED" },
		{SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED,
		 "SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED"},
	       {0, NULL}
	};

	return value_string_find(ascqs, ascq);
}

inline uint32_t
scsi_get_uint32(const unsigned char *c)
{
	return ntohl(*(uint32_t *)c);
}

inline uint16_t
scsi_get_uint16(const unsigned char *c)
{
	return ntohs(*(uint16_t *)c);
}

inline void
scsi_set_uint32(unsigned char *c, uint32_t val)
{
	*(uint32_t *)c = htonl(val);
}

inline void
scsi_set_uint16(unsigned char *c, uint16_t val)
{
	*(uint16_t *)c = htons(val);
}

/*
 * TESTUNITREADY
 */
struct scsi_task *
scsi_cdb_testunitready(void)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_TESTUNITREADY;

	task->cdb_size   = 6;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}


/*
 * REPORTLUNS
 */
struct scsi_task *
scsi_reportluns_cdb(int report_type, int alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_REPORTLUNS;
	task->cdb[2]   = report_type;
	scsi_set_uint32(&task->cdb[6], alloc_len);

	task->cdb_size = 12;
	if (alloc_len != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = alloc_len;

	return task;
}

/*
 * parse the data in blob and calculate the size of a full report luns
 * datain structure
 */
static int
scsi_reportluns_datain_getfullsize(struct scsi_task *task)
{
	uint32_t list_size;

	list_size = scsi_get_uint32(&task->datain.data[0]) + 8;

	return list_size;
}

/*
 * unmarshall the data in blob for reportluns into a structure
 */
static struct scsi_reportluns_list *
scsi_reportluns_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_reportluns_list *list;
	int list_size;
	int i, num_luns;

	if (task->datain.size < 4) {
		return NULL;
	}

	list_size = scsi_get_uint32(&task->datain.data[0]) + 8;
	if (list_size < task->datain.size) {
		return NULL;
	}

	num_luns = list_size / 8 - 1;
	list = scsi_malloc(task, offsetof(struct scsi_reportluns_list, luns)
			   + sizeof(uint16_t) * num_luns);
	if (list == NULL) {
		return NULL;
	}

	list->num = num_luns;
	for (i = 0; i < num_luns; i++) {
		list->luns[i] = scsi_get_uint16(&task->datain.data[i*8+8]);
	}

	return list;
}

/*
 * READCAPACITY10
 */
struct scsi_task *
scsi_cdb_readcapacity10(int lba, int pmi)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READCAPACITY10;

	scsi_set_uint32(&task->cdb[2], lba);

	if (pmi) {
		task->cdb[8] |= 0x01;
	}

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 8;

	return task;
}

/*
 * READTOC
 */
struct scsi_task *
scsi_cdb_readtoc(int msf, int format, int track_session, uint16_t alloc_len)
{
	struct scsi_task *task;

	if (format != SCSI_READ_TOC && format != SCSI_READ_SESSION_INFO
	    && format != SCSI_READ_FULL_TOC){
		fprintf(stderr, "Read TOC format %d not fully supported yet\n", format);
		return NULL;
	}

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READTOC;

	if (msf) {
		task->cdb[1] |= 0x02;
	}

	task->cdb[2] = format & 0xf;

	/* Prevent invalid setting of Track/Session Number */
	if (format == SCSI_READ_TOC || format == SCSI_READ_FULL_TOC) {
		task->cdb[6] = 0xff & track_session;
	}

	scsi_set_uint16(&task->cdb[7], alloc_len);

	task->cdb_size = 10;
	if (alloc_len != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = alloc_len;

	return task;
}

/*
 * parse the data in blob and calculate the size of a full read TOC
 * datain structure
 */
static int
scsi_readtoc_datain_getfullsize(struct scsi_task *task)
{
	uint16_t toc_data_len;

	toc_data_len = scsi_get_uint16(&task->datain.data[0]) + 2;

	return toc_data_len;
}

static inline enum scsi_readtoc_fmt
scsi_readtoc_format(const struct scsi_task *task)
{
	return task->cdb[2] & 0xf;
}

static void
scsi_readtoc_desc_unmarshall(struct scsi_task *task, struct scsi_readtoc_list *list, int i)
{
	switch(scsi_readtoc_format(task)) {
	case SCSI_READ_TOC:
		list->desc[i].desc.toc.adr
			= task->datain.data[4+8*i+1] & 0xf0;
		list->desc[i].desc.toc.control
			= task->datain.data[4+8*i+1] & 0x0f;
		list->desc[i].desc.toc.track
			= task->datain.data[4+8*i+2];
		list->desc[i].desc.toc.lba
			= scsi_get_uint32(&task->datain.data[4+8*i+4]);
		break;
	case SCSI_READ_SESSION_INFO:
		list->desc[i].desc.ses.adr
			= task->datain.data[4+8*i+1] & 0xf0;
		list->desc[i].desc.ses.control
			= task->datain.data[4+8*i+1] & 0x0f;
		list->desc[i].desc.ses.first_in_last
			= task->datain.data[4+8*i+2];
		list->desc[i].desc.ses.lba
			= scsi_get_uint32(&task->datain.data[4+8*i+4]);
		break;
	case SCSI_READ_FULL_TOC:
		list->desc[i].desc.full.session
			= task->datain.data[4+11*i+0] & 0xf0;
		list->desc[i].desc.full.adr
			= task->datain.data[4+11*i+1] & 0xf0;
		list->desc[i].desc.full.control
			= task->datain.data[4+11*i+1] & 0x0f;
		list->desc[i].desc.full.tno
			= task->datain.data[4+11*i+2];
		list->desc[i].desc.full.point
			= task->datain.data[4+11*i+3];
		list->desc[i].desc.full.min
			= task->datain.data[4+11*i+4];
		list->desc[i].desc.full.sec
			= task->datain.data[4+11*i+5];
		list->desc[i].desc.full.frame
			= task->datain.data[4+11*i+6];
		list->desc[i].desc.full.zero
			= task->datain.data[4+11*i+7];
		list->desc[i].desc.full.pmin
			= task->datain.data[4+11*i+8];
		list->desc[i].desc.full.psec
			= task->datain.data[4+11*i+9];
		list->desc[i].desc.full.pframe
			= task->datain.data[4+11*i+10];
		break;
	default:
		break;
	}
}

/*
 * unmarshall the data in blob for read TOC into a structure
 */
static struct scsi_readtoc_list *
scsi_readtoc_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_readtoc_list *list;
	int data_len;
	int i, num_desc;

	if (task->datain.size < 4) {
		return NULL;
	}

	/* Do we have all data? */
	data_len = scsi_readtoc_datain_getfullsize(task) - 2;
	if(task->datain.size < data_len) {
		return NULL;
	}

	/* Remove header size (4) to get bytes in descriptor list */
	num_desc = (data_len - 4) / 8;

	list = scsi_malloc(task, offsetof(struct scsi_readtoc_list, desc)
			   + sizeof(struct scsi_readtoc_desc) * num_desc);
	if (list == NULL) {
		return NULL;
	}

	list->num = num_desc;
	list->first = task->datain.data[2];
	list->last = task->datain.data[3];

	for (i = 0; i < num_desc; i++) {
		scsi_readtoc_desc_unmarshall(task, list, i);
	}

	return list;
}

/*
 * RESERVE6
 */
struct scsi_task *
scsi_cdb_reserve6(void)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_RESERVE6;

	task->cdb_size = 6;
	task->xfer_dir = SCSI_XFER_NONE;

	return task;
}
/*
 * RELEASE10
 */
struct scsi_task *
scsi_cdb_release6(void)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0] = SCSI_OPCODE_RELEASE6;

	task->cdb_size = 6;
	task->xfer_dir = SCSI_XFER_NONE;

	return task;
}

static inline uint8_t
scsi_serviceactionin_sa(const struct scsi_task *task)
{
	return task->cdb[1];
}

/*
 * service_action_in unmarshall
 */
static void *
scsi_serviceactionin_datain_unmarshall(struct scsi_task *task)
{
	switch (scsi_serviceactionin_sa(task)) {
	case SCSI_READCAPACITY16: {
		struct scsi_readcapacity16 *rc16 = scsi_malloc(task,
							       sizeof(*rc16));
		if (rc16 == NULL) {
			return NULL;
		}
		rc16->returned_lba = scsi_get_uint32(&task->datain.data[0]);
		rc16->returned_lba = (rc16->returned_lba << 32) | scsi_get_uint32(&task->datain.data[4]);
		rc16->block_length = scsi_get_uint32(&task->datain.data[8]);
		rc16->p_type       = (task->datain.data[12] >> 1) & 0x07;
		rc16->prot_en      = task->datain.data[12] & 0x01;
		rc16->p_i_exp      = (task->datain.data[13] >> 4) & 0x0f;
		rc16->lbppbe       = task->datain.data[13] & 0x0f;
		rc16->lbpme        = !!(task->datain.data[14] & 0x80);
		rc16->lbprz        = !!(task->datain.data[14] & 0x40);
		rc16->lalba        = scsi_get_uint16(&task->datain.data[14]) & 0x3fff;
		return rc16;
	}
	case SCSI_GET_LBA_STATUS: {
		struct scsi_get_lba_status *gls = scsi_malloc(task,
							      sizeof(*gls));
		int32_t len = scsi_get_uint32(&task->datain.data[0]);
		int i;

		if (gls == NULL) {
			return NULL;
		}

		if (len > task->datain.size - 4) {
			len = task->datain.size - 4;
		}
		len = len / 16;

		gls->num_descriptors = len;
		gls->descriptors = scsi_malloc(task,
					       sizeof(*gls->descriptors) * len);
		if (gls->descriptors == NULL) {
			free(gls);
			return NULL;
		}

		for (i = 0; i < (int)gls->num_descriptors; i++) {
			gls->descriptors[i].lba  = scsi_get_uint32(&task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 0]);
			gls->descriptors[i].lba <<= 32;
 			gls->descriptors[i].lba |= scsi_get_uint32(&task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 4]);

			gls->descriptors[i].num_blocks = scsi_get_uint32(&task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 8]);

			gls->descriptors[i].provisioning = task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 12] & 0x0f;
		}

		return gls;
	}
	default:
		return NULL;
	}
}

static inline int
scsi_maintenancein_return_timeouts(const struct scsi_task *task)
{
	return task->cdb[2] & 0x80;
}

static inline uint8_t
scsi_maintenancein_sa(const struct scsi_task *task)
{
	return task->cdb[1];
}

/*
 * parse the data in blob and calculate the size of a full maintenancein
 * datain structure
 */
static int
scsi_maintenancein_datain_getfullsize(struct scsi_task *task)
{

	switch (scsi_maintenancein_sa(task)) {
	case SCSI_REPORT_SUPPORTED_OP_CODES:
		return scsi_get_uint32(&task->datain.data[0]) + 4;
	default:
		return -1;
	}
}

/*
 * maintenance_in unmarshall
 */
static void *
scsi_maintenancein_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_report_supported_op_codes *rsoc;
	struct scsi_command_descriptor *desc, *datain;
	uint32_t len, i;
	int return_timeouts, desc_size;

	switch (scsi_maintenancein_sa(task)) {
	case SCSI_REPORT_SUPPORTED_OP_CODES:
		if (task->datain.size < 4) {
			return NULL;
		}

		len = scsi_get_uint32(&task->datain.data[0]);
		rsoc = scsi_malloc(task, sizeof(struct scsi_report_supported_op_codes) + len);
		if (rsoc == NULL) {
			return NULL;
		}
		/* Does the descriptor include command timeout info? */
		return_timeouts = scsi_maintenancein_return_timeouts(task);

		/* Size of descriptor depends on whether timeout included. */
		desc_size = sizeof (struct scsi_command_descriptor);
		if (return_timeouts) {
			desc_size += sizeof (struct scsi_op_timeout_descriptor);
		}
		rsoc->num_descriptors = len / desc_size;

		desc = &rsoc->descriptors[0];
		datain = (struct scsi_command_descriptor *)&task->datain.data[4];

		for (i=0; i < rsoc->num_descriptors; i++) {
			desc->op_code = datain->op_code;
			desc->service_action = ntohs(datain->service_action);
			desc->cdb_length =  ntohs(datain->cdb_length);
			if (return_timeouts) {
				desc->to[0].descriptor_length = ntohs(datain->to[0].descriptor_length);
				desc->to[0].command_specific = datain->to[0].command_specific;
				desc->to[0].nominal_processing_timeout
					= ntohl(datain->to[0].nominal_processing_timeout);
				desc->to[0].recommended_timeout
					= ntohl(datain->to[0].recommended_timeout);
			}
			desc = (struct scsi_command_descriptor *)((char *)desc + desc_size);
			datain = (struct scsi_command_descriptor *)((char *)datain + desc_size);
		}

		return rsoc;
	};

	return NULL;
}

/*
 * MAINTENANCE In / Read Supported Op Codes
 */
struct scsi_task *
scsi_cdb_report_supported_opcodes(int return_timeouts, uint32_t alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MAINTENANCE_IN;
	task->cdb[1]   = SCSI_REPORT_SUPPORTED_OP_CODES;
	task->cdb[2]   = SCSI_REPORT_SUPPORTING_OPS_ALL;

	if (return_timeouts) {
		task->cdb[2] |= 0x80;
	}

	scsi_set_uint32(&task->cdb[6], alloc_len);

	task->cdb_size = 12;
	if (alloc_len != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = alloc_len;

	return task;
}

/*
 * parse the data in blob and calculate the size of a full
 * readcapacity10 datain structure
 */
static int
scsi_readcapacity10_datain_getfullsize(struct scsi_task *task _U_)
{
	return 8;
}

/*
 * unmarshall the data in blob for readcapacity10 into a structure
 */
static struct scsi_readcapacity10 *
scsi_readcapacity10_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_readcapacity10 *rc10;

	if (task->datain.size < 8) {
		return NULL;
	}
	rc10 = scsi_malloc(task, sizeof(struct scsi_readcapacity10));
	if (rc10 == NULL) {
		return NULL;
	}

	rc10->lba        = scsi_get_uint32(&task->datain.data[0]);
	rc10->block_size = scsi_get_uint32(&task->datain.data[4]);

	return rc10;
}

/*
 * INQUIRY
 */
struct scsi_task *
scsi_cdb_inquiry(int evpd, int page_code, int alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_INQUIRY;

	if (evpd) {
		task->cdb[1] |= 0x01;
	}

	task->cdb[2] = page_code;

	scsi_set_uint16(&task->cdb[3], alloc_len);

	task->cdb_size = 6;
	if (alloc_len != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = alloc_len;

	return task;
}

static inline int
scsi_inquiry_evpd_set(const struct scsi_task *task)
{
	return task->cdb[1] & 0x1;
}

static inline uint8_t
scsi_inquiry_page_code(const struct scsi_task *task)
{
	return task->cdb[2];
}

/*
 * parse the data in blob and calculate the size of a full
 * inquiry datain structure
 */
static int
scsi_inquiry_datain_getfullsize(struct scsi_task *task)
{
	if (scsi_inquiry_evpd_set(task) == 0) {
		return task->datain.data[4] + 5;
	}

	switch (scsi_inquiry_page_code(task)) {
	case SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES:
	case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
	case SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER:
		return task->datain.data[3] + 4;
	case SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION:
	case SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS:
	case SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING:
		return scsi_get_uint16(&task->datain.data[2]) + 4;
	default:
		return -1;
	}
}

static struct scsi_inquiry_standard *
scsi_inquiry_unmarshall_standard(struct scsi_task *task)
{
	struct scsi_inquiry_standard *inq = scsi_malloc(task, sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}

	inq->qualifier              = (task->datain.data[0]>>5)&0x07;
	inq->device_type            = task->datain.data[0]&0x1f;
	inq->rmb                    = !!(task->datain.data[1]&0x80);
	inq->version                = task->datain.data[2];
	inq->normaca                = !!(task->datain.data[3]&0x20);
	inq->hisup                  = !!(task->datain.data[3]&0x10);
	inq->response_data_format   = task->datain.data[3]&0x0f;

	inq->additional_length      = task->datain.data[4];

	inq->sccs                   = !!(task->datain.data[5]&0x80);
	inq->acc                    = !!(task->datain.data[5]&0x40);
	inq->tpgs                   = (task->datain.data[5]>>4)&0x03;
	inq->threepc                = !!(task->datain.data[5]&0x08);
	inq->protect                = !!(task->datain.data[5]&0x01);

	inq->encserv                = !!(task->datain.data[6]&0x40);
	inq->multip                 = !!(task->datain.data[6]&0x10);
	inq->addr16                 = !!(task->datain.data[6]&0x01);
	inq->wbus16                 = !!(task->datain.data[7]&0x20);
	inq->sync                   = !!(task->datain.data[7]&0x10);
	inq->cmdque                 = !!(task->datain.data[7]&0x02);

	memcpy(&inq->vendor_identification[0],
	       &task->datain.data[8], 8);
	memcpy(&inq->product_identification[0],
	       &task->datain.data[16], 16);
	memcpy(&inq->product_revision_level[0],
	       &task->datain.data[32], 4);

	inq->clocking               = (task->datain.data[56]>>2)&0x03;
	inq->qas                    = !!(task->datain.data[56]&0x02);
	inq->ius                    = !!(task->datain.data[56]&0x01);

	return inq;
}

static struct scsi_inquiry_supported_pages *
scsi_inquiry_unmarshall_supported_pages(struct scsi_task *task)
{
	struct scsi_inquiry_supported_pages *inq = scsi_malloc(task,
							       sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier = (task->datain.data[0]>>5)&0x07;
	inq->device_type = task->datain.data[0]&0x1f;
	inq->pagecode = task->datain.data[1];

	inq->num_pages = task->datain.data[3];
	inq->pages = scsi_malloc(task, inq->num_pages);
	if (inq->pages == NULL) {
		free (inq);
		return NULL;
	}
	memcpy(inq->pages, &task->datain.data[4], inq->num_pages);
	return inq;
}

static struct scsi_inquiry_unit_serial_number *
scsi_inquiry_unmarshall_unit_serial_number(struct scsi_task* task)
{
	struct scsi_inquiry_unit_serial_number *inq = scsi_malloc(task,
								  sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier = (task->datain.data[0]>>5)&0x07;
	inq->device_type = task->datain.data[0]&0x1f;
	inq->pagecode = task->datain.data[1];

	inq->usn = scsi_malloc(task, task->datain.data[3]+1);
	if (inq->usn == NULL) {
		free(inq);
		return NULL;
	}
	memcpy(inq->usn, &task->datain.data[4], task->datain.data[3]);
	inq->usn[task->datain.data[3]] = 0;
	return inq;
}

static struct scsi_inquiry_device_identification *
scsi_inquiry_unmarshall_device_identification(struct scsi_task *task)
{
	struct scsi_inquiry_device_identification *inq = scsi_malloc(task,
								     sizeof(*inq));
	int remaining = scsi_get_uint16(&task->datain.data[2]);
	unsigned char *dptr;

	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier             = (task->datain.data[0]>>5)&0x07;
	inq->device_type           = task->datain.data[0]&0x1f;
	inq->pagecode              = task->datain.data[1];

	dptr = &task->datain.data[4];
	while (remaining > 0) {
		struct scsi_inquiry_device_designator *dev =
			scsi_malloc(task, sizeof(*dev));
		if (dev == NULL) {
			goto err;
		}

		dev->next = inq->designators;
		inq->designators = dev;

		dev->protocol_identifier = (dptr[0]>>4) & 0x0f;
		dev->code_set            = dptr[0] & 0x0f;
		dev->piv                 = !!(dptr[1]&0x80);
		dev->association         = (dptr[1]>>4)&0x03;
		dev->designator_type     = dptr[1]&0x0f;

		dev->designator_length   = dptr[3];
		dev->designator = scsi_malloc(task, dev->designator_length + 1);
		if (dev->designator == NULL) {
			goto err;
		}
		dev->designator[dev->designator_length] = 0;
		memcpy(dev->designator, &dptr[4],
		       dev->designator_length);

		remaining -= 4;
		remaining -= dev->designator_length;

		dptr += dev->designator_length + 4;
	}
	return inq;

 err:
	while (inq->designators) {
		struct scsi_inquiry_device_designator *dev = inq->designators;
		inq->designators = dev->next;
		free(dev->designator);
		free(dev);
	}

	free(inq);
	return NULL;
}

static struct scsi_inquiry_block_limits *
scsi_inquiry_unmarshall_block_limits(struct scsi_task *task)
{
	struct scsi_inquiry_block_limits *inq = scsi_malloc(task,
							    sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier             = (task->datain.data[0]>>5)&0x07;
	inq->device_type           = task->datain.data[0]&0x1f;
	inq->pagecode              = task->datain.data[1];

	inq->wsnz                  = task->datain.data[4] & 0x01;
	inq->max_cmp               = task->datain.data[5];
	inq->opt_gran              = scsi_get_uint16(&task->datain.data[6]);
	inq->max_xfer_len          = scsi_get_uint32(&task->datain.data[8]);
	inq->opt_xfer_len          = scsi_get_uint32(&task->datain.data[12]);
	inq->max_prefetch          = scsi_get_uint32(&task->datain.data[16]);
	inq->max_unmap             = scsi_get_uint32(&task->datain.data[20]);
	inq->max_unmap_bdc         = scsi_get_uint32(&task->datain.data[24]);
	inq->opt_unmap_gran        = scsi_get_uint32(&task->datain.data[28]);
	inq->ugavalid              = !!(task->datain.data[32]&0x80);
	inq->unmap_gran_align      = scsi_get_uint32(&task->datain.data[32]) & 0x7fffffff;
	inq->max_ws_len            = scsi_get_uint32(&task->datain.data[36]);
	inq->max_ws_len            = (inq->max_ws_len << 32) | scsi_get_uint32(&task->datain.data[40]);

	return inq;
}

static struct scsi_inquiry_block_device_characteristics *
scsi_inquiry_unmarshall_block_device_characteristics(struct scsi_task *task)
{
	struct scsi_inquiry_block_device_characteristics *inq =
		scsi_malloc(task, sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier             = (task->datain.data[0]>>5)&0x07;
	inq->device_type           = task->datain.data[0]&0x1f;
	inq->pagecode              = task->datain.data[1];

	inq->medium_rotation_rate  = scsi_get_uint16(&task->datain.data[4]);
	return inq;
}

struct scsi_inquiry_logical_block_provisioning *
scsi_inquiry_unmarshall_logical_block_provisioning(struct scsi_task *task)
{
	struct scsi_inquiry_logical_block_provisioning *inq =
		scsi_malloc(task, sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier             = (task->datain.data[0]>>5)&0x07;
	inq->device_type           = task->datain.data[0]&0x1f;
	inq->pagecode              = task->datain.data[1];

	inq->threshold_exponent = task->datain.data[4];
	inq->lbpu               = !!(task->datain.data[5] & 0x80);
	inq->lbpws              = !!(task->datain.data[5] & 0x40);
	inq->lbpws10            = !!(task->datain.data[5] & 0x20);
	inq->lbprz              = !!(task->datain.data[5] & 0x04);
	inq->anc_sup            = !!(task->datain.data[5] & 0x02);
	inq->dp	                = !!(task->datain.data[5] & 0x01);
	inq->provisioning_type  = task->datain.data[6] & 0x07;

	return inq;
}

/*
 * unmarshall the data in blob for inquiry into a structure
 */
static void *
scsi_inquiry_datain_unmarshall(struct scsi_task *task)
{
	if (scsi_inquiry_evpd_set(task) == 0) {
		return scsi_inquiry_unmarshall_standard(task);
	}

	switch (scsi_inquiry_page_code(task))
	{
	case SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES:
		return scsi_inquiry_unmarshall_supported_pages(task);
	case SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER:
		return scsi_inquiry_unmarshall_unit_serial_number(task);
	case SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION:
		return scsi_inquiry_unmarshall_device_identification(task);
	case SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS:
		return scsi_inquiry_unmarshall_block_limits(task);
	case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
		return scsi_inquiry_unmarshall_block_device_characteristics(task);
	case  SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING:
		return scsi_inquiry_unmarshall_logical_block_provisioning(task);
	default:
		return NULL;
	}
}

/*
 * READ6
 */
struct scsi_task *
scsi_cdb_read6(uint32_t lba, uint32_t xferlen, int blocksize)
{
	struct scsi_task *task;
	int num_blocks;

	num_blocks = xferlen/blocksize;
	if (num_blocks > 265) {
		return NULL;
	}

	if (lba > 0x1fffff) {
		return NULL;
	}

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ6;
	task->cdb_size = 6;

	task->cdb[1] = (lba>>16)&0x1f;
	task->cdb[2] = (lba>> 8)&0xff;
	task->cdb[3] = (lba    )&0xff;

	if (num_blocks < 256) {
		task->cdb[4] = num_blocks;
	}

	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * READ10
 */
struct scsi_task *
scsi_cdb_read10(uint32_t lba, uint32_t xferlen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ10;

	task->cdb[1] |= ((rdprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint16(&task->cdb[7], xferlen/blocksize);

	task->cdb[6] |= (group_number & 0x1f);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * READ12
 */
struct scsi_task *
scsi_cdb_read12(uint32_t lba, uint32_t xferlen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ12;

	task->cdb[1] |= ((rdprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint32(&task->cdb[6], xferlen/blocksize);

	task->cdb[10] |= (group_number & 0x1f);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * READ16
 */
struct scsi_task *
scsi_cdb_read16(uint64_t lba, uint32_t xferlen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ16;

	task->cdb[1] |= ((rdprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITE10
 */
struct scsi_task *
scsi_cdb_write10(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE10;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint16(&task->cdb[7], xferlen/blocksize);

	task->cdb[6] |= (group_number & 0x1f);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITE12
 */
struct scsi_task *
scsi_cdb_write12(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE12;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint32(&task->cdb[6], xferlen/blocksize);

	task->cdb[10] |= (group_number & 0x1f);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITE16
 */
struct scsi_task *
scsi_cdb_write16(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE16;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * ORWRITE
 */
struct scsi_task *
scsi_cdb_orwrite(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_ORWRITE;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * COMPAREANDWRITE
 */
struct scsi_task *
scsi_cdb_compareandwrite(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_COMPARE_AND_WRITE;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}
	if (fua_nv) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	task->cdb[13] = xferlen/blocksize;

	task->cdb[14] |= (group_number & 0x1f);
	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * VERIFY10
 */
struct scsi_task *
scsi_cdb_verify10(uint32_t lba, uint32_t xferlen, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_VERIFY10;

	if (vprotect) {
		task->cdb[1] |= ((vprotect << 5) & 0xe0);
	}
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (bytchk) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint16(&task->cdb[7], xferlen/blocksize);

	task->cdb_size = 10;
	if (xferlen != 0 && bytchk) {
		task->xfer_dir = SCSI_XFER_WRITE;
		task->expxferlen = xferlen;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
		task->expxferlen = 0;
	}

	return task;
}

/*
 * VERIFY12
 */
struct scsi_task *
scsi_cdb_verify12(uint32_t lba, uint32_t xferlen, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_VERIFY12;

	if (vprotect) {
		task->cdb[1] |= ((vprotect << 5) & 0xe0);
	}
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (bytchk) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint32(&task->cdb[6], xferlen/blocksize);

	task->cdb_size = 12;
	if (xferlen != 0 && bytchk) {
		task->xfer_dir = SCSI_XFER_WRITE;
		task->expxferlen = xferlen;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
		task->expxferlen = 0;
	}

	return task;
}

/*
 * VERIFY16
 */
struct scsi_task *
scsi_cdb_verify16(uint64_t lba, uint32_t xferlen, int vprotect, int dpo, int bytchk, int blocksize)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_VERIFY16;

	if (vprotect) {
		task->cdb[1] |= ((vprotect << 5) & 0xe0);
	}
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (bytchk) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], xferlen/blocksize);

	task->cdb_size = 16;
	if (xferlen != 0 && bytchk) {
		task->xfer_dir = SCSI_XFER_WRITE;
		task->expxferlen = xferlen;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
		task->expxferlen = 0;
	}

	return task;
}

/*
 * UNMAP
 */
struct scsi_task *
scsi_cdb_unmap(int anchor, int group, uint16_t xferlen)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_UNMAP;

	if (anchor) {
		task->cdb[1] |= 0x01;
	}
	task->cdb[6] |= group & 0x1f;

	scsi_set_uint16(&task->cdb[7], xferlen);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * PERSISTENT_RESEERVE_IN
 */
struct scsi_task *
scsi_cdb_persistent_reserve_in(enum scsi_persistent_in_sa sa, uint16_t xferlen)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_PERSISTENT_RESERVE_IN;

	task->cdb[1] |= sa & 0x1f;

	scsi_set_uint16(&task->cdb[7], xferlen);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITE_SAME10
 */
struct scsi_task *
scsi_cdb_writesame10(int wrprotect, int anchor, int unmap, int pbdata, int lbdata, uint32_t lba, int group, uint16_t num_blocks)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE_SAME10;

	if (wrprotect) {
		task->cdb[1] |= ((wrprotect & 0x7) << 5);
	}
	if (anchor) {
		task->cdb[1] |= 0x10;
	}
	if (unmap) {
		task->cdb[1] |= 0x08;
	}
	if (pbdata) {
		task->cdb[1] |= 0x04;
	}
	if (lbdata) {
		task->cdb[1] |= 0x02;
	}
	scsi_set_uint32(&task->cdb[2], lba);
	if (group) {
		task->cdb[6] |= (group & 0x1f);
	}
	scsi_set_uint16(&task->cdb[7], num_blocks);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = 512;

	return task;
}

/*
 * WRITE_SAME16
 */
struct scsi_task *
scsi_cdb_writesame16(int wrprotect, int anchor, int unmap, int pbdata, int lbdata, uint64_t lba, int group, uint32_t num_blocks)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE_SAME16;

	if (wrprotect) {
		task->cdb[1] |= ((wrprotect & 0x7) << 5);
	}
	if (anchor) {
		task->cdb[1] |= 0x10;
	}
	if (unmap) {
		task->cdb[1] |= 0x08;
	}
	if (pbdata) {
		task->cdb[1] |= 0x04;
	}
	if (lbdata) {
		task->cdb[1] |= 0x02;
	}
	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], num_blocks);
	if (group) {
		task->cdb[14] |= (group & 0x1f);
	}

	task->cdb_size = 16;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = 512;

	return task;
}

/*
 * MODESENSE6
 */
struct scsi_task *
scsi_cdb_modesense6(int dbd, enum scsi_modesense_page_control pc,
		    enum scsi_modesense_page_code page_code,
		    int sub_page_code, unsigned char alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MODESENSE6;

	if (dbd) {
		task->cdb[1] |= 0x08;
	}
	task->cdb[2] = pc<<6 | page_code;
	task->cdb[3] = sub_page_code;
	task->cdb[4] = alloc_len;

	task->cdb_size = 6;
	if (alloc_len != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = alloc_len;

	return task;
}

/*
 * parse the data in blob and calculate the size of a full
 * modesense6 datain structure
 */
static int
scsi_modesense6_datain_getfullsize(struct scsi_task *task)
{
	int len;

	len = task->datain.data[0] + 1;

	return len;
}

static void
scsi_parse_mode_caching(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->caching.ic   = task->datain.data[pos] & 0x80;
	mp->caching.abpf = task->datain.data[pos] & 0x40;
	mp->caching.cap  = task->datain.data[pos] & 0x20;
	mp->caching.disc = task->datain.data[pos] & 0x10;
	mp->caching.size = task->datain.data[pos] & 0x08;
	mp->caching.wce  = task->datain.data[pos] & 0x04;
	mp->caching.mf   = task->datain.data[pos] & 0x02;
	mp->caching.rcd  = task->datain.data[pos] & 0x01;

	mp->caching.demand_read_retention_priority = (task->datain.data[pos+1] >> 4) & 0x0f;
	mp->caching.write_retention_priority       = task->datain.data[pos+1] & 0x0f;

	mp->caching.disable_prefetch_transfer_length = scsi_get_uint16(&task->datain.data[pos+2]);
	mp->caching.minimum_prefetch = scsi_get_uint16(&task->datain.data[pos+4]);
	mp->caching.maximum_prefetch = scsi_get_uint16(&task->datain.data[pos+6]);
	mp->caching.maximum_prefetch_ceiling = scsi_get_uint16(&task->datain.data[pos+8]);

	mp->caching.fsw    = task->datain.data[pos+10] & 0x80;
	mp->caching.lbcss  = task->datain.data[pos+10] & 0x40;
	mp->caching.dra    = task->datain.data[pos+10] & 0x20;
	mp->caching.nv_dis = task->datain.data[pos+10] & 0x01;

	mp->caching.number_of_cache_segments = task->datain.data[pos+11];
	mp->caching.cache_segment_size = scsi_get_uint16(&task->datain.data[pos+12]);
}

static void
scsi_parse_mode_disconnect_reconnect(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->disconnect_reconnect.buffer_full_ratio = task->datain.data[pos];
	mp->disconnect_reconnect.buffer_empty_ratio = task->datain.data[pos+1];
	mp->disconnect_reconnect.bus_inactivity_limit = scsi_get_uint16(&task->datain.data[pos+2]);
	mp->disconnect_reconnect.disconnect_time_limit = scsi_get_uint16(&task->datain.data[pos+4]);
	mp->disconnect_reconnect.connect_time_limit = scsi_get_uint16(&task->datain.data[pos+6]);
	mp->disconnect_reconnect.maximum_burst_size = scsi_get_uint16(&task->datain.data[pos+8]);
	mp->disconnect_reconnect.emdp = task->datain.data[pos+10] & 0x80;
	mp->disconnect_reconnect.fair_arbitration = (task->datain.data[pos+10]>>4) & 0x0f;
	mp->disconnect_reconnect.dimm = task->datain.data[pos+10] & 0x08;
	mp->disconnect_reconnect.dtdc = task->datain.data[pos+10] & 0x07;
	mp->disconnect_reconnect.first_burst_size = scsi_get_uint16(&task->datain.data[pos+12]);
}

static void
scsi_parse_mode_informational_exceptions_control(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->iec.perf           = task->datain.data[pos] & 0x80;
	mp->iec.ebf            = task->datain.data[pos] & 0x20;
	mp->iec.ewasc          = task->datain.data[pos] & 0x10;
	mp->iec.dexcpt         = task->datain.data[pos] & 0x08;
	mp->iec.test           = task->datain.data[pos] & 0x04;
	mp->iec.ebackerr       = task->datain.data[pos] & 0x02;
	mp->iec.logerr         = task->datain.data[pos] & 0x01;
	mp->iec.mrie           = task->datain.data[pos+1] & 0x0f;
	mp->iec.interval_timer = scsi_get_uint32(&task->datain.data[pos+2]);
	mp->iec.report_count   = scsi_get_uint32(&task->datain.data[pos+6]);
}


/*
 * parse and unmarshall the mode sense data in buffer
 */
static struct scsi_mode_sense *
scsi_modesense_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_mode_sense *ms;
	int pos;

	if (task->datain.size < 4) {
		return NULL;
	}

	ms = scsi_malloc(task, sizeof(struct scsi_mode_sense));
	if (ms == NULL) {
		return NULL;
	}

	ms->mode_data_length          = task->datain.data[0];
	ms->medium_type               = task->datain.data[1];
	ms->device_specific_parameter = task->datain.data[2];
	ms->block_descriptor_length   = task->datain.data[3];
	ms->pages                     = NULL;

	if (ms->mode_data_length + 1 > task->datain.size) {
		return NULL;
	}

	pos = 4 + ms->block_descriptor_length;
	while (pos < task->datain.size) {
		struct scsi_mode_page *mp;

		mp = scsi_malloc(task, sizeof(struct scsi_mode_page));
		if (mp == NULL) {
			return ms;
		}
		mp->ps           = task->datain.data[pos] & 0x80;
		mp->spf          = task->datain.data[pos] & 0x40;
		mp->page_code    = task->datain.data[pos] & 0x3f;
		pos++;

		if (mp->spf) {
			mp->subpage_code = task->datain.data[pos++];
			mp->len = scsi_get_uint16(&task->datain.data[pos]);
			pos += 2;
		} else {
			mp->subpage_code = 0;
			mp->len          = task->datain.data[pos++];
		}

		switch (mp->page_code) {
		case SCSI_MODESENSE_PAGECODE_CACHING:
			scsi_parse_mode_caching(task, pos, mp);
			break;
		case SCSI_MODESENSE_PAGECODE_DISCONNECT_RECONNECT:
			scsi_parse_mode_disconnect_reconnect(task, pos, mp);
			break;
		case SCSI_MODESENSE_PAGECODE_INFORMATIONAL_EXCEPTIONS_CONTROL:
			scsi_parse_mode_informational_exceptions_control(task, pos, mp);
			break;
		default:
			/* TODO: process other pages, or add raw data to struct
			 * scsi_mode_page.  */
			break;
		}

		mp->next  = ms->pages;
		ms->pages = mp;

		pos += mp->len;
	}

	return ms;
}

/*
 * STARTSTOPUNIT
 */
struct scsi_task *
scsi_cdb_startstopunit(int immed, int pcm, int pc, int no_flush, int loej, int start)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_STARTSTOPUNIT;

	if (immed) {
		task->cdb[1] |= 0x01;
	}
	task->cdb[3] |= pcm & 0x0f;
	task->cdb[4] |= (pc << 4) & 0xf0;
	if (no_flush) {
		task->cdb[4] |= 0x04;
	}
	if (loej) {
		task->cdb[4] |= 0x02;
	}
	if (start) {
		task->cdb[4] |= 0x01;
	}


	task->cdb_size   = 6;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}

/*
 * PREVENTALLOWMEDIUMREMOVAL
 */
struct scsi_task *
scsi_cdb_preventallow(int prevent)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_PREVENTALLOW;

	task->cdb[4] = prevent & 0x03;

	task->cdb_size   = 6;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}

/*
 * SYNCHRONIZECACHE10
 */
struct scsi_task *
scsi_cdb_synchronizecache10(int lba, int num_blocks, int syncnv, int immed)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_SYNCHRONIZECACHE10;

	if (syncnv) {
		task->cdb[1] |= 0x04;
	}
	if (immed) {
		task->cdb[1] |= 0x02;
	}
	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint16(&task->cdb[7], num_blocks);

	task->cdb_size   = 10;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}

/*
 * SYNCHRONIZECACHE16
 */
struct scsi_task *
scsi_cdb_synchronizecache16(uint64_t lba, uint32_t num_blocks, int syncnv, int immed)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_SYNCHRONIZECACHE16;

	if (syncnv) {
		task->cdb[1] |= 0x04;
	}
	if (immed) {
		task->cdb[1] |= 0x02;
	}
	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], num_blocks);

	task->cdb_size   = 16;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}

/*
 * PREFETCH10
 */
struct scsi_task *
scsi_cdb_prefetch10(uint32_t lba, int num_blocks, int immed, int group)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_PREFETCH10;

	if (immed) {
		task->cdb[1] |= 0x02;
	}
	scsi_set_uint32(&task->cdb[2], lba);
	task->cdb[6] |= group & 0x1f;
	scsi_set_uint16(&task->cdb[7], num_blocks);

	task->cdb_size   = 10;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}

/*
 * PREFETCH16
 */
struct scsi_task *
scsi_cdb_prefetch16(uint64_t lba, int num_blocks, int immed, int group)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_PREFETCH16;

	if (immed) {
		task->cdb[1] |= 0x02;
	}
	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], num_blocks);

	task->cdb[14] |= group & 0x1f;

	task->cdb_size   = 16;
	task->xfer_dir   = SCSI_XFER_NONE;
	task->expxferlen = 0;

	return task;
}

/*
 * SERVICEACTIONIN16
 */
struct scsi_task *
scsi_cdb_serviceactionin16(enum scsi_service_action_in sa, uint32_t xferlen)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_SERVICE_ACTION_IN;

	task->cdb[1] = sa;

	scsi_set_uint32(&task->cdb[10], xferlen);

	task->cdb_size   = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * READCAPACITY16
 */
struct scsi_task *
scsi_cdb_readcapacity16(void)
{
	return scsi_cdb_serviceactionin16(SCSI_READCAPACITY16, 32);
}

/*
 * GET_LBA_STATUS
 */
struct scsi_task *
scsi_cdb_get_lba_status(uint64_t starting_lba, uint32_t alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_SERVICE_ACTION_IN;

	task->cdb[1] = SCSI_GET_LBA_STATUS;

	scsi_set_uint32(&task->cdb[2], starting_lba >> 32);
	scsi_set_uint32(&task->cdb[6], starting_lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], alloc_len);

	task->cdb_size   = 16;
	if (alloc_len != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = alloc_len;

	return task;
}

/*
 * WRITEVERIFY10
 */
struct scsi_task *
scsi_cdb_writeverify10(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int bytchk, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE_VERIFY10;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (bytchk) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint16(&task->cdb[7], xferlen/blocksize);

	task->cdb[6] |= (group_number & 0x1f);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITEVERIFY12
 */
struct scsi_task *
scsi_cdb_writeverify12(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int bytchk, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE_VERIFY12;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (bytchk) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba);
	scsi_set_uint32(&task->cdb[6], xferlen/blocksize);

	task->cdb[10] |= (group_number & 0x1f);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

/*
 * WRITEVERIFY16
 */
struct scsi_task *
scsi_cdb_writeverify16(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int bytchk, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE_VERIFY16;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (bytchk) {
		task->cdb[1] |= 0x02;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	return task;
}

int
scsi_datain_getfullsize(struct scsi_task *task)
{
	switch (task->cdb[0]) {
	case SCSI_OPCODE_TESTUNITREADY:
		return 0;
	case SCSI_OPCODE_INQUIRY:
		return scsi_inquiry_datain_getfullsize(task);
	case SCSI_OPCODE_MODESENSE6:
		return scsi_modesense6_datain_getfullsize(task);
	case SCSI_OPCODE_READCAPACITY10:
		return scsi_readcapacity10_datain_getfullsize(task);
	case SCSI_OPCODE_SYNCHRONIZECACHE10:
		return 0;
	case SCSI_OPCODE_READTOC:
		return scsi_readtoc_datain_getfullsize(task);
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_getfullsize(task);
	case SCSI_OPCODE_MAINTENANCE_IN:
		return scsi_maintenancein_datain_getfullsize(task);
	}
	return -1;
}

void *
scsi_datain_unmarshall(struct scsi_task *task)
{
	switch (task->cdb[0]) {
	case SCSI_OPCODE_TESTUNITREADY:
		return NULL;
	case SCSI_OPCODE_INQUIRY:
		return scsi_inquiry_datain_unmarshall(task);
	case SCSI_OPCODE_MODESENSE6:
		return scsi_modesense_datain_unmarshall(task);
	case SCSI_OPCODE_READCAPACITY10:
		return scsi_readcapacity10_datain_unmarshall(task);
	case SCSI_OPCODE_SYNCHRONIZECACHE10:
		return NULL;
	case SCSI_OPCODE_READTOC:
		return scsi_readtoc_datain_unmarshall(task);
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_unmarshall(task);
	case SCSI_OPCODE_SERVICE_ACTION_IN:
		return scsi_serviceactionin_datain_unmarshall(task);
	case SCSI_OPCODE_MAINTENANCE_IN:
		return scsi_maintenancein_datain_unmarshall(task);
	}
	return NULL;
}


static struct scsi_read10_cdb *
scsi_read10_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_read10_cdb *read10;

	read10 = scsi_malloc(task, sizeof(struct scsi_read10_cdb));
	if (read10 == NULL) {
		return NULL;
	}

	read10->opcode          = SCSI_OPCODE_READ10;
	read10->rdprotect       = (task->cdb[1] >> 5) & 0x7;
	read10->dpo             = !!(task->cdb[1] & 0x10);
	read10->fua             = !!(task->cdb[1] & 0x08);
	read10->fua_nv          = !!(task->cdb[1] & 0x02);
	read10->lba             = scsi_get_uint32(&task->cdb[2]);
	read10->group           = task->cdb[6] & 0x1f;
	read10->transfer_length = scsi_get_uint16(&task->cdb[7]);
	read10->control         = task->cdb[9];

        return read10;
}

void *
scsi_cdb_unmarshall(struct scsi_task *task, enum scsi_opcode opcode)
{
	if (task->cdb[0] != opcode) {
		return NULL;
	}

	switch (task->cdb[0]) {
	case SCSI_OPCODE_READ10:
		return scsi_read10_cdb_unmarshall(task);
	}
	return NULL;
}

const char *
scsi_devtype_to_str(enum scsi_inquiry_peripheral_device_type type)
{
	switch (type) {
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS:
		return "DIRECT_ACCESS";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQUENTIAL_ACCESS:
		return "SEQUENTIAL_ACCESS";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_PRINTER:
		return "PRINTER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_PROCESSOR:
		return "PROCESSOR";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_WRITE_ONCE:
		return "WRITE_ONCE";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MMC:
		return "MMC";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SCANNER:
		return "SCANNER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OPTICAL_MEMORY:
		return "OPTICAL_MEMORY";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MEDIA_CHANGER:
		return "MEDIA_CHANGER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_COMMUNICATIONS:
		return "COMMUNICATIONS";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_STORAGE_ARRAY_CONTROLLER:
		return "STORAGE_ARRAY_CONTROLLER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_ENCLOSURE_SERVICES:
		return "ENCLOSURE_SERVICES";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SIMPLIFIED_DIRECT_ACCESS:
		return "SIMPLIFIED_DIRECT_ACCESS";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OPTICAL_CARD_READER:
		return "OPTICAL_CARD_READER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_BRIDGE_CONTROLLER:
		return "BRIDGE_CONTROLLER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OSD:
		return "OSD";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_AUTOMATION:
		return "AUTOMATION";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQURITY_MANAGER:
		return "SEQURITY_MANAGER";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_WELL_KNOWN_LUN:
		return "WELL_KNOWN_LUN";
	case SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_UNKNOWN:
		return "UNKNOWN";
	}
	return "unknown";
}

const char *
scsi_devqualifier_to_str(enum scsi_inquiry_peripheral_qualifier qualifier)
{
	switch (qualifier) {
	case SCSI_INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED:
		return "CONNECTED";
	case SCSI_INQUIRY_PERIPHERAL_QUALIFIER_DISCONNECTED:
		return "DISCONNECTED";
	case SCSI_INQUIRY_PERIPHERAL_QUALIFIER_NOT_SUPPORTED:
		return "NOT_SUPPORTED";
	}
	return "unknown";
}

const char *
scsi_version_to_str(enum scsi_version version)
{
	switch (version) {
	case SCSI_VERSION_SPC:
		return "ANSI INCITS 301-1997 (SPC)";
	case SCSI_VERSION_SPC2:
		return "ANSI INCITS 351-2001 (SPC-2)";
	case SCSI_VERSION_SPC3:
		return "ANSI INCITS 408-2005 (SPC-3)";
	}
	return "unknown";
}


const char *
scsi_inquiry_pagecode_to_str(int pagecode)
{
	switch (pagecode) {
	case SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES:
	     return "SUPPORTED_VPD_PAGES";
	case SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER:
		return "UNIT_SERIAL_NUMBER";
	case SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION:
		return "DEVICE_IDENTIFICATION";
	case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
		return "BLOCK_DEVICE_CHARACTERISTICS";
	}
	return "unknown";
}


const char *
scsi_protocol_identifier_to_str(int identifier)
{
	switch (identifier) {
	case SCSI_PROTOCOL_IDENTIFIER_FIBRE_CHANNEL:
	     return "FIBRE_CHANNEL";
	case SCSI_PROTOCOL_IDENTIFIER_PARALLEL_SCSI:
	     return "PARALLEL_SCSI";
	case SCSI_PROTOCOL_IDENTIFIER_SSA:
		return "SSA";
	case SCSI_PROTOCOL_IDENTIFIER_IEEE_1394:
		return "IEEE_1394";
	case SCSI_PROTOCOL_IDENTIFIER_RDMA:
		return "RDMA";
	case SCSI_PROTOCOL_IDENTIFIER_ISCSI:
		return "ISCSI";
	case SCSI_PROTOCOL_IDENTIFIER_SAS:
		return "SAS";
	case SCSI_PROTOCOL_IDENTIFIER_ADT:
		return "ADT";
	case SCSI_PROTOCOL_IDENTIFIER_ATA:
		return "ATA";
	}
	return "unknown";
}

const char *
scsi_codeset_to_str(int codeset)
{
	switch (codeset) {
	case SCSI_CODESET_BINARY:
		return "BINARY";
	case SCSI_CODESET_ASCII:
		return "ASCII";
	case SCSI_CODESET_UTF8:
		return "UTF8";
	}
	return "unknown";
}

const char *
scsi_association_to_str(int association)
{
	switch (association) {
	case SCSI_ASSOCIATION_LOGICAL_UNIT:
		return "LOGICAL_UNIT";
	case SCSI_ASSOCIATION_TARGET_PORT:
		return "TARGET_PORT";
	case SCSI_ASSOCIATION_TARGET_DEVICE:
		return "TARGET_DEVICE";
	}
	return "unknown";
}

const char *
scsi_designator_type_to_str(int type)
{
	switch (type) {
	case SCSI_DESIGNATOR_TYPE_VENDOR_SPECIFIC:
		return "VENDOR_SPECIFIC";
	case SCSI_DESIGNATOR_TYPE_T10_VENDORT_ID:
		return "T10_VENDORT_ID";
	case SCSI_DESIGNATOR_TYPE_EUI_64:
		return "EUI_64";
	case SCSI_DESIGNATOR_TYPE_NAA:
		return "NAA";
	case SCSI_DESIGNATOR_TYPE_RELATIVE_TARGET_PORT:
		return "RELATIVE_TARGET_PORT";
	case SCSI_DESIGNATOR_TYPE_TARGET_PORT_GROUP:
		return "TARGET_PORT_GROUP";
	case SCSI_DESIGNATOR_TYPE_LOGICAL_UNIT_GROUP:
		return "LOGICAL_UNIT_GROUP";
	case SCSI_DESIGNATOR_TYPE_MD5_LOGICAL_UNIT_IDENTIFIER:
		return "MD5_LOGICAL_UNIT_IDENTIFIER";
	case SCSI_DESIGNATOR_TYPE_SCSI_NAME_STRING:
		return "SCSI_NAME_STRING";
	}
	return "unknown";
}

void
scsi_set_task_private_ptr(struct scsi_task *task, void *ptr)
{
	task->ptr = ptr;
}

void *
scsi_get_task_private_ptr(struct scsi_task *task)
{
	return task->ptr;
}

void
scsi_task_set_iov_out(struct scsi_task *task, struct scsi_iovec *iov, int niov) 
{
	task->iovector_out.iov = iov;
	task->iovector_out.niov = niov;
}

void
scsi_task_set_iov_in(struct scsi_task *task, struct scsi_iovec *iov, int niov) 
{
	task->iovector_in.iov = iov;
	task->iovector_in.niov = niov;
}

#define IOVECTOR_INITAL_ALLOC (16)

static int
scsi_iovector_add(struct scsi_task *task, struct scsi_iovector *iovector, int len, unsigned char *buf)
{
	if (len < 0) {
		return -1;
	}
	
	if (iovector->iov == NULL) {
		iovector->iov = scsi_malloc(task, IOVECTOR_INITAL_ALLOC*sizeof(struct iovec));
		if (iovector->iov == NULL) {
			return -1;
		}
		iovector->nalloc = IOVECTOR_INITAL_ALLOC;
	}

	/* iovec allocation is too small */
	if (iovector->nalloc < iovector->niov + 1) {
		struct scsi_iovec *old_iov = iovector->iov;
		iovector->iov = scsi_malloc(task, 2 * iovector->nalloc * sizeof(struct iovec));
		if (iovector->iov == NULL) {
			return -1;
		}
		memcpy(iovector->iov, old_iov, iovector->niov * sizeof(struct iovec));
		iovector->nalloc <<= 1;
	}

	iovector->iov[iovector->niov].iov_len = len;
	iovector->iov[iovector->niov].iov_base = buf;
	iovector->niov++;

	return 0;
}

int
scsi_task_add_data_in_buffer(struct scsi_task *task, int len, unsigned char *buf)
{
	return scsi_iovector_add(task, &task->iovector_in, len, buf);
}

int
scsi_task_add_data_out_buffer(struct scsi_task *task, int len, unsigned char *buf)
{
	return scsi_iovector_add(task, &task->iovector_out, len, buf);
}
