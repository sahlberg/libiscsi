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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef AROS
#include "aros/aros_compat.h"
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include "win32/win32_compat.h"
#else
#include <strings.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "slist.h"
#include "scsi-lowlevel.h"

void scsi_task_set_iov_out(struct scsi_task *task, struct scsi_iovec *iov, int niov);

struct scsi_allocated_memory {
	struct scsi_allocated_memory *next;
	char buf[0];
};

void
scsi_free_scsi_task(struct scsi_task *task)
{
	struct scsi_allocated_memory *mem;

	if (!task)
		return;

	while ((mem = task->mem)) {
		   ISCSI_LIST_REMOVE(&task->mem, mem);
		   free(mem);
	}

	free(task->datain.data);
	free(task);
}

struct scsi_task *
scsi_create_task(int cdb_size, unsigned char *cdb, int xfer_dir, int expxferlen)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));

	memcpy(&task->cdb[0], cdb, cdb_size);
	task->cdb_size   = cdb_size;
	task->xfer_dir   = xfer_dir;
	task->expxferlen = expxferlen;

	return task;
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
	ISCSI_LIST_ADD(&task->mem, mem);
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
		{SCSI_SENSE_ASCQ_SANITIZE_IN_PROGRESS,
		 "SANITIZE_IN_PROGRESS"},
		{SCSI_SENSE_ASCQ_WRITE_AFTER_SANITIZE_REQUIRED,
		 "WRITE_AFTER_SANITIZE_REQUIRED"},
		{SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE,
		 "INVALID_OPERATION_CODE"},
		{SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE,
		 "LBA_OUT_OF_RANGE"},
		{SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB,
		 "INVALID_FIELD_IN_CDB"},
		{SCSI_SENSE_ASCQ_LOGICAL_UNIT_NOT_SUPPORTED,
		 "LOGICAL_UNIT_NOT_SUPPORTED"},
		{SCSI_SENSE_ASCQ_PARAMETER_LIST_LENGTH_ERROR,
		 "PARAMETER_LIST_LENGTH_ERROR"},
		{SCSI_SENSE_ASCQ_INVALID_FIELD_IN_PARAMETER_LIST,
		 "INVALID_FIELD_IN_PARAMETER_LIST"},
		{SCSI_SENSE_ASCQ_WRITE_PROTECTED,
		 "WRITE_PROTECTED"},
		{SCSI_SENSE_ASCQ_WRITE_PROTECTED,
		 "WRITE_PROTECTED"},
		{SCSI_SENSE_ASCQ_HARDWARE_WRITE_PROTECTED,
		 "HARDWARE_WRITE_PROTECTED"},
		{SCSI_SENSE_ASCQ_SOFTWARE_WRITE_PROTECTED,
		 "SOFTWARE_WRITE_PROTECTED"},
		{SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT,
		 "MEDIUM_NOT_PRESENT"},
		{SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED,
		 "MEDIUM_NOT_PRESENT-TRAY_CLOSED"},
		{SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN,
		 "MEDIUM_NOT_PRESENT-TRAY_OPEN"},
		{SCSI_SENSE_ASCQ_BUS_RESET,
		 "BUS_RESET"},
		{SCSI_SENSE_ASCQ_POWER_ON_OCCURED,
		 "POWER_ON_OCCURED"},
		{SCSI_SENSE_ASCQ_SCSI_BUS_RESET_OCCURED,
		 "SCSI_BUS_RESET_OCCURED"},
		{SCSI_SENSE_ASCQ_BUS_DEVICE_RESET_FUNCTION_OCCURED,
		 "BUS_DEVICE_RESET_FUNCTION_OCCURED"},
		{SCSI_SENSE_ASCQ_DEVICE_INTERNAL_RESET,
		 "DEVICE_INTERNAL_RESET"},
		{SCSI_SENSE_ASCQ_TRANSCEIVER_MODE_CHANGED_TO_SINGLE_ENDED,
		 "TRANSCEIVER_MODE_CHANGED_TO_SINGLE_ENDED"},
		{SCSI_SENSE_ASCQ_TRANSCEIVER_MODE_CHANGED_TO_LVD,
		 "TRANSCEIVER_MODE_CHANGED_TO_LVD"},
		{SCSI_SENSE_ASCQ_MODE_PARAMETERS_CHANGED,
		 "MODE PARAMETERS CHANGED"},
		{SCSI_SENSE_ASCQ_CAPACITY_DATA_HAS_CHANGED,
		 "CAPACITY_DATA_HAS_CHANGED"},
		{SCSI_SENSE_ASCQ_THIN_PROVISION_SOFT_THRES_REACHED,
		 "THIN PROVISIONING SOFT THRESHOLD REACHED"},
		{SCSI_SENSE_ASCQ_INQUIRY_DATA_HAS_CHANGED,
		 "INQUIRY DATA HAS CHANGED"},
		{SCSI_SENSE_ASCQ_INTERNAL_TARGET_FAILURE,
		 "INTERNAL_TARGET_FAILURE"},
		{SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY,
		 "MISCOMPARE_DURING_VERIFY"},
		{SCSI_SENSE_ASCQ_MISCOMPARE_VERIFY_OF_UNMAPPED_LBA,
		 "MISCOMPARE_VERIFY_OF_UNMAPPED_LBA"},
		{ SCSI_SENSE_ASCQ_MEDIUM_LOAD_OR_EJECT_FAILED,
		  "MEDIUM_LOAD_OR_EJECT_FAILED" },
		{SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED,
		 "SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED"},
	       {0, NULL}
	};

	return value_string_find(ascqs, ascq);
}

const char *
scsi_pr_type_str(enum scsi_persistent_out_type pr_type)
{
	struct value_string pr_type_strings[] = {
		{SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE,
		 "Write Exclusive"},
		{SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS,
		 "Exclusive Access"},
		{SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_REGISTRANTS_ONLY,
		 "Write Exclusive, Registrants Only"},
		{SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_REGISTRANTS_ONLY,
		 "Exclusive Access Registrants Only"},
		{SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS,
		 "Write Exclusive, All Registrants"},
		{SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS,
		 "Exclusive Access, All Registrants"},
		{0, NULL}
	};

	return value_string_find(pr_type_strings, pr_type);
}

uint64_t
scsi_get_uint64(const unsigned char *c)
{
	uint64_t val;

	val = scsi_get_uint32(c);
	val <<= 32;
	c += 4;
	val |= scsi_get_uint32(c);

	return val;
}

uint32_t
scsi_get_uint32(const unsigned char *c)
{
	uint32_t val;
	val = c[0];
	val = (val << 8) | c[1];
	val = (val << 8) | c[2];
	val = (val << 8) | c[3];
	return val;
}

uint16_t
scsi_get_uint16(const unsigned char *c)
{
	uint16_t val;
	val = c[0];
	val = (val << 8) | c[1];
	return val;
}

static inline uint64_t
task_get_uint64(struct scsi_task *task, int offset)
{
	if (offset <= task->datain.size - 8) {
		const unsigned char *c = &task->datain.data[offset];

		return scsi_get_uint64(c);
	} else {
		return 0;
	}
}

static inline uint32_t
task_get_uint32(struct scsi_task *task, int offset)
{
	if (offset <= task->datain.size - 4) {
		const unsigned char *c = &task->datain.data[offset];

		return scsi_get_uint32(c);
	} else {
		return 0;
	}
}

static inline uint16_t
task_get_uint16(struct scsi_task *task, int offset)
{
	if (offset <= task->datain.size - 2) {
		const unsigned char *c = &task->datain.data[offset];

		return scsi_get_uint16(c);
	} else {
		return 0;
	}
}

static inline uint8_t
task_get_uint8(struct scsi_task *task, int offset)
{
	if (offset <= task->datain.size - 1) {
		return task->datain.data[offset];
	} else {
		return 0;
	}
}

void
scsi_set_uint64(unsigned char *c, uint64_t v)
{
	uint32_t val;

	val = (v >> 32) & 0xffffffff;
	scsi_set_uint32(c, val);

	c += 4;
	val = v & 0xffffffff;
	scsi_set_uint32(c, val);
}

void
scsi_set_uint32(unsigned char *c, uint32_t val)
{
	c[0] = val >> 24;
	c[1] = val >> 16;
	c[2] = val >> 8;
	c[3] = val;
}

void
scsi_set_uint16(unsigned char *c, uint16_t val)
{
	c[0] = val >> 8;
	c[1] = val;
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
 * SANITIZE
 */
struct scsi_task *
scsi_cdb_sanitize(int immed, int ause, int sa, int param_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_SANITIZE;

	task->cdb[1] = sa & 0x1f;
	if (immed) {
		task->cdb[1] |= 0x80;
	}
	if (ause) {
		task->cdb[1] |= 0x20;
	}

	scsi_set_uint16(&task->cdb[7], param_len);

	task->cdb_size   = 10;
	if (param_len != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = (param_len + 3) & 0xfffc;

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

	list_size = task_get_uint32(task, 0) + 8;

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

	list_size = task_get_uint32(task, 0) + 8;
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
		list->luns[i] = task_get_uint16(task, i * 8 + 8);
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
 * READDEFECTDATA10
 */
struct scsi_task *
scsi_cdb_readdefectdata10(int req_plist, int req_glist, int defect_list_format,
                          uint16_t alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ_DEFECT_DATA10;

        if (req_plist) {
                task->cdb[2] |= 0x10;
	}
        if (req_glist) {
                task->cdb[2] |= 0x08;
	}
        task->cdb[2] |= (defect_list_format & 0x07);

	scsi_set_uint16(&task->cdb[7], alloc_len);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	return task;
}

/*
 * READDEFECTDATA12
 */
struct scsi_task *
scsi_cdb_readdefectdata12(int req_plist, int req_glist, int defect_list_format,
                          uint32_t address_descriptor_index, uint32_t alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_READ_DEFECT_DATA12;

        if (req_plist) {
                task->cdb[2] |= 0x10;
	}
        if (req_glist) {
                task->cdb[2] |= 0x08;
	}
        task->cdb[2] |= (defect_list_format & 0x07);

	scsi_set_uint32(&task->cdb[2], address_descriptor_index);
	scsi_set_uint32(&task->cdb[6], alloc_len);

	task->cdb_size = 12;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

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

	toc_data_len = task_get_uint16(task, 0) + 2;

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
			= task_get_uint8(task, 4 + 8 * i + 1) & 0xf0;
		list->desc[i].desc.toc.control
			= task_get_uint8(task, 4 + 8 * i + 1) & 0x0f;
		list->desc[i].desc.toc.track
			= task_get_uint8(task, 4 + 8 * i + 2);
		list->desc[i].desc.toc.lba
			= task_get_uint32(task, 4 + 8 * i + 4);
		break;
	case SCSI_READ_SESSION_INFO:
		list->desc[i].desc.ses.adr
			= task_get_uint8(task, 4 + 8 * i + 1) & 0xf0;
		list->desc[i].desc.ses.control
			= task_get_uint8(task, 4 + 8 * i + 1) & 0x0f;
		list->desc[i].desc.ses.first_in_last
			= task_get_uint8(task, 4 + 8 * i + 2);
		list->desc[i].desc.ses.lba
			= task_get_uint32(task, 4 + 8 * i + 4);
		break;
	case SCSI_READ_FULL_TOC:
		list->desc[i].desc.full.session
			= task_get_uint8(task, 4 + 11 * i + 0) & 0xf0;
		list->desc[i].desc.full.adr
			= task_get_uint8(task, 4 + 11 * i + 1) & 0xf0;
		list->desc[i].desc.full.control
			= task_get_uint8(task, 4 + 11 * i + 1) & 0x0f;
		list->desc[i].desc.full.tno
			= task_get_uint8(task, 4 + 11 * i + 2);
		list->desc[i].desc.full.point
			= task_get_uint8(task, 4 + 11 * i + 3);
		list->desc[i].desc.full.min
			= task_get_uint8(task, 4 + 11 * i + 4);
		list->desc[i].desc.full.sec
			= task_get_uint8(task, 4 + 11 * i + 5);
		list->desc[i].desc.full.frame
			= task_get_uint8(task, 4 + 11 * i + 6);
		list->desc[i].desc.full.zero
			= task_get_uint8(task, 4 + 11 * i + 7);
		list->desc[i].desc.full.pmin
			= task_get_uint8(task, 4 + 11 * i + 8);
		list->desc[i].desc.full.psec
			= task_get_uint8(task, 4 + 11 * i + 9);
		list->desc[i].desc.full.pframe
			= task_get_uint8(task, 4 + 11 * i + 10);
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
	list->first = task_get_uint8(task, 2);
	list->last = task_get_uint8(task, 3);

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
		rc16->returned_lba = task_get_uint32(task, 0);
		rc16->returned_lba = (rc16->returned_lba << 32) | task_get_uint32(task, 4);
		rc16->block_length = task_get_uint32(task, 8);
		rc16->p_type       = (task_get_uint8(task, 12) >> 1) & 0x07;
		rc16->prot_en      = task_get_uint8(task, 12) & 0x01;
		rc16->p_i_exp      = (task_get_uint8(task, 13) >> 4) & 0x0f;
		rc16->lbppbe       = task_get_uint8(task, 13) & 0x0f;
		rc16->lbpme        = !!(task_get_uint8(task, 14) & 0x80);
		rc16->lbprz        = !!(task_get_uint8(task, 14) & 0x40);
		rc16->lalba        = task_get_uint16(task, 14) & 0x3fff;
		return rc16;
	}
	case SCSI_GET_LBA_STATUS: {
		struct scsi_get_lba_status *gls = scsi_malloc(task,
							      sizeof(*gls));
		int32_t len = task_get_uint32(task, 0);
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
			return NULL;
		}

		for (i = 0; i < (int)gls->num_descriptors; i++) {
			gls->descriptors[i].lba  = task_get_uint32(task, 8 + i * sizeof(struct scsi_lba_status_descriptor) + 0);
			gls->descriptors[i].lba <<= 32;
 			gls->descriptors[i].lba |= task_get_uint32(task, 8 + i * sizeof(struct scsi_lba_status_descriptor) + 4);

			gls->descriptors[i].num_blocks = task_get_uint32(task, 8 + i * sizeof(struct scsi_lba_status_descriptor) + 8);

			gls->descriptors[i].provisioning = task_get_uint8(task, 8 + i * sizeof(struct scsi_lba_status_descriptor) + 12) & 0x0f;
		}

		return gls;
	}
	default:
		return NULL;
	}
}

/*
 * persistent_reserve_in unmarshall
 */
static inline uint8_t
scsi_persistentreservein_sa(const struct scsi_task *task)
{
	return task->cdb[1] & 0x1f;
}

static int
scsi_persistentreservein_datain_getfullsize(struct scsi_task *task)
{
	switch (scsi_persistentreservein_sa(task)) {
	case SCSI_PERSISTENT_RESERVE_READ_KEYS:
		return task_get_uint32(task, 4) + 8;
	case SCSI_PERSISTENT_RESERVE_READ_RESERVATION:
		return 8;
	case SCSI_PERSISTENT_RESERVE_REPORT_CAPABILITIES:
		return 8;
	default:
		return -1;
	}
}

static void *
scsi_receivecopyresults_datain_unmarshall(struct scsi_task *task)
{
	int sa = task->cdb[1] & 0x1f;
	int len, i;
	struct scsi_copy_results_copy_status *cs;
	struct scsi_copy_results_op_params *op;

	switch (sa) {
	case SCSI_COPY_RESULTS_COPY_STATUS:
		len = task_get_uint32(task, 0);
		if (len < 8)
			return NULL;
		cs = scsi_malloc(task, sizeof(*cs));
		if (cs == NULL) {
			return NULL;
		}
		cs->available_data = len;
		cs->copy_manager_status = task_get_uint8(task, 4) & 0x7F;
		cs->hdd = (task_get_uint8(task, 4) & 0x80) >> 7;
		cs->segments_processed = task_get_uint16(task, 5);
		cs->transfer_count_units = task_get_uint8(task, 7);
		cs->transfer_count = task_get_uint32(task, 8);
		return cs;

	case SCSI_COPY_RESULTS_OP_PARAMS:
		len = task_get_uint32(task, 0);
		if (len < 40)
			return NULL;
		op = scsi_malloc(task, sizeof(*op) + task_get_uint8(task, 43));
		if (op == NULL) {
			return NULL;
		}
		op->available_data = len;
		op->max_target_desc_count = task_get_uint16(task, 8);
		op->max_segment_desc_count = task_get_uint16(task, 10);
		op->max_desc_list_length = task_get_uint32(task, 12);
		op->max_segment_length = task_get_uint32(task, 16);
		op->max_inline_data_length = task_get_uint32(task, 20);
		op->held_data_limit = task_get_uint32(task, 24);
		op->max_stream_device_transfer_size = task_get_uint32(task, 28);
		op->total_concurrent_copies = task_get_uint16(task, 34);
		op->max_concurrent_copies = task_get_uint8(task, 36);
		op->data_segment_granularity = task_get_uint8(task, 37);
		op->inline_data_granularity = task_get_uint8(task, 38);
		op->held_data_granularity = task_get_uint8(task, 39);
		op->impl_desc_list_length = task_get_uint8(task, 43);
                for (i = 0; i < (int)op->impl_desc_list_length; i++) {
                        op->imp_desc_type_codes[i] = task_get_uint8(task, 44+i);
                }
		return op;
	default:
		return NULL;
	}
}

#ifndef MIN /* instead of including all of iscsi-private.h */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
static void *
scsi_persistentreservein_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_persistent_reserve_in_read_keys *rk;
	struct scsi_persistent_reserve_in_read_reservation *rr;
	struct scsi_persistent_reserve_in_report_capabilities *rc;
	int i;

	switch (scsi_persistentreservein_sa(task)) {
	case SCSI_PERSISTENT_RESERVE_READ_KEYS: {
		uint32_t cdb_keys_len;
		uint32_t data_keys_len;
		uint32_t keys_len;

		if (task->datain.size < 8) {
			return NULL;
		}

		/*
		 * SPC5r17: 6.16.2 READ KEYS service action
		 * The ADDITIONAL LENGTH field indicates the number of bytes in
		 * the Reservation key list. The contents of the ADDITIONAL
		 * LENGTH field are not altered based on the allocation length.
		 */
		cdb_keys_len = task_get_uint32(task, 4);
		data_keys_len = task->datain.size - 8;
		/*
		 * Only process as many keys as permitted by the given
		 * ADDITIONAL LENGTH and data-in size limits.
		 */
		keys_len = MIN(cdb_keys_len, data_keys_len);

		rk = scsi_malloc(task,
			offsetof(struct scsi_persistent_reserve_in_read_keys,
				 keys) + keys_len);
		if (rk == NULL) {
			return NULL;
		}
		rk->prgeneration      = task_get_uint32(task, 0);
		rk->additional_length = cdb_keys_len;

		rk->num_keys = keys_len / 8;
		for (i = 0; i < (int)rk->num_keys; i++) {
			rk->keys[i] = task_get_uint64(task, 8 + i * 8);
		}
		return rk;
	}
	case SCSI_PERSISTENT_RESERVE_READ_RESERVATION: {
		size_t	alloc_sz;

		i = task_get_uint32(task, 4);
		alloc_sz = sizeof(struct scsi_persistent_reserve_in_read_reservation);

		rr = scsi_malloc(task, alloc_sz);
		if (rr == NULL) {
			return NULL;
		}
		memset(rr, 0, alloc_sz);
		rr->prgeneration = task_get_uint32(task, 0);

		if (i > 0) {
			rr->reserved = 1;
			rr->reservation_key =
				task_get_uint64(task, 8);
			rr->pr_scope = task_get_uint8(task, 21) >> 4;
			rr->pr_type = task_get_uint8(task, 21) & 0xf;
		}

		return rr;
	}
	case SCSI_PERSISTENT_RESERVE_REPORT_CAPABILITIES:
		rc = scsi_malloc(task, sizeof(struct scsi_persistent_reserve_in_report_capabilities));
		if (rc == NULL) {
			return NULL;
		}
		rc->length         = task_get_uint16(task, 0);
		rc->crh            = !!(task_get_uint8(task, 2) & 0x10);
		rc->sip_c          = !!(task_get_uint8(task, 2) & 0x08);
		rc->atp_c          = !!(task_get_uint8(task, 2) & 0x04);
		rc->ptpl_c         = !!(task_get_uint8(task, 2) & 0x01);
		rc->tmv            = !!(task_get_uint8(task, 3) & 0x80);
		rc->allow_commands = (task_get_uint8(task, 3) & 0x70) >> 4;
		rc->persistent_reservation_type_mask = task_get_uint16(task, 4);

		return rc;
	default:
		return NULL;
	}
}

static inline uint8_t
scsi_maintenancein_sa(const struct scsi_task *task)
{
	return task->cdb[1];
}

static inline uint8_t
scsi_report_supported_opcodes_options(const struct scsi_task *task)
{
	return task->cdb[2] & 0x07;
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
		switch (scsi_report_supported_opcodes_options(task)) {
		case SCSI_REPORT_SUPPORTING_OPS_ALL:
			return task_get_uint32(task, 0) + 4;
		case SCSI_REPORT_SUPPORTING_OPCODE:
		case SCSI_REPORT_SUPPORTING_SERVICEACTION:
			return 4 +
				(task_get_uint8(task, 1) & 0x80) ? 12 : 0 +
				task_get_uint16(task, 2);
		}
		return -1;
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
	struct scsi_report_supported_op_codes_one_command *rsoc_one;
	int len, i;

	switch (scsi_maintenancein_sa(task)) {
	case SCSI_REPORT_SUPPORTED_OP_CODES:
		switch (scsi_report_supported_opcodes_options(task)) {
		case SCSI_REPORT_SUPPORTING_OPS_ALL:
			if (task->datain.size < 4) {
				return NULL;
			}

			len = task_get_uint32(task, 0);
			/* len / 8 is not always correct since if CTDP==1 then
			 * the descriptor is 20 bytes in size intead of 8.
			 * It doesnt matter here though since it just means
			 * we would allocate more descriptors at the end of
			 * the structure than we strictly need. This avoids
			 * having to traverse the datain buffer twice.
			 */
			rsoc = scsi_malloc(task,
				offsetof(struct scsi_report_supported_op_codes,
					descriptors) +
				len / 8 * sizeof(struct scsi_command_descriptor));
			if (rsoc == NULL) {
				return NULL;
			}

			rsoc->num_descriptors = 0;
			i = 4;
			while (len >= 8) {
				struct scsi_command_descriptor *desc;
			
				desc = &rsoc->descriptors[rsoc->num_descriptors++];
				desc->opcode =
					task_get_uint8(task, i);
				desc->sa =
					task_get_uint16(task, i + 2);
				desc->ctdp =
					!!(task_get_uint8(task, i + 5) & 0x02);
				desc->servactv =
					!!(task_get_uint8(task, i + 5) & 0x01);
				desc->cdb_len =
					task_get_uint16(task, i + 6);

				len -= 8;
				i += 8;

				/* No tiemout description */
				if (!desc->ctdp) {
					continue;
				}

				desc->to.descriptor_length =
					task_get_uint16(task, i);
				desc->to.command_specific =
					task_get_uint8(task, i + 3);
				desc->to.nominal_processing_timeout =
					task_get_uint32(task, i + 4);
				desc->to.recommended_timeout =
					task_get_uint32(task, i + 8);

				len -= desc->to.descriptor_length + 2;
				i += desc->to.descriptor_length + 2;
			}
			return rsoc;
		case SCSI_REPORT_SUPPORTING_OPCODE:
		case SCSI_REPORT_SUPPORTING_SERVICEACTION:
			rsoc_one = scsi_malloc(task, sizeof(struct scsi_report_supported_op_codes_one_command));
			if (rsoc_one == NULL) {
				return NULL;
			}

			rsoc_one->ctdp =
				!!(task_get_uint8(task, 1) & 0x80);
			rsoc_one->support =
				task_get_uint8(task, 1) & 0x07;
			rsoc_one->cdb_length =
				task_get_uint16(task, 2);
			if (rsoc_one->cdb_length <=
			    sizeof(rsoc_one->cdb_usage_data)) {
				memcpy(rsoc_one->cdb_usage_data,
					&task->datain.data[4],
					rsoc_one->cdb_length);
			}

			if (rsoc_one->ctdp) {
				i = 4 + rsoc_one->cdb_length;

				rsoc_one->to.descriptor_length =
					task_get_uint16(task, i);
				rsoc_one->to.command_specific =
					task_get_uint8(task, i + 3);
				rsoc_one->to.nominal_processing_timeout =
					task_get_uint32(task, i + 4);
				rsoc_one->to.recommended_timeout =
					task_get_uint32(task, i + 8);
			}
			return rsoc_one;
		}
	};

	return NULL;
}

/*
 * MAINTENANCE In / Read Supported Op Codes
 */
struct scsi_task *
scsi_cdb_report_supported_opcodes(int rctd, int options, enum scsi_opcode opcode, int sa, uint32_t alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MAINTENANCE_IN;
	task->cdb[1]   = SCSI_REPORT_SUPPORTED_OP_CODES;
	task->cdb[2]   = options & 0x07;

	if (rctd) {
		task->cdb[2] |= 0x80;
	}

	task->cdb[3]   = opcode;

	scsi_set_uint16(&task->cdb[4], sa);

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

	rc10->lba        = task_get_uint32(task, 0);
	rc10->block_size = task_get_uint32(task, 4);

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
		return task_get_uint8(task, 4) + 5;
	}

	switch (scsi_inquiry_page_code(task)) {
	case SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES:
	case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
	case SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER:
		return task_get_uint8(task, 3) + 4;
	case SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION:
	case SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS:
	case SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING:
		return task_get_uint16(task, 2) + 4;
	default:
		return -1;
	}
}

static struct scsi_inquiry_standard *
scsi_inquiry_unmarshall_standard(struct scsi_task *task)
{
	int i;

	struct scsi_inquiry_standard *inq = scsi_malloc(task, sizeof(*inq));
	if (inq == NULL) {
		return NULL;
	}

	inq->qualifier              = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type            = task_get_uint8(task, 0) & 0x1f;
	inq->rmb                    = !!(task_get_uint8(task, 1) & 0x80);
	inq->version                = task_get_uint8(task, 2);
	inq->normaca                = !!(task_get_uint8(task, 3) & 0x20);
	inq->hisup                  = !!(task_get_uint8(task, 3) & 0x10);
	inq->response_data_format   = task_get_uint8(task, 3) & 0x0f;

	inq->additional_length      = task_get_uint8(task, 4);

	inq->sccs                   = !!(task_get_uint8(task, 5) & 0x80);
	inq->acc                    = !!(task_get_uint8(task, 5) & 0x40);
	inq->tpgs                   = (task_get_uint8(task, 5) >> 4) & 0x03;
	inq->threepc                = !!(task_get_uint8(task, 5) & 0x08);
	inq->protect                = !!(task_get_uint8(task, 5) & 0x01);

	inq->encserv                = !!(task_get_uint8(task, 6) & 0x40);
	inq->multip                 = !!(task_get_uint8(task, 6) & 0x10);
	inq->addr16                 = !!(task_get_uint8(task, 6) & 0x01);
	inq->wbus16                 = !!(task_get_uint8(task, 7) & 0x20);
	inq->sync                   = !!(task_get_uint8(task, 7) & 0x10);
	inq->cmdque                 = !!(task_get_uint8(task, 7) & 0x02);

	memcpy(&inq->vendor_identification[0],
	       &task->datain.data[8], 8);
	memcpy(&inq->product_identification[0],
	       &task->datain.data[16], 16);
	memcpy(&inq->product_revision_level[0],
	       &task->datain.data[32], 4);

	inq->clocking               = (task_get_uint8(task, 56) >> 2) & 0x03;
	inq->qas                    = !!(task_get_uint8(task, 56) & 0x02);
	inq->ius                    = !!(task_get_uint8(task, 56) & 0x01);

	for (i = 0; i < 8; i++) {
		inq->version_descriptor[i] = task_get_uint16(task, 58 + i * 2);
	}

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
	inq->qualifier = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type = task_get_uint8(task, 0) & 0x1f;
	inq->pagecode = task_get_uint8(task, 1);

	inq->num_pages = task_get_uint8(task, 3);
	inq->pages = scsi_malloc(task, inq->num_pages);
	if (inq->pages == NULL) {
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
	inq->qualifier = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type = task_get_uint8(task, 0) & 0x1f;
	inq->pagecode = task_get_uint8(task, 1);

	inq->usn = scsi_malloc(task, task_get_uint8(task, 3) + 1);
	if (inq->usn == NULL) {
		return NULL;
	}
	memcpy(inq->usn, &task->datain.data[4], task_get_uint8(task, 3));
	inq->usn[task_get_uint8(task, 3)] = 0;
	return inq;
}

static struct scsi_inquiry_device_identification *
scsi_inquiry_unmarshall_device_identification(struct scsi_task *task)
{
	struct scsi_inquiry_device_identification *inq = scsi_malloc(task,
							     sizeof(*inq));
	int remaining = task_get_uint16(task, 2);
	unsigned char *dptr;

	if (inq == NULL) {
		return NULL;
	}
	inq->qualifier             = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type           = task_get_uint8(task, 0) & 0x1f;
	inq->pagecode              = task_get_uint8(task, 1);

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
	}

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
	inq->qualifier             = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type           = task_get_uint8(task, 0) & 0x1f;
	inq->pagecode              = task_get_uint8(task, 1);

	inq->wsnz                  = task_get_uint8(task, 4) & 0x01;
	inq->max_cmp               = task_get_uint8(task, 5);
	inq->opt_gran              = task_get_uint16(task, 6);
	inq->max_xfer_len          = task_get_uint32(task, 8);
	inq->opt_xfer_len          = task_get_uint32(task, 12);
	inq->max_prefetch          = task_get_uint32(task, 16);
	inq->max_unmap             = task_get_uint32(task, 20);
	inq->max_unmap_bdc         = task_get_uint32(task, 24);
	inq->opt_unmap_gran        = task_get_uint32(task, 28);
	inq->ugavalid              = !!(task_get_uint8(task, 32)&0x80);
	inq->unmap_gran_align      = task_get_uint32(task, 32) & 0x7fffffff;
	inq->max_ws_len            = task_get_uint32(task, 36);
	inq->max_ws_len            = (inq->max_ws_len << 32)
				   	| task_get_uint32(task, 40);

	inq->max_atomic_xfer_len   = task_get_uint32(task, 44);
	inq->atomic_align          = task_get_uint32(task, 48);
	inq->atomic_gran           = task_get_uint32(task, 52);
	inq->max_atomic_tl_with_atomic_boundary =
                task_get_uint32(task, 56);
	inq->max_atomic_boundary_size =
                task_get_uint32(task, 60);

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
	inq->qualifier             = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type           = task_get_uint8(task, 0) & 0x1f;
	inq->pagecode              = task_get_uint8(task, 1);

	inq->medium_rotation_rate  = task_get_uint16(task, 4);
	inq->product_type          = task_get_uint8(task, 6);
	inq->wabereq               = (task_get_uint8(task, 7) >> 6) & 0x03;
	inq->wacereq               = (task_get_uint8(task, 7) >> 4) & 0x03;
	inq->nominal_form_factor   = task_get_uint8(task, 7) & 0x0f;
	inq->fuab                  = !!(task_get_uint8(task, 8) & 0x02);
	inq->vbuls                 = !!(task_get_uint8(task, 8) & 0x01);
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
	inq->qualifier             = (task_get_uint8(task, 0) >> 5) & 0x07;
	inq->device_type           = task_get_uint8(task, 0) & 0x1f;
	inq->pagecode              = task_get_uint8(task, 1);

	inq->threshold_exponent = task_get_uint8(task, 4);
	inq->lbpu               = !!(task_get_uint8(task, 5) & 0x80);
	inq->lbpws              = !!(task_get_uint8(task, 5) & 0x40);
	inq->lbpws10            = !!(task_get_uint8(task, 5) & 0x20);
	inq->lbprz              = !!(task_get_uint8(task, 5) & 0x04);
	inq->anc_sup            = !!(task_get_uint8(task, 5) & 0x02);
	inq->dp	                = !!(task_get_uint8(task, 5) & 0x01);
	inq->provisioning_type  = task_get_uint8(task, 6) & 0x07;

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
	if (num_blocks > 256) {
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
		task->cdb[4] = num_blocks & 0xff;
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
	scsi_set_uint32(&task->cdb[10], xferlen / blocksize);

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
 * WRITEATOMIC16
 */
struct scsi_task *
scsi_cdb_writeatomic16(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int group_number)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_WRITE_ATOMIC16;

	task->cdb[1] |= ((wrprotect & 0x07) << 5);
	if (dpo) {
		task->cdb[1] |= 0x10;
	}
	if (fua) {
		task->cdb[1] |= 0x08;
	}

	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint16(&task->cdb[12], xferlen / blocksize);

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
	task->cdb[13] = xferlen / blocksize / 2;

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
 * PERSISTENT_RESERVE_OUT
 */
struct scsi_task *
scsi_cdb_persistent_reserve_out(enum scsi_persistent_out_sa sa, enum scsi_persistent_out_scope scope, enum scsi_persistent_out_type type, void *param)
{
	struct scsi_task *task;
	struct scsi_persistent_reserve_out_basic *basic;
	struct scsi_iovec *iov;
	unsigned char *buf;
	int xferlen;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL)
		goto err;

	memset(task, 0, sizeof(struct scsi_task));

	iov = scsi_malloc(task, sizeof(struct scsi_iovec));
	if (iov == NULL)
		goto err;

	switch(sa) {
	case SCSI_PERSISTENT_RESERVE_REGISTER:
	case SCSI_PERSISTENT_RESERVE_RESERVE:
	case SCSI_PERSISTENT_RESERVE_RELEASE:
	case SCSI_PERSISTENT_RESERVE_CLEAR:
	case SCSI_PERSISTENT_RESERVE_PREEMPT:
	case SCSI_PERSISTENT_RESERVE_PREEMPT_AND_ABORT:
	case SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY:
		basic = param;

		xferlen = 24;
		buf = scsi_malloc(task, xferlen);
		if (buf == NULL)
			goto err;
		
		memset(buf, 0, xferlen);
		scsi_set_uint64(&buf[0], basic->reservation_key);
		scsi_set_uint64(&buf[8], basic->service_action_reservation_key);
		if (basic->spec_i_pt) {
			buf[20] |= 0x08;
		}
		if (basic->all_tg_pt) {
			buf[20] |= 0x04;
		}
		if (basic->aptpl) {
			buf[20] |= 0x01;
		}
		break;
	case SCSI_PERSISTENT_RESERVE_REGISTER_AND_MOVE:
		/* XXX FIXME */
		goto err;
	default:
		goto err;
	}

	task->cdb[0] = SCSI_OPCODE_PERSISTENT_RESERVE_OUT;
	task->cdb[1] |= sa & 0x1f;
	task->cdb[2] = ((scope << 4) & 0xf0) | (type & 0x0f);
	
	scsi_set_uint32(&task->cdb[5], xferlen);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = xferlen;

	iov->iov_base = buf;
	iov->iov_len  = xferlen;
	scsi_task_set_iov_out(task, iov, 1);

	return task;

err:
	scsi_free_scsi_task(task);
	return NULL;
}

/*
 * WRITE_SAME10
 */
struct scsi_task *
scsi_cdb_writesame10(int wrprotect, int anchor, int unmap, uint32_t lba, int group, uint16_t num_blocks, uint32_t datalen)
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
	scsi_set_uint32(&task->cdb[2], lba);
	if (group) {
		task->cdb[6] |= (group & 0x1f);
	}
	scsi_set_uint16(&task->cdb[7], num_blocks);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = datalen;

	return task;
}

/*
 * WRITE_SAME16
 */
struct scsi_task *
scsi_cdb_writesame16(int wrprotect, int anchor, int unmap, uint64_t lba, int group, uint32_t num_blocks, uint32_t datalen)
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
	if (datalen == 0) {
		task->cdb[1] |= 0x01;
	}
	scsi_set_uint32(&task->cdb[2], lba >> 32);
	scsi_set_uint32(&task->cdb[6], lba & 0xffffffff);
	scsi_set_uint32(&task->cdb[10], num_blocks);
	if (group) {
		task->cdb[14] |= (group & 0x1f);
	}

	task->cdb_size = 16;
	task->xfer_dir = SCSI_XFER_WRITE;
	task->expxferlen = datalen;

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
 * MODESENSE10
 */
struct scsi_task *
scsi_cdb_modesense10(int llbaa, int dbd, enum scsi_modesense_page_control pc,
		    enum scsi_modesense_page_code page_code,
		    int sub_page_code, unsigned char alloc_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MODESENSE10;

	if (llbaa) {
		task->cdb[1] |= 0x10;
	}
	if (dbd) {
		task->cdb[1] |= 0x08;
	}
	task->cdb[2] = pc<<6 | page_code;
	task->cdb[3] = sub_page_code;

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
 * MODESELECT6
 */
struct scsi_task *
scsi_cdb_modeselect6(int pf, int sp, int param_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MODESELECT6;

	if (pf) {
		task->cdb[1] |= 0x10;
	}
	if (sp) {
		task->cdb[1] |= 0x01;
	}
	task->cdb[4] = param_len;

	task->cdb_size = 6;
	if (param_len != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = param_len;

	return task;
}

/*
 * MODESELECT10
 */
struct scsi_task *
scsi_cdb_modeselect10(int pf, int sp, int param_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_MODESELECT10;

	if (pf) {
		task->cdb[1] |= 0x10;
	}
	if (sp) {
		task->cdb[1] |= 0x01;
	}

	scsi_set_uint16(&task->cdb[7], param_len);

	task->cdb_size = 10;
	if (param_len != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = param_len;

	return task;
}

struct scsi_mode_page *
scsi_modesense_get_page(struct scsi_mode_sense *ms, 
			enum scsi_modesense_page_code page_code,
			int subpage_code)
{
	struct scsi_mode_page *mp;

	for (mp = ms->pages; mp; mp = mp->next) {
		if (mp->page_code == page_code
		&&  mp->subpage_code == subpage_code) {
			return mp;
		}
	}
	return NULL;
}


/*
 * parse the data in blob and calculate the size of a full
 * modesense6 datain structure
 */
static int
scsi_modesense_datain_getfullsize(struct scsi_task *task, int is_modesense6)
{
	int len;

	if (is_modesense6) {
		len = task_get_uint8(task, 0) + 1;
	} else {
		len = task_get_uint16(task, 0) + 2;
	}

	return len;
}

static void
scsi_parse_mode_caching(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->caching.ic   = !!(task_get_uint8(task, pos) & 0x80);
	mp->caching.abpf = !!(task_get_uint8(task, pos) & 0x40);
	mp->caching.cap  = !!(task_get_uint8(task, pos) & 0x20);
	mp->caching.disc = !!(task_get_uint8(task, pos) & 0x10);
	mp->caching.size = !!(task_get_uint8(task, pos) & 0x08);
	mp->caching.wce  = !!(task_get_uint8(task, pos) & 0x04);
	mp->caching.mf   = !!(task_get_uint8(task, pos) & 0x02);
	mp->caching.rcd  = !!(task_get_uint8(task, pos) & 0x01);

	mp->caching.demand_read_retention_priority =
		(task_get_uint8(task, pos + 1) >> 4) & 0x0f;
	mp->caching.write_retention_priority       =
		task_get_uint8(task, pos + 1) & 0x0f;

	mp->caching.disable_prefetch_transfer_length =
		task_get_uint16(task, pos + 2);
	mp->caching.minimum_prefetch = task_get_uint16(task, pos + 4);
	mp->caching.maximum_prefetch = task_get_uint16(task, pos + 6);
	mp->caching.maximum_prefetch_ceiling = task_get_uint16(task, pos + 8);

	mp->caching.fsw    = !!(task_get_uint8(task, pos + 10) & 0x80);
	mp->caching.lbcss  = !!(task_get_uint8(task, pos + 10) & 0x40);
	mp->caching.dra    = !!(task_get_uint8(task, pos + 10) & 0x20);
	mp->caching.nv_dis = !!(task_get_uint8(task, pos + 10) & 0x01);

	mp->caching.number_of_cache_segments = task_get_uint8(task, pos + 11);
	mp->caching.cache_segment_size = task_get_uint16(task, pos + 12);
}

static void
scsi_parse_mode_control(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->control.tst      = (task_get_uint8(task, pos) >> 5) & 0x07;
	mp->control.tmf_only = !!(task_get_uint8(task, pos) & 0x10);
	mp->control.dpicz    = !!(task_get_uint8(task, pos) & 0x08);
	mp->control.d_sense  = !!(task_get_uint8(task, pos) & 0x04);
	mp->control.gltsd    = !!(task_get_uint8(task, pos) & 0x02);
	mp->control.rlec     = !!(task_get_uint8(task, pos) & 0x01);

	mp->control.queue_algorithm_modifier  =
		(task_get_uint8(task, pos + 1) >> 4) & 0x0f;
	mp->control.nuar = task_get_uint8(task, pos + 1) & 0x08;
	mp->control.qerr = (task_get_uint8(task, pos + 1) >> 1) & 0x03;

	mp->control.vs  = !!(task_get_uint8(task, pos + 2) & 0x80);
	mp->control.rac = !!(task_get_uint8(task, pos + 2) & 0x40);
	mp->control.ua_intlck_ctrl =
		(task_get_uint8(task, pos + 2) >> 4) & 0x0f;
	mp->control.swp = !!(task_get_uint8(task, pos + 2) & 0x08);

	mp->control.ato   = !!(task_get_uint8(task, pos + 3) & 0x80);
	mp->control.tas   = !!(task_get_uint8(task, pos + 3) & 0x40);
	mp->control.atmpe = !!(task_get_uint8(task, pos + 3) & 0x20);
	mp->control.rwwp  = !!(task_get_uint8(task, pos + 3) & 0x10);
	mp->control.autoload_mode = !!(task_get_uint8(task, pos + 3) & 0x07);

	mp->control.busy_timeout_period =
		task_get_uint16(task, pos + 6);
	mp->control.extended_selftest_completion_time =
		task_get_uint16(task, pos + 8);
}

static void
scsi_parse_mode_power_condition(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->power_condition.pm_bg_precedence = 
		(task_get_uint8(task, pos) >> 6) & 0x03;
	mp->power_condition.standby_y =
		!!(task_get_uint8(task, pos) & 0x01);

	mp->power_condition.idle_c =
		!!(task_get_uint8(task, pos + 1) & 0x08);
	mp->power_condition.idle_b =
		!!(task_get_uint8(task, pos + 1) & 0x04);
	mp->power_condition.idle_a =
		!!(task_get_uint8(task, pos + 1) & 0x02);
	mp->power_condition.standby_z =
		!!(task_get_uint8(task, pos + 1) & 0x01);

	mp->power_condition.idle_a_condition_timer =
		task_get_uint32(task, pos + 2);
	mp->power_condition.standby_z_condition_timer =
		task_get_uint32(task, pos + 6);
	mp->power_condition.idle_b_condition_timer =
		task_get_uint32(task, pos + 10);
	mp->power_condition.idle_c_condition_timer =
		task_get_uint32(task, pos + 14);
	mp->power_condition.standby_y_condition_timer =
		task_get_uint32(task, pos + 18);

	mp->power_condition.ccf_idle =
		(task_get_uint8(task, pos + 37) >> 6) & 0x03;
	mp->power_condition.ccf_standby =
		(task_get_uint8(task, pos + 37) >> 4) & 0x03;
	mp->power_condition.ccf_stopped =
		(task_get_uint8(task, pos + 37) >> 2) & 0x03;
}

static void
scsi_parse_mode_disconnect_reconnect(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->disconnect_reconnect.buffer_full_ratio =
		task_get_uint8(task, pos);
	mp->disconnect_reconnect.buffer_empty_ratio =
		task_get_uint8(task, pos + 1);
	mp->disconnect_reconnect.bus_inactivity_limit =
		task_get_uint16(task, pos + 2);
	mp->disconnect_reconnect.disconnect_time_limit =
		task_get_uint16(task, pos + 4);
	mp->disconnect_reconnect.connect_time_limit =
		task_get_uint16(task, pos + 6);
	mp->disconnect_reconnect.maximum_burst_size =
		task_get_uint16(task, pos + 8);
	mp->disconnect_reconnect.emdp =
		!!(task_get_uint8(task, pos + 10) & 0x80);
	mp->disconnect_reconnect.fair_arbitration =
		(task_get_uint8(task, pos + 10) >> 4) & 0x0f;
	mp->disconnect_reconnect.dimm =
		!!(task_get_uint8(task, pos + 10) & 0x08);
	mp->disconnect_reconnect.dtdc =
		task_get_uint8(task, pos + 10) & 0x07;
	mp->disconnect_reconnect.first_burst_size =
		task_get_uint16(task, pos + 12);
}

static void
scsi_parse_mode_informational_exceptions_control(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->iec.perf           = !!(task_get_uint8(task, pos) & 0x80);
	mp->iec.ebf            = !!(task_get_uint8(task, pos) & 0x20);
	mp->iec.ewasc          = !!(task_get_uint8(task, pos) & 0x10);
	mp->iec.dexcpt         = !!(task_get_uint8(task, pos) & 0x08);
	mp->iec.test           = !!(task_get_uint8(task, pos) & 0x04);
	mp->iec.ebackerr       = !!(task_get_uint8(task, pos) & 0x02);
	mp->iec.logerr         = !!(task_get_uint8(task, pos) & 0x01);
	mp->iec.mrie           = task_get_uint8(task, pos + 1) & 0x0f;
	mp->iec.interval_timer = task_get_uint32(task, pos + 2);
	mp->iec.report_count   = task_get_uint32(task, pos + 6);
}


/*
 * parse and unmarshall the mode sense data in buffer
 */
static struct scsi_mode_sense *
scsi_modesense_datain_unmarshall(struct scsi_task *task, int is_modesense6)
{
	struct scsi_mode_sense *ms;
	int hdr_len;
	int pos;

	if (is_modesense6) {
		hdr_len = 4;
	} else {
		hdr_len = 8;
	}

	if (task->datain.size < hdr_len) {
		return NULL;
	}

	ms = scsi_malloc(task, sizeof(struct scsi_mode_sense));
	if (ms == NULL) {
		return NULL;
	}

	if (is_modesense6) {
		ms->mode_data_length          = task_get_uint8(task, 0);
		ms->medium_type               = task_get_uint8(task, 1);
		ms->device_specific_parameter = task_get_uint8(task, 2);
		ms->block_descriptor_length   = task_get_uint8(task, 3);
		ms->pages                     = NULL;
	} else {
		ms->mode_data_length          = task_get_uint16(task, 0);
		ms->medium_type               = task_get_uint8(task, 2);
		ms->device_specific_parameter = task_get_uint8(task, 3);
		ms->longlba = task_get_uint8(task, 4) & 0x01;
		ms->block_descriptor_length   = task_get_uint16(task, 6);
		ms->pages                     = NULL;
	}

	if (ms->mode_data_length + 1 > task->datain.size) {
		return NULL;
	}

	pos = hdr_len + ms->block_descriptor_length;
	while (pos < task->datain.size) {
		struct scsi_mode_page *mp;

		mp = scsi_malloc(task, sizeof(struct scsi_mode_page));
		if (mp == NULL) {
			return ms;
		}
		mp->ps           = task_get_uint8(task, pos) & 0x80;
		mp->spf          = task_get_uint8(task, pos) & 0x40;
		mp->page_code    = task_get_uint8(task, pos) & 0x3f;
		pos++;

		if (mp->spf) {
			mp->subpage_code = task_get_uint8(task, pos);
			mp->len = task_get_uint16(task, pos + 1);
			pos += 3;
		} else {
			mp->subpage_code = 0;
			mp->len          = task_get_uint8(task, pos);
			pos++;
		}

		switch (mp->page_code) {
		case SCSI_MODEPAGE_CACHING:
			scsi_parse_mode_caching(task, pos, mp);
			break;
		case SCSI_MODEPAGE_CONTROL:
			scsi_parse_mode_control(task, pos, mp);
			break;
		case SCSI_MODEPAGE_DISCONNECT_RECONNECT:
			scsi_parse_mode_disconnect_reconnect(task, pos, mp);
			break;
		case SCSI_MODEPAGE_INFORMATIONAL_EXCEPTIONS_CONTROL:
			scsi_parse_mode_informational_exceptions_control(task, pos, mp);
			break;
		case SCSI_MODEPAGE_POWER_CONDITION:
			scsi_parse_mode_power_condition(task, pos, mp);
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

static struct scsi_data *
scsi_modesense_marshall_caching(struct scsi_task *task,
				struct scsi_mode_page *mp,
				int hdr_size)
{
	struct scsi_data *data;

	data = scsi_malloc(task, sizeof(struct scsi_data));

	data->size = 20 + hdr_size;
	data->data = scsi_malloc(task, data->size);

	if (mp->caching.ic)   data->data[hdr_size + 2] |= 0x80;
	if (mp->caching.abpf) data->data[hdr_size + 2] |= 0x40;
	if (mp->caching.cap)  data->data[hdr_size + 2] |= 0x20;
	if (mp->caching.disc) data->data[hdr_size + 2] |= 0x10;
	if (mp->caching.size) data->data[hdr_size + 2] |= 0x08;
	if (mp->caching.wce)  data->data[hdr_size + 2] |= 0x04;
	if (mp->caching.mf)   data->data[hdr_size + 2] |= 0x02;
	if (mp->caching.rcd)  data->data[hdr_size + 2] |= 0x01;

	data->data[hdr_size + 3] |= (mp->caching.demand_read_retention_priority << 4) & 0xf0;
	data->data[hdr_size + 3] |= mp->caching.write_retention_priority & 0x0f;

	scsi_set_uint16(&data->data[hdr_size + 4], mp->caching.disable_prefetch_transfer_length);
	scsi_set_uint16(&data->data[hdr_size + 6], mp->caching.minimum_prefetch);
	scsi_set_uint16(&data->data[hdr_size + 8], mp->caching.maximum_prefetch);
	scsi_set_uint16(&data->data[hdr_size + 10], mp->caching.maximum_prefetch_ceiling);

	if (mp->caching.fsw)    data->data[hdr_size + 12] |= 0x80;
	if (mp->caching.lbcss)  data->data[hdr_size + 12] |= 0x40;
	if (mp->caching.dra)    data->data[hdr_size + 12] |= 0x20;
	if (mp->caching.nv_dis) data->data[hdr_size + 12] |= 0x01;

	data->data[hdr_size + 13] = mp->caching.number_of_cache_segments;

	scsi_set_uint16(&data->data[hdr_size + 14], mp->caching.cache_segment_size);

	return data;
}

static struct scsi_data *
scsi_modesense_marshall_control(struct scsi_task *task,
				struct scsi_mode_page *mp,
				int hdr_size)
{
	struct scsi_data *data;

	data = scsi_malloc(task, sizeof(struct scsi_data));

	data->size = 12 + hdr_size;
	data->data = scsi_malloc(task, data->size);

	data->data[hdr_size + 2] |= (mp->control.tst << 5) & 0xe0;
	if (mp->control.tmf_only) data->data[hdr_size + 2] |= 0x10;
	if (mp->control.dpicz)    data->data[hdr_size + 2] |= 0x08;
	if (mp->control.d_sense)  data->data[hdr_size + 2] |= 0x04;
	if (mp->control.gltsd)    data->data[hdr_size + 2] |= 0x02;
	if (mp->control.rlec)     data->data[hdr_size + 2] |= 0x01;

	data->data[hdr_size + 3] |= (mp->control.queue_algorithm_modifier << 4) & 0xf0;
	if (mp->control.nuar)     data->data[hdr_size + 3] |= 0x08;
	data->data[hdr_size + 3] |= (mp->control.qerr << 1) & 0x06;

	if (mp->control.vs)       data->data[hdr_size + 4] |= 0x80;
	if (mp->control.rac)      data->data[hdr_size + 4] |= 0x40;
	data->data[hdr_size + 4] |= (mp->control.ua_intlck_ctrl << 4) & 0x30;
	if (mp->control.swp)      data->data[hdr_size + 4] |= 0x08;

	if (mp->control.ato)      data->data[hdr_size + 5] |= 0x80;
	if (mp->control.tas)      data->data[hdr_size + 5] |= 0x40;
	if (mp->control.atmpe)    data->data[hdr_size + 5] |= 0x20;
	if (mp->control.rwwp)     data->data[hdr_size + 5] |= 0x10;
	data->data[hdr_size + 5] |= mp->control.autoload_mode & 0x07;

	scsi_set_uint16(&data->data[hdr_size + 8], mp->control.busy_timeout_period);
	scsi_set_uint16(&data->data[hdr_size + 10], mp->control.extended_selftest_completion_time);

	return data;
}

static struct scsi_data *
scsi_modesense_marshall_power_condition(struct scsi_task *task,
					struct scsi_mode_page *mp,
					int hdr_size)
{
	struct scsi_data *data;

	data = scsi_malloc(task, sizeof(struct scsi_data));

	data->size = 40 + hdr_size;
	data->data = scsi_malloc(task, data->size);

	data->data[hdr_size + 2] |=
		(mp->power_condition.pm_bg_precedence << 6) & 0xc0;
	if (mp->power_condition.standby_y) data->data[hdr_size + 2] |= 0x01;

	if (mp->power_condition.idle_c) data->data[hdr_size + 3] |= 0x08;
	if (mp->power_condition.idle_b) data->data[hdr_size + 3] |= 0x04;
	if (mp->power_condition.idle_a) data->data[hdr_size + 3] |= 0x02;
	if (mp->power_condition.standby_z) data->data[hdr_size + 3] |= 0x01;

	scsi_set_uint32(&data->data[hdr_size + 4],
		mp->power_condition.idle_a_condition_timer);
	scsi_set_uint32(&data->data[hdr_size + 8],
		mp->power_condition.standby_z_condition_timer);
	scsi_set_uint32(&data->data[hdr_size + 12],
		mp->power_condition.idle_b_condition_timer);
	scsi_set_uint32(&data->data[hdr_size + 16],
		mp->power_condition.idle_c_condition_timer);
	scsi_set_uint32(&data->data[hdr_size + 20],
		mp->power_condition.standby_y_condition_timer);

	data->data[hdr_size + 39] |=
		(mp->power_condition.ccf_idle << 6) & 0xc0;
	data->data[hdr_size + 39] |=
		(mp->power_condition.ccf_standby << 4) & 0x30;
	data->data[hdr_size + 39] |=
		(mp->power_condition.ccf_stopped << 2) & 0x0c;

	return data;
}

static struct scsi_data *
scsi_modesense_marshall_disconnect_reconnect(struct scsi_task *task,
					struct scsi_mode_page *mp,
					int hdr_size)
{
	struct scsi_data *data;

	data = scsi_malloc(task, sizeof(struct scsi_data));

	data->size = 16 + hdr_size;
	data->data = scsi_malloc(task, data->size);

	data->data[hdr_size + 2] = mp->disconnect_reconnect.buffer_full_ratio;
	data->data[hdr_size + 3] = mp->disconnect_reconnect.buffer_empty_ratio;
	scsi_set_uint16(&data->data[hdr_size + 4], mp->disconnect_reconnect.bus_inactivity_limit);
	scsi_set_uint16(&data->data[hdr_size + 6], mp->disconnect_reconnect.disconnect_time_limit);
	scsi_set_uint16(&data->data[hdr_size + 8], mp->disconnect_reconnect.connect_time_limit);
	scsi_set_uint16(&data->data[hdr_size + 10], mp->disconnect_reconnect.maximum_burst_size);

	if (mp->disconnect_reconnect.emdp) data->data[hdr_size + 12] |= 0x80;
	data->data[hdr_size + 12] |= (mp->disconnect_reconnect.fair_arbitration << 4) & 0x70;
	if (mp->disconnect_reconnect.dimm) data->data[hdr_size + 12] |= 0x08;
	data->data[hdr_size + 12] |= mp->disconnect_reconnect.dtdc & 0x07;

	scsi_set_uint16(&data->data[hdr_size + 14], mp->disconnect_reconnect.first_burst_size);

	return data;
}

static struct scsi_data *
scsi_modesense_marshall_informational_exceptions_control(struct scsi_task *task,
					struct scsi_mode_page *mp,
					int hdr_size)
{
	struct scsi_data *data;

	data = scsi_malloc(task, sizeof(struct scsi_data));

	data->size = 12 + hdr_size;
	data->data = scsi_malloc(task, data->size);

	if (mp->iec.perf)     data->data[hdr_size + 2] |= 0x80;
	if (mp->iec.ebf)      data->data[hdr_size + 2] |= 0x20;
	if (mp->iec.ewasc)    data->data[hdr_size + 2] |= 0x10;
	if (mp->iec.dexcpt)   data->data[hdr_size + 2] |= 0x08;
	if (mp->iec.test)     data->data[hdr_size + 2] |= 0x04;
	if (mp->iec.ebackerr) data->data[hdr_size + 2] |= 0x02;
	if (mp->iec.logerr)   data->data[hdr_size + 2] |= 0x01;

	data->data[hdr_size + 3] |= mp->iec.mrie & 0x0f;

	scsi_set_uint32(&data->data[hdr_size + 4], mp->iec.interval_timer);
	scsi_set_uint32(&data->data[hdr_size + 8], mp->iec.report_count);

	return data;
}

/*
 * marshall the mode sense data out buffer
 */
struct scsi_data *
scsi_modesense_dataout_marshall(struct scsi_task *task,
				struct scsi_mode_page *mp,
				int is_modeselect6)
{
	struct scsi_data *data;
	int hdr_size = is_modeselect6 ? 4 : 8;

	switch (mp->page_code) {
	case SCSI_MODEPAGE_CACHING:
		data = scsi_modesense_marshall_caching(task, mp, hdr_size);
		break;
	case SCSI_MODEPAGE_CONTROL:
		data = scsi_modesense_marshall_control(task, mp, hdr_size);
		break;
	case SCSI_MODEPAGE_DISCONNECT_RECONNECT:
		data = scsi_modesense_marshall_disconnect_reconnect(task, mp, hdr_size);
		break;
	case SCSI_MODEPAGE_INFORMATIONAL_EXCEPTIONS_CONTROL:
		data = scsi_modesense_marshall_informational_exceptions_control(task, mp, hdr_size);
		break;
	case SCSI_MODEPAGE_POWER_CONDITION:
		data = scsi_modesense_marshall_power_condition(task, mp, hdr_size);
		break;
	default:
		/* TODO error reporting ? */
		return NULL;
	}

	if (data == NULL) {
		return NULL;
	}

	data->data[hdr_size + 0] = mp->page_code & 0x3f;
	if (mp->ps) {
		data->data[hdr_size + 0] |= 0x80;
	}
	if (mp->spf) {
		data->data[hdr_size + 0] |= 0x40;
		data->data[hdr_size + 1] = mp->subpage_code;
		scsi_set_uint16(&data->data[hdr_size + 2], data->size -hdr_size - 4);
	} else {
		data->data[hdr_size + 1] = (data->size - hdr_size - 2) & 0xff;
	}

	return data;
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

/*
 * EXTENDED COPY
 */
struct scsi_task *
scsi_cdb_extended_copy(int param_len)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL)
		return NULL;

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]	= SCSI_OPCODE_EXTENDED_COPY;
	task->cdb[10]	= (param_len >> 24) & 0xFF;
	task->cdb[11]	= (param_len >> 16) & 0xFF;
	task->cdb[12] 	= (param_len >> 8) & 0xFF;
	task->cdb[13]	= param_len & 0xFF;
	/* Inititalize other fields in CDB */
	task->cdb_size = 16;
        if (param_len) {
                task->xfer_dir = SCSI_XFER_WRITE;
        }
	task->expxferlen = param_len;

	return task;
}

/*
 * RECEIVE COPY RESULTS
 */
struct scsi_task *
scsi_cdb_receive_copy_results(enum scsi_copy_results_sa sa, int list_id, int xferlen)
{
	struct scsi_task *task;

	task = malloc(sizeof(struct scsi_task));
	if (task == NULL) {
		return NULL;
	}

	memset(task, 0, sizeof(struct scsi_task));
	task->cdb[0]   = SCSI_OPCODE_RECEIVE_COPY_RESULTS;
	task->cdb[1] |= sa & 0x1f;
	task->cdb[2] = list_id & 0xFF;

	scsi_set_uint32(&task->cdb[10], xferlen);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
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
		return scsi_modesense_datain_getfullsize(task, 1);
	case SCSI_OPCODE_READCAPACITY10:
		return scsi_readcapacity10_datain_getfullsize(task);
	case SCSI_OPCODE_SYNCHRONIZECACHE10:
		return 0;
	case SCSI_OPCODE_READTOC:
		return scsi_readtoc_datain_getfullsize(task);
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_getfullsize(task);
	case SCSI_OPCODE_PERSISTENT_RESERVE_IN:
		return scsi_persistentreservein_datain_getfullsize(task);
	case SCSI_OPCODE_MAINTENANCE_IN:
		return scsi_maintenancein_datain_getfullsize(task);
	}
	return -1;
}

void *
scsi_datain_unmarshall(struct scsi_task *task)
{
	if (!task || !task->datain.size)
		return NULL;

	switch (task->cdb[0]) {
	case SCSI_OPCODE_INQUIRY:
		return scsi_inquiry_datain_unmarshall(task);
	case SCSI_OPCODE_MODESENSE6:
		return scsi_modesense_datain_unmarshall(task, 1);
	case SCSI_OPCODE_MODESENSE10:
		return scsi_modesense_datain_unmarshall(task, 0);
	case SCSI_OPCODE_READCAPACITY10:
		return scsi_readcapacity10_datain_unmarshall(task);
	case SCSI_OPCODE_READTOC:
		return scsi_readtoc_datain_unmarshall(task);
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_unmarshall(task);
	case SCSI_OPCODE_SERVICE_ACTION_IN:
		return scsi_serviceactionin_datain_unmarshall(task);
	case SCSI_OPCODE_PERSISTENT_RESERVE_IN:
		return scsi_persistentreservein_datain_unmarshall(task);
	case SCSI_OPCODE_MAINTENANCE_IN:
		return scsi_maintenancein_datain_unmarshall(task);
	case SCSI_OPCODE_RECEIVE_COPY_RESULTS:
		return scsi_receivecopyresults_datain_unmarshall(task);
	}
	return NULL;
}


static struct scsi_read6_cdb *
scsi_read6_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_read6_cdb *read6;

	read6 = scsi_malloc(task, sizeof(struct scsi_read6_cdb));
	if (read6 == NULL) {
		return NULL;
	}

	read6->opcode          = SCSI_OPCODE_READ6;
	read6->lba             = scsi_get_uint32(&task->cdb[0]) & 0x001fffff;
	read6->transfer_length = task->cdb[4];
	read6->control         = task->cdb[5];

        return read6;
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

static struct scsi_read12_cdb *
scsi_read12_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_read12_cdb *read12;

	read12 = scsi_malloc(task, sizeof(struct scsi_read12_cdb));
	if (read12 == NULL) {
		return NULL;
	}

	read12->opcode          = SCSI_OPCODE_READ12;
	read12->rdprotect       = (task->cdb[1] >> 5) & 0x7;
	read12->dpo             = !!(task->cdb[1] & 0x10);
	read12->fua             = !!(task->cdb[1] & 0x08);
	read12->rarc            = !!(task->cdb[1] & 0x04);
	read12->fua_nv          = !!(task->cdb[1] & 0x02);
	read12->lba             = scsi_get_uint32(&task->cdb[2]);
	read12->transfer_length = scsi_get_uint32(&task->cdb[6]);
	read12->group           = task->cdb[10] & 0x1f;
	read12->control         = task->cdb[11];

        return read12;
}

static struct scsi_read16_cdb *
scsi_read16_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_read16_cdb *read16;

	read16 = scsi_malloc(task, sizeof(struct scsi_read16_cdb));
	if (read16 == NULL) {
		return NULL;
	}

	read16->opcode          = SCSI_OPCODE_READ16;
	read16->rdprotect       = (task->cdb[1] >> 5) & 0x7;
	read16->dpo             = !!(task->cdb[1] & 0x10);
	read16->fua             = !!(task->cdb[1] & 0x08);
	read16->rarc            = !!(task->cdb[1] & 0x04);
	read16->fua_nv          = !!(task->cdb[1] & 0x02);
	read16->lba             = scsi_get_uint64(&task->cdb[2]);
	read16->transfer_length = scsi_get_uint32(&task->cdb[10]);
	read16->group           = task->cdb[14] & 0x1f;
	read16->control         = task->cdb[15];

        return read16;
}

static struct scsi_verify10_cdb *
scsi_verify10_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_verify10_cdb *verify10;

	verify10 = scsi_malloc(task, sizeof(struct scsi_verify10_cdb));
	if (verify10 == NULL) {
		return NULL;
	}

	verify10->opcode              = SCSI_OPCODE_VERIFY10;
	verify10->vrprotect           = (task->cdb[1] >> 5) & 0x7;
	verify10->dpo                 = !!(task->cdb[1] & 0x10);
	verify10->bytchk              = !!(task->cdb[1] & 0x02);
	verify10->lba                 = scsi_get_uint32(&task->cdb[2]);
	verify10->group               = task->cdb[6] & 0x1f;
	verify10->verification_length = scsi_get_uint16(&task->cdb[7]);
	verify10->control             = task->cdb[9];

        return verify10;
}

static struct scsi_verify12_cdb *
scsi_verify12_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_verify12_cdb *verify12;

	verify12 = scsi_malloc(task, sizeof(struct scsi_verify12_cdb));
	if (verify12 == NULL) {
		return NULL;
	}

	verify12->opcode              = SCSI_OPCODE_VERIFY12;
	verify12->vrprotect           = (task->cdb[1] >> 5) & 0x7;
	verify12->dpo                 = !!(task->cdb[1] & 0x10);
	verify12->bytchk              = !!(task->cdb[1] & 0x02);
	verify12->lba                 = scsi_get_uint32(&task->cdb[2]);
	verify12->verification_length = scsi_get_uint32(&task->cdb[6]);
	verify12->group               = task->cdb[10] & 0x1f;
	verify12->control             = task->cdb[11];

        return verify12;
}

static struct scsi_verify16_cdb *
scsi_verify16_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_verify16_cdb *verify16;

	verify16 = scsi_malloc(task, sizeof(struct scsi_verify16_cdb));
	if (verify16 == NULL) {
		return NULL;
	}

	verify16->opcode              = SCSI_OPCODE_VERIFY16;
	verify16->vrprotect           = (task->cdb[1] >> 5) & 0x7;
	verify16->dpo                 = !!(task->cdb[1] & 0x10);
	verify16->bytchk              = !!(task->cdb[1] & 0x02);
	verify16->lba                 = scsi_get_uint64(&task->cdb[2]);
	verify16->verification_length = scsi_get_uint32(&task->cdb[10]);
	verify16->group               = task->cdb[14] & 0x1f;
	verify16->control             = task->cdb[15];

        return verify16;
}

static struct scsi_write10_cdb *
scsi_write10_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_write10_cdb *write10;

	write10 = scsi_malloc(task, sizeof(struct scsi_write10_cdb));
	if (write10 == NULL) {
		return NULL;
	}

	write10->opcode          = SCSI_OPCODE_WRITE10;
	write10->wrprotect       = (task->cdb[1] >> 5) & 0x7;
	write10->dpo             = !!(task->cdb[1] & 0x10);
	write10->fua             = !!(task->cdb[1] & 0x08);
	write10->fua_nv          = !!(task->cdb[1] & 0x02);
	write10->lba             = scsi_get_uint32(&task->cdb[2]);
	write10->group           = task->cdb[6] & 0x1f;
	write10->transfer_length = scsi_get_uint16(&task->cdb[7]);
	write10->control         = task->cdb[9];

        return write10;
}

static struct scsi_write12_cdb *
scsi_write12_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_write12_cdb *write12;

	write12 = scsi_malloc(task, sizeof(struct scsi_write12_cdb));
	if (write12 == NULL) {
		return NULL;
	}

	write12->opcode          = SCSI_OPCODE_WRITE12;
	write12->wrprotect       = (task->cdb[1] >> 5) & 0x7;
	write12->dpo             = !!(task->cdb[1] & 0x10);
	write12->fua             = !!(task->cdb[1] & 0x08);
	write12->fua_nv          = !!(task->cdb[1] & 0x02);
	write12->lba             = scsi_get_uint32(&task->cdb[2]);
	write12->transfer_length = scsi_get_uint32(&task->cdb[6]);
	write12->group           = task->cdb[10] & 0x1f;
	write12->control         = task->cdb[11];

        return write12;
}

static struct scsi_write16_cdb *
scsi_write16_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_write16_cdb *write16;

	write16 = scsi_malloc(task, sizeof(struct scsi_write16_cdb));
	if (write16 == NULL) {
		return NULL;
	}

	write16->opcode          = SCSI_OPCODE_WRITE16;
	write16->wrprotect       = (task->cdb[1] >> 5) & 0x7;
	write16->dpo             = !!(task->cdb[1] & 0x10);
	write16->fua             = !!(task->cdb[1] & 0x08);
	write16->fua_nv          = !!(task->cdb[1] & 0x02);
	write16->lba             = scsi_get_uint64(&task->cdb[2]);
	write16->transfer_length = scsi_get_uint32(&task->cdb[10]);
	write16->group           = task->cdb[14] & 0x1f;
	write16->control         = task->cdb[15];

        return write16;
}

static struct scsi_writeatomic16_cdb *
scsi_writeatomic16_cdb_unmarshall(struct scsi_task *task)
{
	struct scsi_writeatomic16_cdb *writeatomic16;

	writeatomic16 = scsi_malloc(task, sizeof(struct scsi_writeatomic16_cdb));
	if (writeatomic16 == NULL) {
		return NULL;
	}

	writeatomic16->opcode          = SCSI_OPCODE_WRITE_ATOMIC16;
	writeatomic16->wrprotect       = (task->cdb[1] >> 5) & 0x7;
	writeatomic16->dpo             = !!(task->cdb[1] & 0x10);
	writeatomic16->fua             = !!(task->cdb[1] & 0x08);
	writeatomic16->lba             = scsi_get_uint64(&task->cdb[2]);
	writeatomic16->transfer_length = scsi_get_uint16(&task->cdb[12]);
	writeatomic16->group           = task->cdb[14] & 0x1f;
	writeatomic16->control         = task->cdb[15];

        return writeatomic16;
}

void *
scsi_cdb_unmarshall(struct scsi_task *task, enum scsi_opcode opcode)
{
	if (task->cdb[0] != opcode) {
		return NULL;
	}

	switch (task->cdb[0]) {
	case SCSI_OPCODE_READ6:
		return scsi_read6_cdb_unmarshall(task);
	case SCSI_OPCODE_READ10:
		return scsi_read10_cdb_unmarshall(task);
	case SCSI_OPCODE_READ12:
		return scsi_read12_cdb_unmarshall(task);
	case SCSI_OPCODE_READ16:
		return scsi_read16_cdb_unmarshall(task);
	case SCSI_OPCODE_VERIFY10:
		return scsi_verify10_cdb_unmarshall(task);
	case SCSI_OPCODE_VERIFY12:
		return scsi_verify12_cdb_unmarshall(task);
	case SCSI_OPCODE_VERIFY16:
		return scsi_verify16_cdb_unmarshall(task);
	case SCSI_OPCODE_WRITE10:
		return scsi_write10_cdb_unmarshall(task);
	case SCSI_OPCODE_WRITE12:
		return scsi_write12_cdb_unmarshall(task);
	case SCSI_OPCODE_WRITE16:
		return scsi_write16_cdb_unmarshall(task);
	case SCSI_OPCODE_WRITE_ATOMIC16:
		return scsi_writeatomic16_cdb_unmarshall(task);
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
scsi_version_descriptor_to_str(enum scsi_version_descriptor version_descriptor)
{
	switch (version_descriptor) {
	case SCSI_VERSION_DESCRIPTOR_ISCSI:
		return "iSCSI";
	case SCSI_VERSION_DESCRIPTOR_SBC:
		return "SBC";
	case SCSI_VERSION_DESCRIPTOR_SBC_ANSI_INCITS_306_1998:
		return "SBC ANSI INCITS 306-1998";
	case SCSI_VERSION_DESCRIPTOR_SBC_T10_0996_D_R08C:
		return "SBC T10/0996-D revision 08c";
	case SCSI_VERSION_DESCRIPTOR_SBC_2:
		return "SBC-2";
	case SCSI_VERSION_DESCRIPTOR_SBC_2_ISO_IEC_14776_322:
		return "SBC-2 ISO/IEC 14776-322";
	case SCSI_VERSION_DESCRIPTOR_SBC_2_ANSI_INCITS_405_2005:
		return "SBC-2 ANSI INCITS 405-2005";
	case SCSI_VERSION_DESCRIPTOR_SBC_2_T10_1417_D_R16:
		return "SBC-2 T10/1417-D revision 16";
	case SCSI_VERSION_DESCRIPTOR_SBC_2_T10_1417_D_R5A:
		return "SBC-2 T10/1417-D revision 5A";
	case SCSI_VERSION_DESCRIPTOR_SBC_2_T10_1417_D_R15:
		return "SBC-2 T10/1417-D revision 15";
	case SCSI_VERSION_DESCRIPTOR_SBC_3:
		return "SBC-3";
	case SCSI_VERSION_DESCRIPTOR_SPC:
		return "SPC";
	case SCSI_VERSION_DESCRIPTOR_SPC_ANSI_INCITS_301_1997:
		return "SPC ANSI INCITS 301-1997";
	case SCSI_VERSION_DESCRIPTOR_SPC_T10_0995_D_R11A:
		return "SPC T10/0995-D revision 11a";
	case SCSI_VERSION_DESCRIPTOR_SPC_2:
		return "SPC-2";
	case SCSI_VERSION_DESCRIPTOR_SPC_2_ISO_IEC_14776_452:
		return "SPC-2 ISO.IEC 14776-452";
	case SCSI_VERSION_DESCRIPTOR_SPC_2_ANSI_INCITS_351_2001:
		return "SPC-2 ANSI INCITS 351-2001";
	case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R20:
		return "SPC-2 T10/1236-D revision 20";
	case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R12:
		return "SPC-2 T10/1236-D revision 12";
	case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R18:
		return "SPC-2 T10/1236-D revision 18";
	case SCSI_VERSION_DESCRIPTOR_SPC_2_T10_1236_D_R19:
		return "SPC-2 T10/1236-D revision 19";
	case SCSI_VERSION_DESCRIPTOR_SPC_3:
		return "SPC-3";
	case SCSI_VERSION_DESCRIPTOR_SPC_3_ISO_IEC_14776_453:
		return "SPC-3 ISO/IEC 14776-453";
	case SCSI_VERSION_DESCRIPTOR_SPC_3_ANSI_INCITS_408_2005:
		return "SPC-3 ANSI INCITS 408-2005";
	case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R7:
		return "SPC-3 T10/1416-D revision 7";
	case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R21:
		return "SPC-3 T10/1416-D revision 21";
	case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R22:
		return "SPC-3 T10/1416-D revision 22";
	case SCSI_VERSION_DESCRIPTOR_SPC_3_T10_1416_D_R23:
		return "SPC-3 T10/1416-D revision 23";
	case SCSI_VERSION_DESCRIPTOR_SPC_4:
		return "SPC-4";
	case SCSI_VERSION_DESCRIPTOR_SPC_4_T10_1731_D_R16:
		return "SPC-4 T10/1731-D revision 16";
	case SCSI_VERSION_DESCRIPTOR_SPC_4_T10_1731_D_R18:
		return "SPC-4 T10/1731-D revision 18";
	case SCSI_VERSION_DESCRIPTOR_SPC_4_T10_1731_D_R23:
		return "SPC-4 T10/1731-D revision 23";
	case SCSI_VERSION_DESCRIPTOR_SSC:
		return "SSC";
	case SCSI_VERSION_DESCRIPTOR_UAS_T10_2095D_R04:
		return "UAS T10/2095-D revision 04";
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
	case SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS:
		return "BLOCK_LIMITS";
	case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
		return "BLOCK_DEVICE_CHARACTERISTICS";
	case SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING:
		return "LOGICAL_BLOCK_PROVISIONING";
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

void
scsi_task_reset_iov(struct scsi_iovector *iovector)
{
	iovector->offset = 0;
	iovector->consumed = 0;
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

int
scsi_task_get_status(struct scsi_task *task, struct scsi_sense *sense)
{
	if (sense) {
		memcpy(sense, &task->sense, sizeof(struct scsi_sense));
	}
	return task->status;
}
