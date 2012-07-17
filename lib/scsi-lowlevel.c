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
 * would be nice if this could grow into a full blown library for scsi to
 * 1, build a CDB
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
#include "slist.h"
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"


void
scsi_free_scsi_task(struct scsi_task *task)
{
	struct scsi_allocated_memory *mem;

	while ((mem = task->mem)) {
		   SLIST_REMOVE(&task->mem, mem);
		   free(mem->ptr);
		   free(mem);
	}

	free(task->datain.data);
	free(task);
}

void *
scsi_malloc(struct scsi_task *task, size_t size)
{
	struct scsi_allocated_memory *mem;

	mem = malloc(sizeof(struct scsi_allocated_memory));
	if (mem == NULL) {
		return NULL;
	}
	memset(mem, 0, sizeof(struct scsi_allocated_memory));
	mem->ptr = malloc(size);
	if (mem->ptr == NULL) {
		free(mem);
		return NULL;
	}
	memset(mem->ptr, 0, size);
	SLIST_ADD(&task->mem, mem);
	return mem->ptr;
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
	       {0, NULL}
	};

	return value_string_find(ascqs, ascq);
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
	*(uint32_t *)&task->cdb[6] = htonl(alloc_len);

	task->cdb_size = 12;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.reportluns.report_type = report_type;

	return task;
}

/*
 * parse the data in blob and calcualte the size of a full report luns
 * datain structure
 */
static int
scsi_reportluns_datain_getfullsize(struct scsi_task *task)
{
	uint32_t list_size;

	list_size = htonl(*(uint32_t *)&(task->datain.data[0])) + 8;

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

	list_size = htonl(*(uint32_t *)&(task->datain.data[0])) + 8;
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
		list->luns[i] = htons(*(uint16_t *)
				      &(task->datain.data[i*8+8]));
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

	*(uint32_t *)&task->cdb[2] = htonl(lba);

	if (pmi) {
		task->cdb[8] |= 0x01;
	}

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = 8;

	task->params.readcapacity10.lba = lba;
	task->params.readcapacity10.pmi = pmi;

	return task;
}

/*
 * service_action_in unmarshall
 */
static void *
scsi_serviceactionin_datain_unmarshall(struct scsi_task *task)
{
	struct scsi_readcapacity16 *rc16;
	struct scsi_get_lba_status *gls;
	int32_t len;
	int i;

	switch (task->params.serviceactionin.sa) {
	case SCSI_READCAPACITY16:
		rc16 = scsi_malloc(task, sizeof(struct scsi_readcapacity16));
		if (rc16 == NULL) {
			return NULL;
		}
		rc16->returned_lba = ntohl(*(uint32_t *)&(task->datain.data[0]));
		rc16->returned_lba = (rc16->returned_lba << 32) | ntohl(*(uint32_t *)&(task->datain.data[4]));
		rc16->block_length = ntohl(*(uint32_t *)&(task->datain.data[8]));
		rc16->p_type       = (task->datain.data[12] >> 1) & 0x07;
		rc16->prot_en      = task->datain.data[12] & 0x01;
		rc16->p_i_exp      = (task->datain.data[13] >> 4) & 0x0f;
		rc16->lbppbe       = task->datain.data[13] & 0x0f;
		rc16->lbpme        = !!(task->datain.data[14] & 0x80);
		rc16->lbprz        = !!(task->datain.data[14] & 0x40);
		rc16->lalba        = ntohs(*(uint16_t *)&(task->datain.data[14])) & 0x3fff;
		return rc16;
	case SCSI_GET_LBA_STATUS:
		len = ntohl(*(uint32_t *)&(task->datain.data[0]));
		if (len > task->datain.size - 4) {
			len = task->datain.size - 4;
		}
		len = len / 16;

		gls = scsi_malloc(task, sizeof(struct scsi_get_lba_status));
		if (gls == NULL) {
			return NULL;
		}
		gls->num_descriptors = len;
		gls->descriptors = scsi_malloc(task, sizeof(struct scsi_lba_status_descriptor) * gls->num_descriptors);
		if (gls->descriptors == NULL) {
			return NULL;
		}

		for (i = 0; i < (int)gls->num_descriptors; i++) {
			gls->descriptors[i].lba  = ntohl(*(uint32_t *)&(task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 0]));
			gls->descriptors[i].lba <<= 32;
 			gls->descriptors[i].lba |= ntohl(*(uint32_t *)&(task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 4]));

			gls->descriptors[i].num_blocks = ntohl(*(uint32_t *)&(task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 8]));

			gls->descriptors[i].provisioning = task->datain.data[8 + i * sizeof(struct scsi_lba_status_descriptor) + 12] & 0x0f;
		}

		return gls;
	}

	return NULL;
}

/*
 * parse the data in blob and calcualte the size of a full
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

	rc10->lba        = htonl(*(uint32_t *)&(task->datain.data[0]));
	rc10->block_size = htonl(*(uint32_t *)&(task->datain.data[4]));

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

	*(uint16_t *)&task->cdb[3] = htons(alloc_len);

	task->cdb_size = 6;
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.inquiry.evpd      = evpd;
	task->params.inquiry.page_code = page_code;

	return task;
}

/*
 * parse the data in blob and calcualte the size of a full
 * inquiry datain structure
 */
static int
scsi_inquiry_datain_getfullsize(struct scsi_task *task)
{
	if (task->params.inquiry.evpd == 0) {
		return task->datain.data[4] + 3;
	}

	switch (task->params.inquiry.page_code) {
	case SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES:
	case SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS:
	case SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER:
		return task->datain.data[3] + 4;
	case SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION:
	case SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS:
	case SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING:
		return ntohs(*(uint16_t *)&task->datain.data[2]) + 4;
	default:
		return -1;
	}
}

/*
 * unmarshall the data in blob for inquiry into a structure
 */
static void *
scsi_inquiry_datain_unmarshall(struct scsi_task *task)
{
	if (task->params.inquiry.evpd == 0) {
		struct scsi_inquiry_standard *inq;

		/* standard inquiry */
		inq = scsi_malloc(task, sizeof(struct scsi_inquiry_standard));
		if (inq == NULL) {
			return NULL;
		}

		inq->periperal_qualifier    = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type  = task->datain.data[0]&0x1f;
		inq->rmb                    = !!(task->datain.data[1]&0x80);
		inq->version                = task->datain.data[2];
		inq->normaca                = !!(task->datain.data[3]&0x20);
		inq->hisup                  = !!(task->datain.data[3]&0x10);
		inq->response_data_format   = task->datain.data[3]&0x0f;

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

	if (task->params.inquiry.page_code
	    == SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES) {
		struct scsi_inquiry_supported_pages *inq;

		inq = scsi_malloc(task,
			   sizeof(struct scsi_inquiry_supported_pages));
		if (inq == NULL) {
			return NULL;
		}
		inq->periperal_qualifier   = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type = task->datain.data[0]&0x1f;
		inq->pagecode              = task->datain.data[1];

		inq->num_pages = task->datain.data[3];
		inq->pages = scsi_malloc(task, inq->num_pages);
		if (inq->pages == NULL) {
			return NULL;
		}
		memcpy(inq->pages, &task->datain.data[4], inq->num_pages);
		return inq;
	} else if (task->params.inquiry.page_code
		   == SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER) {
		struct scsi_inquiry_unit_serial_number *inq;

		inq = scsi_malloc(task,
			   sizeof(struct scsi_inquiry_unit_serial_number));
		if (inq == NULL) {
			return NULL;
		}
		inq->periperal_qualifier   = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type = task->datain.data[0]&0x1f;
		inq->pagecode              = task->datain.data[1];

		inq->usn = scsi_malloc(task, task->datain.data[3]+1);
		if (inq->usn == NULL) {
			return NULL;
		}
		memcpy(inq->usn, &task->datain.data[4], task->datain.data[3]);
		inq->usn[task->datain.data[3]] = 0;
		return inq;
	} else if (task->params.inquiry.page_code
		   == SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION) {
		struct scsi_inquiry_device_identification *inq;
		int remaining = ntohs(*(uint16_t *)&task->datain.data[2]);
		unsigned char *dptr;

		inq = scsi_malloc(task,
			   sizeof(struct scsi_inquiry_device_identification));
		if (inq == NULL) {
			return NULL;
		}
		inq->periperal_qualifier   = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type = task->datain.data[0]&0x1f;
		inq->pagecode              = task->datain.data[1];

		dptr = &task->datain.data[4];
		while (remaining > 0) {
			struct scsi_inquiry_device_designator *dev;

			dev = scsi_malloc(task,
			      sizeof(struct scsi_inquiry_device_designator));
			if (dev == NULL) {
				return NULL;
			}

			dev->next = inq->designators;
			inq->designators = dev;

			dev->protocol_identifier = (dptr[0]>>4) & 0x0f;
			dev->code_set            = dptr[0] & 0x0f;
			dev->piv                 = !!(dptr[1]&0x80);
			dev->association         = (dptr[1]>>4)&0x03;
			dev->designator_type     = dptr[1]&0x0f;

			dev->designator_length   = dptr[3];
			dev->designator          = scsi_malloc(task,
						   dev->designator_length+1);
			if (dev->designator == NULL) {
				return NULL;
			}
			dev->designator[dev->designator_length] = 0;
			memcpy(dev->designator, &dptr[4],
			       dev->designator_length);

			remaining -= 4;
			remaining -= dev->designator_length;

			dptr += dev->designator_length + 4;
		}
		return inq;
	} else if (task->params.inquiry.page_code
		   == SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS) {
		struct scsi_inquiry_block_limits *inq;

		inq = scsi_malloc(task,
		      sizeof(struct scsi_inquiry_block_limits));
		if (inq == NULL) {
			return NULL;
		}
		inq->periperal_qualifier   = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type = task->datain.data[0]&0x1f;
		inq->pagecode              = task->datain.data[1];

		inq->wsnz                  = task->datain.data[4] & 0x01;
		inq->max_cmp               = task->datain.data[5];
		inq->opt_gran              = ntohs(*(uint16_t *)&task->datain.data[6]);
		inq->max_xfer_len          = ntohl(*(uint32_t *)&task->datain.data[8]);
		inq->opt_xfer_len          = ntohl(*(uint32_t *)&task->datain.data[12]);
		inq->max_prefetch          = ntohl(*(uint32_t *)&task->datain.data[16]);
		inq->max_unmap             = ntohl(*(uint32_t *)&task->datain.data[20]);
		inq->max_unmap_bdc         = ntohl(*(uint32_t *)&task->datain.data[24]);
		inq->opt_unmap_gran        = ntohl(*(uint32_t *)&task->datain.data[28]);
		inq->ugavalid              = !!(task->datain.data[32]&0x80);
		inq->unmap_gran_align      = ntohl(*(uint32_t *)&task->datain.data[32]) & 0x7fffffff;
		inq->max_ws_len            = ntohl(*(uint32_t *)&task->datain.data[36]);
		inq->max_ws_len            = (inq->max_ws_len << 32) | ntohl(*(uint32_t *)&task->datain.data[40]);

		return inq;
	} else if (task->params.inquiry.page_code
		   == SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS) {
		struct scsi_inquiry_block_device_characteristics *inq;

		inq = scsi_malloc(task,
		      sizeof(struct scsi_inquiry_block_device_characteristics));
		if (inq == NULL) {
			return NULL;
		}
		inq->periperal_qualifier   = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type = task->datain.data[0]&0x1f;
		inq->pagecode              = task->datain.data[1];

		inq->medium_rotation_rate  = ntohs(*(uint16_t *)&task->datain.data[4]);
		return inq;
	} else if (task->params.inquiry.page_code
		   == SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING) {
		struct scsi_inquiry_logical_block_provisioning *inq;

		inq = scsi_malloc(task,
			   sizeof(struct scsi_inquiry_logical_block_provisioning));
		if (inq == NULL) {
			return NULL;
		}
		inq->periperal_qualifier   = (task->datain.data[0]>>5)&0x07;
		inq->periperal_device_type = task->datain.data[0]&0x1f;
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

	return NULL;
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

	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = xferlen;

	task->params.read6.lba        = lba;
	task->params.read6.num_blocks = num_blocks;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(xferlen/blocksize);

	task->cdb[6] |= (group_number & 0x1f);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.read10.lba        = lba;
	task->params.read10.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint32_t *)&task->cdb[6] = htonl(xferlen/blocksize);

	task->cdb[10] |= (group_number & 0x1f);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.read12.lba        = lba;
	task->params.read12.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_READ;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.read16.lba        = lba;
	task->params.read16.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(xferlen/blocksize);

	task->cdb[6] |= (group_number & 0x1f);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.write10.lba        = lba;
	task->params.write10.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint32_t *)&task->cdb[6] = htonl(xferlen/blocksize);

	task->cdb[10] |= (group_number & 0x1f);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.write12.lba        = lba;
	task->params.write12.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.write16.lba        = lba;
	task->params.write16.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.orwrite.lba        = lba;
	task->params.orwrite.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(lba & 0xffffffff);
	task->cdb[13] = xferlen/blocksize;

	task->cdb[14] |= (group_number & 0x1f);
	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
        }
	task->expxferlen = xferlen;

	task->params.compareandwrite.lba        = lba;
	task->params.compareandwrite.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(xferlen/blocksize);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.verify10.lba        = lba;
	task->params.verify10.num_blocks = xferlen/blocksize;
	task->params.verify10.vprotect   = vprotect;
	task->params.verify10.dpo        = dpo;
	task->params.verify10.bytchk     = bytchk;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint32_t *)&task->cdb[6] = htonl(xferlen/blocksize);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.verify12.lba        = lba;
	task->params.verify12.num_blocks = xferlen/blocksize;
	task->params.verify12.vprotect   = vprotect;
	task->params.verify12.dpo        = dpo;
	task->params.verify12.bytchk     = bytchk;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(xferlen/blocksize);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.verify16.lba        = lba;
	task->params.verify16.num_blocks = xferlen/blocksize;
	task->params.verify16.vprotect   = vprotect;
	task->params.verify16.dpo        = dpo;
	task->params.verify16.bytchk     = bytchk;

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

	*(uint16_t *)&task->cdb[7] = htons(xferlen);

	task->cdb_size = 10;
	task->xfer_dir = SCSI_XFER_WRITE;
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
	*(uint32_t *)&task->cdb[2] = htonl(lba);
	if (group) {
		task->cdb[6] |= (group & 0x1f);
	}
	*(uint16_t *)&task->cdb[7] = htons(num_blocks);

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
	*(uint32_t *)&task->cdb[2]  = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6]  = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(num_blocks);
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
	task->xfer_dir = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.modesense6.dbd           = dbd;
	task->params.modesense6.pc            = pc;
	task->params.modesense6.page_code     = page_code;
	task->params.modesense6.sub_page_code = sub_page_code;

	return task;
}

/*
 * parse the data in blob and calcualte the size of a full
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

	mp->caching.disable_prefetch_transfer_length = htons(*(uint16_t *)&(task->datain.data[pos+2]));
	mp->caching.minimum_prefetch = htons(*(uint16_t *)&(task->datain.data[pos+4]));
	mp->caching.maximum_prefetch = htons(*(uint16_t *)&(task->datain.data[pos+6]));
	mp->caching.maximum_prefetch_ceiling = htons(*(uint16_t *)&(task->datain.data[pos+8]));

	mp->caching.fsw    = task->datain.data[pos+10] & 0x80;
	mp->caching.lbcss  = task->datain.data[pos+10] & 0x40;
	mp->caching.dra    = task->datain.data[pos+10] & 0x20;
	mp->caching.nv_dis = task->datain.data[pos+10] & 0x01;

	mp->caching.number_of_cache_segments = task->datain.data[pos+11];
	mp->caching.cache_segment_size = htons(*(uint16_t *)&(task->datain.data[pos+12]));
}

static void
scsi_parse_mode_disconnect_reconnect(struct scsi_task *task, int pos, struct scsi_mode_page *mp)
{
	mp->disconnect_reconnect.buffer_full_ratio = task->datain.data[pos];
	mp->disconnect_reconnect.buffer_empty_ratio = task->datain.data[pos+1];
	mp->disconnect_reconnect.bus_inactivity_limit = htons(*(uint16_t *)&(task->datain.data[pos+2]));
	mp->disconnect_reconnect.disconnect_time_limit = htons(*(uint16_t *)&(task->datain.data[pos+4]));
	mp->disconnect_reconnect.connect_time_limit = htons(*(uint16_t *)&(task->datain.data[pos+6]));
	mp->disconnect_reconnect.maximum_burst_size = htons(*(uint16_t *)&(task->datain.data[pos+8]));
	mp->disconnect_reconnect.emdp = task->datain.data[pos+10] & 0x80;
	mp->disconnect_reconnect.fair_arbitration = (task->datain.data[pos+10]>>4) & 0x0f;
	mp->disconnect_reconnect.dimm = task->datain.data[pos+10] & 0x08;
	mp->disconnect_reconnect.dtdc = task->datain.data[pos+10] & 0x07;
	mp->disconnect_reconnect.first_burst_size = htons(*(uint16_t *)&(task->datain.data[pos+12]));
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
	mp->iec.interval_timer = htonl(*(uint32_t *)&(task->datain.data[pos+2]));
	mp->iec.report_count   = htonl(*(uint32_t *)&(task->datain.data[pos+6]));
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
			mp->len = ntohs(*(uint16_t *)&task->datain.data[pos]);
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

	task->params.startstopunit.immed    = immed;
	task->params.startstopunit.pcm      = pcm;
	task->params.startstopunit.pc       = pc;
	task->params.startstopunit.no_flush = no_flush;
	task->params.startstopunit.loej     = loej;
	task->params.startstopunit.start    = start;

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

	task->params.preventallow.prevent = prevent;

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
	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(num_blocks);

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
	*(uint32_t *)&task->cdb[2]  = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6]  = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(num_blocks);

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
	*(uint32_t *)&task->cdb[2] = htonl(lba);
	task->cdb[6] |= group & 0x1f;
	*(uint16_t *)&task->cdb[7] = htons(num_blocks);

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
	*(uint32_t *)&task->cdb[2]  = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6]  = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(num_blocks);

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

	*(uint32_t *)&task->cdb[10] = htonl(xferlen);

	task->cdb_size   = 16;
	task->xfer_dir   = SCSI_XFER_READ;
	task->expxferlen = xferlen;

	task->params.serviceactionin.sa = sa;

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

	*(uint32_t *)&task->cdb[2] = htonl(starting_lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(starting_lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(alloc_len);

	task->cdb_size   = 16;
	task->xfer_dir   = SCSI_XFER_READ;
	task->expxferlen = alloc_len;

	task->params.serviceactionin.sa = SCSI_GET_LBA_STATUS;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint16_t *)&task->cdb[7] = htons(xferlen/blocksize);

	task->cdb[6] |= (group_number & 0x1f);

	task->cdb_size = 10;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.writeverify10.lba        = lba;
	task->params.writeverify10.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba);
	*(uint32_t *)&task->cdb[6] = htonl(xferlen/blocksize);

	task->cdb[10] |= (group_number & 0x1f);

	task->cdb_size = 12;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.writeverify12.lba        = lba;
	task->params.writeverify12.num_blocks = xferlen/blocksize;

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

	*(uint32_t *)&task->cdb[2] = htonl(lba >> 32);
	*(uint32_t *)&task->cdb[6] = htonl(lba & 0xffffffff);
	*(uint32_t *)&task->cdb[10] = htonl(xferlen/blocksize);

	task->cdb[14] |= (group_number & 0x1f);

	task->cdb_size = 16;
	if (xferlen != 0) {
		task->xfer_dir = SCSI_XFER_WRITE;
	} else {
		task->xfer_dir = SCSI_XFER_NONE;
	}
	task->expxferlen = xferlen;

	task->params.writeverify16.lba        = lba;
	task->params.writeverify16.num_blocks = xferlen/blocksize;

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
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_getfullsize(task);
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
	case SCSI_OPCODE_REPORTLUNS:
		return scsi_reportluns_datain_unmarshall(task);
	case SCSI_OPCODE_SERVICE_ACTION_IN:
		return scsi_serviceactionin_datain_unmarshall(task);
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



struct scsi_data_buffer {
       struct scsi_data_buffer *next;
       uint32_t len;
       unsigned char *data;
};

int
scsi_task_add_data_in_buffer(struct scsi_task *task, int len, unsigned char *buf)
{
	struct scsi_data_buffer *data_buf;

	if (len < 0) {
		return -1;
	}
	data_buf = scsi_malloc(task, sizeof(struct scsi_data_buffer));
	if (data_buf == NULL) {
		return -1;
	}

	data_buf->len  = len;
	data_buf->data = buf;

	SLIST_ADD_END(&task->in_buffers, data_buf);
	return 0;
}

unsigned char *
scsi_task_get_data_in_buffer(struct scsi_task *task, uint32_t pos, ssize_t *count)
{
	struct scsi_data_buffer *sdb;

	sdb = task->in_buffers;
	if (sdb == NULL) {
		return NULL;
	}

	while (pos >= sdb->len) {
		pos -= sdb->len;
		sdb  = sdb->next;
		if (sdb == NULL) {
			/* someone issued a read but did not provide enough user buffers for all the data.
			 * maybe someone tried to read just 512 bytes off a MMC device?
			 */
			return NULL;			
		}
	}

	if (count && *count > (ssize_t)(sdb->len - pos)) {
		*count = sdb->len - pos;
	}

	return &sdb->data[pos];
}

int
iscsi_scsi_task_cancel(struct iscsi_context *iscsi,
		  struct scsi_task *task)
{
	struct iscsi_pdu *pdu;

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		if (pdu->itt == task->itt) {
			while(task->in_buffers != NULL) {
				struct scsi_data_buffer *ptr = task->in_buffers;
				SLIST_REMOVE(&task->in_buffers, ptr);
			}
			SLIST_REMOVE(&iscsi->waitpdu, pdu);
			if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
			}
			iscsi_free_pdu(iscsi, pdu);
			return 0;
		}
	}
	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		if (pdu->itt == task->itt) {
			while(task->in_buffers != NULL) {
				struct scsi_data_buffer *ptr = task->in_buffers;
				SLIST_REMOVE(&task->in_buffers, ptr);
			}
			SLIST_REMOVE(&iscsi->outqueue, pdu);
			if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
				pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
			}
			iscsi_free_pdu(iscsi, pdu);
			return 0;
		}
	}
	return -1;
}

void
iscsi_scsi_cancel_all_tasks(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		struct scsi_task *task = iscsi_scsi_get_task_from_pdu(pdu);
	
		while(task->in_buffers != NULL) {
			struct scsi_data_buffer *ptr = task->in_buffers;
			SLIST_REMOVE(&task->in_buffers, ptr);
		}
		SLIST_REMOVE(&iscsi->waitpdu, pdu);
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}
	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		struct scsi_task *task = iscsi_scsi_get_task_from_pdu(pdu);

		while(task->in_buffers != NULL) {
			struct scsi_data_buffer *ptr = task->in_buffers;
			SLIST_REMOVE(&task->in_buffers, ptr);
		}
		SLIST_REMOVE(&iscsi->outqueue, pdu);
		if ( !(pdu->flags & ISCSI_PDU_NO_CALLBACK)) {
			pdu->callback(iscsi, SCSI_STATUS_CANCELLED, NULL,
				      pdu->private_data);
		}
		iscsi_free_pdu(iscsi, pdu);
	}
}
