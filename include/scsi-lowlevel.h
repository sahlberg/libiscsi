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
#ifndef __scsi_lowlevel_h__
#define __scsi_lowlevel_h__

#if defined(WIN32)
#define EXTERN __declspec( dllexport )
#else
#define EXTERN
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define SCSI_CDB_MAX_SIZE			16

enum scsi_opcode {
	SCSI_OPCODE_TESTUNITREADY      = 0x00,
	SCSI_OPCODE_READ6              = 0x08,
	SCSI_OPCODE_INQUIRY            = 0x12,
	SCSI_OPCODE_RESERVE6           = 0x16,
	SCSI_OPCODE_RELEASE6           = 0x17,
	SCSI_OPCODE_MODESENSE6         = 0x1a,
	SCSI_OPCODE_STARTSTOPUNIT      = 0x1b,
	SCSI_OPCODE_PREVENTALLOW       = 0x1e,
	SCSI_OPCODE_READCAPACITY10     = 0x25,
	SCSI_OPCODE_READ10             = 0x28,
	SCSI_OPCODE_WRITE10            = 0x2A,
	SCSI_OPCODE_WRITE_VERIFY10     = 0x2E,
	SCSI_OPCODE_VERIFY10           = 0x2F,
	SCSI_OPCODE_PREFETCH10         = 0x34,
	SCSI_OPCODE_SYNCHRONIZECACHE10 = 0x35,
	SCSI_OPCODE_WRITE_SAME10       = 0x41,
	SCSI_OPCODE_UNMAP              = 0x42,
	SCSI_OPCODE_READTOC            = 0x43,
	SCSI_OPCODE_READ16             = 0x88,
	SCSI_OPCODE_COMPARE_AND_WRITE  = 0x89,
	SCSI_OPCODE_WRITE16            = 0x8A,
	SCSI_OPCODE_ORWRITE            = 0x8B,
	SCSI_OPCODE_WRITE_VERIFY16     = 0x8E,
	SCSI_OPCODE_VERIFY16           = 0x8F,
	SCSI_OPCODE_PREFETCH16         = 0x90,
	SCSI_OPCODE_SYNCHRONIZECACHE16 = 0x91,
	SCSI_OPCODE_WRITE_SAME16       = 0x93,
	SCSI_OPCODE_SERVICE_ACTION_IN  = 0x9E,
	SCSI_OPCODE_REPORTLUNS         = 0xA0,
	SCSI_OPCODE_MAINTENANCE_IN     = 0xA3,
	SCSI_OPCODE_READ12             = 0xA8,
	SCSI_OPCODE_WRITE12            = 0xAA,
	SCSI_OPCODE_WRITE_VERIFY12     = 0xAE,
	SCSI_OPCODE_VERIFY12           = 0xAF
};

enum scsi_service_action_in {
	SCSI_READCAPACITY16            = 0x10,
	SCSI_GET_LBA_STATUS            = 0x12
};

enum scsi_maintenance_in {
	SCSI_REPORT_SUPPORTED_OP_CODES = 0x0c
};

enum scsi_op_code_reporting_options {
	SCSI_REPORT_SUPPORTING_OPS_ALL       = 0x00,
	SCSI_REPORT_SUPPORTING_OPCODE        = 0x01,
	SCSI_REPORT_SUPPORTING_SERVICEACTION = 0x02
};

/* sense keys */
enum scsi_sense_key {
	SCSI_SENSE_NO_SENSE            = 0x00,
	SCSI_SENSE_RECOVERED_ERROR     = 0x01,
	SCSI_SENSE_NOT_READY           = 0x02,
	SCSI_SENSE_MEDIUM_ERROR        = 0x03,
	SCSI_SENSE_HARDWARE_ERROR      = 0x04,
	SCSI_SENSE_ILLEGAL_REQUEST     = 0x05,
	SCSI_SENSE_UNIT_ATTENTION      = 0x06,
	SCSI_SENSE_DATA_PROTECTION     = 0x07,
	SCSI_SENSE_BLANK_CHECK         = 0x08,
	SCSI_SENSE_VENDOR_SPECIFIC     = 0x09,
	SCSI_SENSE_COPY_ABORTED        = 0x0a,
	SCSI_SENSE_COMMAND_ABORTED     = 0x0b,
	SCSI_SENSE_OBSOLETE_ERROR_CODE = 0x0c,
	SCSI_SENSE_OVERFLOW_COMMAND    = 0x0d,
	SCSI_SENSE_MISCOMPARE          = 0x0e
};

EXTERN const char *scsi_sense_key_str(int key);

/* ascq */
#define SCSI_SENSE_ASCQ_MISCOMPARE_DURING_VERIFY	0x1d00
#define SCSI_SENSE_ASCQ_INVALID_OPERATION_CODE		0x2000
#define SCSI_SENSE_ASCQ_LBA_OUT_OF_RANGE		0x2100
#define SCSI_SENSE_ASCQ_INVALID_FIELD_IN_CDB		0x2400
#define SCSI_SENSE_ASCQ_LOGICAL_UNIT_NOT_SUPPORTED	0x2500
#define SCSI_SENSE_ASCQ_WRITE_PROTECTED			0x2700
#define SCSI_SENSE_ASCQ_BUS_RESET			0x2900
#define SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT		0x3a00
#define SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_CLOSED	0x3a01
#define SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT_TRAY_OPEN	0x3a02
#define SCSI_SENSE_ASCQ_INTERNAL_TARGET_FAILURE	        0x4400
#define SCSI_SENSE_ASCQ_MEDIUM_LOAD_OR_EJECT_FAILED     0x5300
#define SCSI_SENSE_ASCQ_MEDIUM_REMOVAL_PREVENTED        0x5302

EXTERN const char *scsi_sense_ascq_str(int ascq);


enum scsi_xfer_dir {
	SCSI_XFER_NONE  = 0,
	SCSI_XFER_READ  = 1,
	SCSI_XFER_WRITE = 2
};

/*
 * READTOC
 */
EXTERN struct scsi_task *scsi_cdb_readtoc(int msf, int format, int track_session, uint16_t alloc_len);

enum scsi_readtoc_fmt {
	SCSI_READ_TOC          = 0,
	SCSI_READ_SESSION_INFO = 1,
	SCSI_READ_FULL_TOC     = 2,
	SCSI_READ_PMA          = 3,
	SCSI_READ_ATIP         = 4
};
struct scsi_readtoc_desc{
	union {
		struct scsi_toc_desc {
			int adr;
			int control;
			int track;
			uint32_t lba;
		} toc;
		struct scsi_session_desc {
			int adr;
			int control;
			int first_in_last;
			uint32_t lba;
		} ses;
		struct scsi_fulltoc_desc {
			int session;
			int adr;
			int control;
			int tno;
			int point;
			int min;
			int sec;
			int frame;
			int zero;
			int pmin;
			int psec;
			int pframe;
		} full;
	} desc;
};

struct scsi_readtoc_list {
	int num;
	int first;
	int last;
	struct scsi_readtoc_desc desc[0];
};

struct scsi_compareandwrite_params {
	uint64_t lba;
	uint32_t num_blocks;
};
struct scsi_writeverify10_params {
	uint32_t lba;
	uint32_t num_blocks;
};
struct scsi_writeverify12_params {
	uint32_t lba;
	uint32_t num_blocks;
};
struct scsi_writeverify16_params {
	uint64_t lba;
	uint32_t num_blocks;
};
struct scsi_verify10_params {
	uint32_t lba;
	uint32_t num_blocks;
	int vprotect;
	int dpo;
	int bytchk;
};
struct scsi_verify12_params {
	uint32_t lba;
	uint32_t num_blocks;
	int vprotect;
	int dpo;
	int bytchk;
};
struct scsi_verify16_params {
	uint64_t lba;
	uint32_t num_blocks;
	int vprotect;
	int dpo;
	int bytchk;
};
struct scsi_readcapacity10_params {
	int lba;
	int pmi;
};
struct scsi_inquiry_params {
	int evpd;
	int page_code;
};
struct scsi_modesense6_params {
	int dbd;
	int pc;
	int page_code;
	int sub_page_code;
};
struct scsi_serviceactionin_params {
	enum scsi_service_action_in sa;
};
struct scsi_readtoc_params {
	int msf;
	int format;
	int track_session;
};


struct scsi_report_supported_params {
	int return_timeouts;
};

struct scsi_maintenancein_params {
	enum scsi_maintenance_in sa;
	union {
		struct scsi_report_supported_params reportsupported;
	} params;
};

struct scsi_sense {
	unsigned char       error_type;
	enum scsi_sense_key key;
	int                 ascq;
};

struct scsi_data {
	int            size;
	unsigned char *data;
};

enum scsi_residual {
	SCSI_RESIDUAL_NO_RESIDUAL = 0,
	SCSI_RESIDUAL_UNDERFLOW,
	SCSI_RESIDUAL_OVERFLOW
};

struct scsi_task {
	int status;

	int cdb_size;
	int xfer_dir;
	int expxferlen;
	unsigned char cdb[SCSI_CDB_MAX_SIZE];
	union {
		struct scsi_compareandwrite_params compareandwrite;
		struct scsi_writeverify10_params   writeverify10;
		struct scsi_writeverify12_params   writeverify12;
		struct scsi_writeverify16_params   writeverify16;
		struct scsi_verify10_params        verify10;
		struct scsi_verify12_params        verify12;
		struct scsi_verify16_params        verify16;
		struct scsi_readcapacity10_params  readcapacity10;
		struct scsi_inquiry_params         inquiry;
		struct scsi_modesense6_params      modesense6;
		struct scsi_serviceactionin_params serviceactionin;
		struct scsi_readtoc_params         readtoc;
		struct scsi_maintenancein_params   maintenancein;
	} params;

	enum scsi_residual residual_status;
	int residual;
	struct scsi_sense sense;
	struct scsi_data datain;
	struct scsi_allocated_memory *mem;

	void *ptr;

	uint32_t itt;
	uint32_t cmdsn;
	uint32_t lun;

	struct scsi_data_buffer *in_buffers;
};

/* This function will free a scsi task structure.
   You may NOT cancel a task until the callback has been invoked
   and the command has completed on the transport layer.
*/
EXTERN void scsi_free_scsi_task(struct scsi_task *task);

EXTERN void scsi_set_task_private_ptr(struct scsi_task *task, void *ptr);
EXTERN void *scsi_get_task_private_ptr(struct scsi_task *task);

/*
 * TESTUNITREADY
 */
EXTERN struct scsi_task *scsi_cdb_testunitready(void);


/*
 * REPORTLUNS
 */
#define SCSI_REPORTLUNS_REPORT_ALL_LUNS				0x00
#define SCSI_REPORTLUNS_REPORT_WELL_KNOWN_ONLY			0x01
#define SCSI_REPORTLUNS_REPORT_AVAILABLE_LUNS_ONLY		0x02

struct scsi_reportluns_list {
	uint32_t num;
	uint16_t luns[0];
};

EXTERN struct scsi_task *scsi_reportluns_cdb(int report_type, int alloc_len);

/*
 * RESERVE6
 */
EXTERN struct scsi_task *scsi_cdb_reserve6(void);
/*
 * RELEASE6
 */
EXTERN struct scsi_task *scsi_cdb_release6(void);

/*
 * READCAPACITY10
 */
struct scsi_readcapacity10 {
	uint32_t lba;
	uint32_t block_size;
};
EXTERN struct scsi_task *scsi_cdb_readcapacity10(int lba, int pmi);


/*
 * INQUIRY
 */
enum scsi_inquiry_peripheral_qualifier {
	SCSI_INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED     = 0x00,
	SCSI_INQUIRY_PERIPHERAL_QUALIFIER_DISCONNECTED  = 0x01,
	SCSI_INQUIRY_PERIPHERAL_QUALIFIER_NOT_SUPPORTED = 0x03
};

const char *scsi_devqualifier_to_str(
			enum scsi_inquiry_peripheral_qualifier qualifier);

enum scsi_inquiry_peripheral_device_type {
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS            = 0x00,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQUENTIAL_ACCESS        = 0x01,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_PRINTER                  = 0x02,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_PROCESSOR                = 0x03,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_WRITE_ONCE               = 0x04,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MMC                      = 0x05,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SCANNER                  = 0x06,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OPTICAL_MEMORY           = 0x07,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_MEDIA_CHANGER            = 0x08,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_COMMUNICATIONS           = 0x09,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_STORAGE_ARRAY_CONTROLLER = 0x0c,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_ENCLOSURE_SERVICES       = 0x0d,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SIMPLIFIED_DIRECT_ACCESS = 0x0e,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OPTICAL_CARD_READER      = 0x0f,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_BRIDGE_CONTROLLER        = 0x10,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_OSD                      = 0x11,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_AUTOMATION               = 0x12,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_SEQURITY_MANAGER         = 0x13,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_WELL_KNOWN_LUN           = 0x1e,
	SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_UNKNOWN                  = 0x1f
};

EXTERN const char *scsi_devtype_to_str(enum scsi_inquiry_peripheral_device_type type);

enum scsi_version {
	SCSI_VERSION_SPC  = 0x03,
	SCSI_VERSION_SPC2 = 0x04,
	SCSI_VERSION_SPC3 = 0x05
};

EXTERN const char *scsi_version_to_str(enum scsi_version version);

enum scsi_inquiry_tpgs {
	SCSI_INQUIRY_TPGS_NO_SUPPORT            = 0x00,
	SCSI_INQUIRY_TPGS_IMPLICIT              = 0x01,
	SCSI_INQUIRY_TPGS_EXPLICIT              = 0x02,
	SCSI_INQUIRY_TPGS_IMPLICIT_AND_EXPLICIT = 0x03
};

/* fix typos, leave old names for backward compatibility */
#define periperal_qualifier qualifier
#define periperal_device_type device_type

struct scsi_inquiry_standard {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	int rmb;
	int version;
	int normaca;
	int hisup;
	int response_data_format;

	int additional_length;

	int sccs;
	int acc;
	int tpgs;
	int threepc;
	int protect;

	int encserv;
	int multip;
	int addr16;
	int wbus16;
	int sync;
	int cmdque;

	int clocking;
	int qas;
	int ius;

	char vendor_identification[8+1];
	char product_identification[16+1];
	char product_revision_level[4+1];
};

enum scsi_inquiry_pagecode {
	SCSI_INQUIRY_PAGECODE_SUPPORTED_VPD_PAGES          = 0x00,
	SCSI_INQUIRY_PAGECODE_UNIT_SERIAL_NUMBER           = 0x80,
	SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION        = 0x83,
	SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS                 = 0xB0,
	SCSI_INQUIRY_PAGECODE_BLOCK_DEVICE_CHARACTERISTICS = 0xB1,
	SCSI_INQUIRY_PAGECODE_LOGICAL_BLOCK_PROVISIONING   = 0xB2
};

EXTERN const char *scsi_inquiry_pagecode_to_str(int pagecode);

struct scsi_inquiry_supported_pages {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	enum scsi_inquiry_pagecode pagecode;

	int num_pages;
	unsigned char *pages;
};

struct scsi_inquiry_block_limits {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	enum scsi_inquiry_pagecode pagecode;

	int wsnz;		   		/* write same no zero */
	uint8_t max_cmp;			/* maximum_compare_and_write_length */
	uint16_t opt_gran;			/* optimal_transfer_length_granularity */
	uint32_t max_xfer_len;			/* maximum_transfer_length */
	uint32_t opt_xfer_len;			/* optimal_transfer_length */
	uint32_t max_prefetch;			/* maximum_prefetched_xdread_xdwrite_transfer_length */
	uint32_t max_unmap;			/* maximum_unmap_lba_count */
	uint32_t max_unmap_bdc;			/* maximum_unmap_block_descriptor_count */
	uint32_t opt_unmap_gran;		/* optimal_unmap_granularity */
	int ugavalid;
	uint32_t unmap_gran_align;		/* unmap_granularity_alignment */
	uint64_t max_ws_len;			/* maximum_write_same_length */
};

struct scsi_inquiry_block_device_characteristics {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	enum scsi_inquiry_pagecode pagecode;

	int medium_rotation_rate;
};

enum scsi_inquiry_provisioning_type {
	PROVISIONING_TYPE_NONE     = 0,
	PROVISIONING_TYPE_RESOURCE = 1,
	PROVISIONING_TYPE_THIN     = 2
};

struct scsi_inquiry_logical_block_provisioning {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	enum scsi_inquiry_pagecode pagecode;

       int threshold_exponent;
       int lbpu;
       int lbpws;
       int lbpws10;
       int lbprz;
       int anc_sup;
       int dp;
       enum scsi_inquiry_provisioning_type provisioning_type;
};

EXTERN struct scsi_task *scsi_cdb_inquiry(int evpd, int page_code, int alloc_len);

struct scsi_inquiry_unit_serial_number {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	enum scsi_inquiry_pagecode pagecode;

	char *usn;
};

enum scsi_protocol_identifier {
	SCSI_PROTOCOL_IDENTIFIER_FIBRE_CHANNEL = 0x00,
	SCSI_PROTOCOL_IDENTIFIER_PARALLEL_SCSI = 0x01,
	SCSI_PROTOCOL_IDENTIFIER_SSA           = 0x02,
	SCSI_PROTOCOL_IDENTIFIER_IEEE_1394     = 0x03,
	SCSI_PROTOCOL_IDENTIFIER_RDMA          = 0x04,
	SCSI_PROTOCOL_IDENTIFIER_ISCSI         = 0x05,
	SCSI_PROTOCOL_IDENTIFIER_SAS           = 0x06,
	SCSI_PROTOCOL_IDENTIFIER_ADT           = 0x07,
	SCSI_PROTOCOL_IDENTIFIER_ATA           = 0x08
};

EXTERN const char *scsi_protocol_identifier_to_str(int identifier);

enum scsi_codeset {
	SCSI_CODESET_BINARY = 0x01,
	SCSI_CODESET_ASCII  = 0x02,
	SCSI_CODESET_UTF8   = 0x03
};

EXTERN const char *scsi_codeset_to_str(int codeset);

enum scsi_association {
	SCSI_ASSOCIATION_LOGICAL_UNIT  = 0x00,
	SCSI_ASSOCIATION_TARGET_PORT   = 0x01,
	SCSI_ASSOCIATION_TARGET_DEVICE = 0x02
};

EXTERN const char *scsi_association_to_str(int association);

enum scsi_designator_type {
	SCSI_DESIGNATOR_TYPE_VENDOR_SPECIFIC             = 0x00,
	SCSI_DESIGNATOR_TYPE_T10_VENDORT_ID              = 0x01,
	SCSI_DESIGNATOR_TYPE_EUI_64                      = 0x02,
	SCSI_DESIGNATOR_TYPE_NAA                         = 0x03,
	SCSI_DESIGNATOR_TYPE_RELATIVE_TARGET_PORT        = 0x04,
	SCSI_DESIGNATOR_TYPE_TARGET_PORT_GROUP           = 0x05,
	SCSI_DESIGNATOR_TYPE_LOGICAL_UNIT_GROUP          = 0x06,
	SCSI_DESIGNATOR_TYPE_MD5_LOGICAL_UNIT_IDENTIFIER = 0x07,
	SCSI_DESIGNATOR_TYPE_SCSI_NAME_STRING            = 0x08
};

EXTERN const char *scsi_designator_type_to_str(int association);

struct scsi_inquiry_device_designator {
	struct scsi_inquiry_device_designator *next;

	enum scsi_protocol_identifier protocol_identifier;
	enum scsi_codeset code_set;
	int piv;
	enum scsi_association association;
	enum scsi_designator_type designator_type;
	int designator_length;
	char *designator;
};

struct scsi_inquiry_device_identification {
	enum scsi_inquiry_peripheral_qualifier qualifier;
	enum scsi_inquiry_peripheral_device_type device_type;
	enum scsi_inquiry_pagecode pagecode;

	struct scsi_inquiry_device_designator *designators;
};

/*
 * MODESENSE6
 */
enum scsi_modesense_page_control {
	SCSI_MODESENSE_PC_CURRENT    = 0x00,
	SCSI_MODESENSE_PC_CHANGEABLE = 0x01,
	SCSI_MODESENSE_PC_DEFAULT    = 0x02,
	SCSI_MODESENSE_PC_SAVED      = 0x03
};

struct scsi_mode_page_caching {
	int ic;
	int abpf;
	int cap;
	int disc;
	int size;
	int wce;
	int mf;
	int rcd;

	int demand_read_retention_priority;
	int write_retention_priority;

	int disable_prefetch_transfer_length;
	int minimum_prefetch;
	int maximum_prefetch;
	int maximum_prefetch_ceiling;

	int fsw;
	int lbcss;
	int dra;
	int nv_dis;

	int number_of_cache_segments;
	int cache_segment_size;
};

struct scsi_mode_page_disconnect_reconnect {
	int buffer_full_ratio;
	int buffer_empty_ratio;
	int bus_inactivity_limit;
	int disconnect_time_limit;
	int connect_time_limit;
	int maximum_burst_size;
	int emdp;
	int fair_arbitration;
	int dimm;
	int dtdc;
	int first_burst_size;
};

struct scsi_mode_page_informational_exceptions_control {
	int perf;
	int ebf;
	int ewasc;
	int dexcpt;
	int test;
	int ebackerr;
	int logerr;
	int mrie;
	int interval_timer;
	int report_count;
};

enum scsi_modesense_page_code {
	SCSI_MODESENSE_PAGECODE_READ_WRITE_ERROR_RECOVERY = 0x01,
	SCSI_MODESENSE_PAGECODE_DISCONNECT_RECONNECT      = 0x02,
	SCSI_MODESENSE_PAGECODE_VERIFY_ERROR_RECOVERY     = 0x07,
	SCSI_MODESENSE_PAGECODE_CACHING                   = 0x08,
	SCSI_MODESENSE_PAGECODE_XOR_CONTROL               = 0x10,
	SCSI_MODESENSE_PAGECODE_INFORMATIONAL_EXCEPTIONS_CONTROL        = 0x1c,
	SCSI_MODESENSE_PAGECODE_RETURN_ALL_PAGES          = 0x3f
};

struct scsi_mode_page {
       struct scsi_mode_page *next;
       int ps;
       int spf;
       enum scsi_modesense_page_code page_code;
       int subpage_code;
       int len;
       union {
              struct scsi_mode_page_caching caching;
              struct scsi_mode_page_disconnect_reconnect disconnect_reconnect;
	      struct scsi_mode_page_informational_exceptions_control iec;
       };
};

struct scsi_mode_sense {
       uint8_t mode_data_length;
       uint8_t medium_type;
       uint8_t device_specific_parameter;
       uint8_t block_descriptor_length;
       struct scsi_mode_page *pages;
};

EXTERN struct scsi_task *scsi_cdb_modesense6(int dbd,
			enum scsi_modesense_page_control pc,
			enum scsi_modesense_page_code page_code,
			int sub_page_code,
			unsigned char alloc_len);




struct scsi_readcapacity16 {
       uint64_t returned_lba;
       uint32_t block_length;
       uint8_t  p_type;
       uint8_t  prot_en;
       uint8_t  p_i_exp;
       uint8_t  lbppbe;
       uint8_t  lbpme;
       uint8_t  lbprz;
       uint16_t lalba;
};

enum scsi_provisioning_type {
     SCSI_PROVISIONING_TYPE_MAPPED	= 0x00,
     SCSI_PROVISIONING_TYPE_DEALLOCATED	= 0x01,
     SCSI_PROVISIONING_TYPE_ANCHORED	= 0x02
};

struct scsi_lba_status_descriptor {
       uint64_t	lba;
       uint32_t num_blocks;
       enum scsi_provisioning_type provisioning;
};

struct scsi_get_lba_status {
       uint32_t num_descriptors;
       struct scsi_lba_status_descriptor *descriptors;
};


struct scsi_op_timeout_descriptor {
	uint16_t descriptor_length;
	uint8_t reserved;
	uint8_t command_specific;
	uint32_t nominal_processing_timeout;
	uint32_t recommended_timeout;

};
struct scsi_command_descriptor {
	uint8_t op_code;
	uint8_t reserved1;
	uint16_t service_action;
	uint8_t reserved2;
	uint8_t reserved3;
	uint16_t cdb_length;
	struct scsi_op_timeout_descriptor to[0];
};

struct scsi_report_supported_op_codes {
	uint32_t num_descriptors;
	struct scsi_command_descriptor descriptors[0];
};

EXTERN int scsi_datain_getfullsize(struct scsi_task *task);
EXTERN void *scsi_datain_unmarshall(struct scsi_task *task);

EXTERN struct scsi_task *scsi_cdb_read6(uint32_t lba, uint32_t xferlen, int blocksize);
EXTERN struct scsi_task *scsi_cdb_read10(uint32_t lba, uint32_t xferlen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_read12(uint32_t lba, uint32_t xferlen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_read16(uint64_t lba, uint32_t xferlen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_write10(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_write12(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_write16(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_startstopunit(int immed, int pcm, int pc, int no_flush, int loej, int start);
EXTERN struct scsi_task *scsi_cdb_preventallow(int prevent);
EXTERN struct scsi_task *scsi_cdb_orwrite(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_compareandwrite(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group_number);
EXTERN struct scsi_task *scsi_cdb_writeverify10(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int bytchk, int group_number);
EXTERN struct scsi_task *scsi_cdb_writeverify12(uint32_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int bytchk, int group_number);
EXTERN struct scsi_task *scsi_cdb_writeverify16(uint64_t lba, uint32_t xferlen, int blocksize, int wrprotect, int dpo, int bytchk, int group_number);
EXTERN struct scsi_task *scsi_cdb_verify10(uint32_t lba, uint32_t xferlen, int vprotect, int dpo, int bytchk, int blocksize);
EXTERN struct scsi_task *scsi_cdb_verify12(uint32_t lba, uint32_t xferlen, int vprotect, int dpo, int bytchk, int blocksize);
EXTERN struct scsi_task *scsi_cdb_verify16(uint64_t lba, uint32_t xferlen, int vprotect, int dpo, int bytchk, int blocksize);

EXTERN struct scsi_task *scsi_cdb_synchronizecache10(int lba, int num_blocks,
			int syncnv, int immed);
EXTERN struct scsi_task *scsi_cdb_synchronizecache16(uint64_t lba, uint32_t num_blocks,
			int syncnv, int immed);
EXTERN struct scsi_task *scsi_cdb_serviceactionin16(enum scsi_service_action_in sa, uint32_t xferlen);
EXTERN struct scsi_task *scsi_cdb_readcapacity16(void);
EXTERN struct scsi_task *scsi_cdb_get_lba_status(uint64_t starting_lba, uint32_t alloc_len);
EXTERN struct scsi_task *scsi_cdb_unmap(int anchor, int group, uint16_t xferlen);
EXTERN struct scsi_task *scsi_cdb_writesame10(int wrprotect, int anchor, int unmap, int pbdata, int lbdata, uint32_t lba, int group, uint16_t num_blocks);
EXTERN struct scsi_task *scsi_cdb_writesame16(int wrprotect, int anchor, int unmap, int pbdata, int lbdata, uint64_t lba, int group, uint32_t num_blocks);
EXTERN struct scsi_task *scsi_cdb_prefetch10(uint32_t lba, int num_blocks, int immed, int group);
EXTERN struct scsi_task *scsi_cdb_prefetch16(uint64_t lba, int num_blocks, int immed, int group);
EXTERN struct scsi_task *scsi_cdb_report_supported_opcodes(int report_timeouts, uint32_t alloc_len);

void *scsi_malloc(struct scsi_task *task, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __scsi_lowlevel_h__ */
