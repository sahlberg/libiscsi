/*
   iscsi-test tool support

   Copyright (C) 2012 by Lee Duncan <leeman.duncan@gmail.com>

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

#ifndef	_ISCSI_SUPPORT_H_
#define	_ISCSI_SUPPORT_H_

#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

extern const char *initiatorname1;
extern const char *initiatorname2;
extern const char *tgt_url;

extern int loglevel;
#define LOG_SILENT  0
#define LOG_NORMAL  1
#define LOG_VERBOSE 2
void logging(int level, const char *format, ...) _R_(2,3);

/*
 * define special flags for logging a blank line, so compiler
 * does not commplain when logging a ""
 */
#define	LOG_BLANK_LINE " "
#define LOG_BLANK_LINE_CMP_LEN 2

#define CHECK_FOR_DATALOSS						\
do {									\
	if (!data_loss) {						\
		logging(LOG_NORMAL, "[SKIPPED] --dataloss flag is not " \
				"set. Skipping test.");	       	      	\
		CU_PASS("[SKIPPED] --dataloss flag is not set."		\
			" Skipping test");				\
		return;							\
	}								\
} while (0);

#define CHECK_FOR_SANITIZE						\
do {									\
	if (!allow_sanitize) {						\
		logging(LOG_NORMAL, "[SKIPPED] --allow-sanitize flag " \
			"is not set. Skipping test.");	       	      	\
		CU_PASS("[SKIPPED] --allow-sanitize flag is not set."	\
			" Skipping test");				\
		return;							\
	}								\
} while (0);

#define CHECK_FOR_READONLY						\
do {									\
	if (!readonly) {						\
		logging(LOG_NORMAL, "[SKIPPED] Logical unit is not "	\
				"write-protected. Skipping test.");	\
		CU_PASS("[SKIPPED] Logical unit is not write-"		\
				   "protected. Skipping test");		\
		return;							\
	}								\
} while (0);

#define CHECK_FOR_REMOVABLE						\
do {									\
	if (!inq->rmb) {						\
		logging(LOG_NORMAL, "[SKIPPED] Logical unit is not "	\
			"removable. Skipping test.");			\
		CU_PASS("[SKIPPED] Logical unit is not removable"	\
			" Skipping test");				\
		return;							\
	}								\
} while (0);

#define CHECK_FOR_THIN_PROVISIONING					\
do {									\
	if (rc16 == NULL || rc16->lbpme == 0) {	       		       	\
		logging(LOG_NORMAL, "[SKIPPED] Logical unit is fully"	\
			" provisioned. Skipping test");			\
		CU_PASS("[SKIPPED] Logical unit is fully provisioned."	\
			" Skipping test");				\
		return;	  	   					\
	}								\
} while (0);

#define CHECK_FOR_LBPWS10						\
do {									\
	if (inq_lbp->lbpws10 == 0) {	       				\
		logging(LOG_NORMAL, "[SKIPPED] Logical unit does not"	\
			" have LBPWS10. Skipping test");		\
		CU_PASS("[SKIPPED] Logical unit does not have LBPWS10."	\
			" Skipping test");				\
		return;	  	   					\
	}								\
} while (0);

#define CHECK_FOR_LBPWS							\
do {									\
	if (inq_lbp->lbpws == 0) {		       			\
		logging(LOG_NORMAL, "[SKIPPED] Logical unit does not"	\
			" have LBPWS. Skipping test");	     	  	\
		CU_PASS("[SKIPPED] Logical unit does not have LBPWS."	\
			" Skipping test");				\
		return;	  	   					\
	}								\
} while (0);

#define CHECK_FOR_LBPPB_GT_1						\
do {									\
	if (lbppb < 2) {						\
	  logging(LOG_NORMAL, "[SKIPPED] LBPPB < 2. Skipping test");	\
		CU_PASS("[SKIPPED] LBPPB < 2. Skipping test");		\
		return;	  	   					\
	}								\
} while (0);

#define CHECK_FOR_SBC							\
do {									\
	if (inq->device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {\
		logging(LOG_NORMAL, "[SKIPPED] Not SBC device."	\
			" Skipping test");				\
		CU_PASS("[SKIPPED] Not SBC device."			\
			" Skipping test");				\
		return;	  	   					\
	}								\
} while (0);

extern struct scsi_inquiry_standard *inq;
extern struct scsi_inquiry_logical_block_provisioning *inq_lbp;
extern struct scsi_inquiry_block_device_characteristics *inq_bdc;
extern struct scsi_inquiry_block_limits *inq_bl;
extern struct scsi_readcapacity16 *rc16;
extern struct scsi_report_supported_op_codes *rsop;

extern size_t block_size;
extern uint64_t num_blocks;
extern int lbppb;
extern int data_loss;
extern int allow_sanitize;
extern int readonly;
extern int sbc3_support;
extern int maximum_transfer_length;

struct iscsi_context *iscsi_context_login(const char *initiatorname, const char *url, int *lun);

struct iscsi_async_state {
	struct scsi_task *task;
	int status;
	int finished;
};
void wait_until_test_finished(struct iscsi_context *iscsi, struct iscsi_async_state *test_state);

struct iscsi_pdu;
int (*local_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

struct scsi_command_descriptor *get_command_descriptor(int opcode, int sa);

/*
 * PGR support
 */

static inline long rand_key(void)
{
	time_t t;
	pid_t p;
	unsigned int s;
	long l;

	(void)time(&t);
	p = getpid();
	s = ((int)p * (t & 0xffff));
	srandom(s);
	l = random();
	return l;
}

static inline int pr_type_is_all_registrants(
	enum scsi_persistent_out_type pr_type)
{
	switch (pr_type) {
	case SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS:
	case SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS:
		return 1;
	default:
		return 0;
	}
}

int prin_task(struct iscsi_context *iscsi, int lun, int service_action,
    int success_expected);
int prin_read_keys(struct iscsi_context *iscsi, int lun, struct scsi_task **tp,
    struct scsi_persistent_reserve_in_read_keys **rkp);
int prout_register_and_ignore(struct iscsi_context *iscsi, int lun,
    unsigned long long key);
int prout_register_key(struct iscsi_context *iscsi, int lun,
    unsigned long long sark, unsigned long long rk);
int prin_verify_key_presence(struct iscsi_context *iscsi, int lun,
    unsigned long long key, int present);
int prout_reregister_key_fails(struct iscsi_context *iscsi, int lun,
    unsigned long long sark);
int prout_reserve(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int prout_release(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int prin_verify_not_reserved(struct iscsi_context *iscsi, int lun);
int prin_verify_reserved_as(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int verify_read_works(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int verify_write_works(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int verify_read_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int verify_write_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int inquiry(struct iscsi_context *iscsi, int lun, int evpd, int page_code, int maxsize, struct scsi_task **save_task);
int inquiry_invalidfieldincdb(struct iscsi_context *iscsi, int lun, int evpd, int page_code, int maxsize);
struct scsi_task *get_lba_status_task(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len);
int compareandwrite(struct iscsi_context *iscsi, int lun, uint64_t lba, unsigned char *data, uint32_t len, int blocksize, int wrprotect, int dpo, int fua, int group_number);
int compareandwrite_miscompare(struct iscsi_context *iscsi, int lun, uint64_t lba, unsigned char *data, uint32_t len, int blocksize, int wrprotect, int dpo, int fua, int group_number);
int get_lba_status(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len);
int get_lba_status_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len);
int get_lba_status_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t len);
int orwrite(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int orwrite_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int orwrite_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int orwrite_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int orwrite_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int prefetch10(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int immed, int group);
int prefetch10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int immed, int group);
int prefetch10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int immed, int group);
int prefetch16(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int immed, int group);
int prefetch16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int immed, int group);
int prefetch16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int immed, int group);
int preventallow(struct iscsi_context *iscsi, int lun, int prevent);
int read6(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, unsigned char *data);
int read6_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, unsigned char *data);
struct scsi_task *read10_task(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16_sanitize(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int readcapacity10(struct iscsi_context *iscsi, int lun, uint32_t lba, int pmi);
int readcapacity10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, int pmi);
int readcapacity16(struct iscsi_context *iscsi, int lun, int alloc_len);
int readcapacity16_nomedium(struct iscsi_context *iscsi, int lun, int alloc_len);
int report_supported_opcodes(struct iscsi_context *iscsi, int lun, int rctd, int options, int opcode, int sa, int alloc_len, struct scsi_task **save_task);
int report_supported_opcodes_invalidfieldincdb(struct iscsi_context *iscsi, int lun, int rctd, int options, int opcode, int sa, int alloc_len, struct scsi_task **save_task);
int release6(struct iscsi_context *iscsi, int lun);
int reserve6(struct iscsi_context *iscsi, int lun);
int reserve6_conflict(struct iscsi_context *iscsi, int lun);
int sanitize(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data);
int sanitize_conflict(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data);
int sanitize_invalidfieldincdb(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data);
int sanitize_writeprotected(struct iscsi_context *iscsi, int lun, int immed, int ause, int sa, int param_len, struct iscsi_data *data);
int startstopunit(struct iscsi_context *iscsi, int lun, int immed, int pcm, int pc, int no_flush, int loej, int start);
int startstopunit_sanitize(struct iscsi_context *iscsi, int lun, int immed, int pcm, int pc, int no_flush, int loej, int start);
int startstopunit_preventremoval(struct iscsi_context *iscsi, int lun, int immed, int pcm, int pc, int no_flush, int loej, int start);
int synchronizecache10(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int sync_nv, int immed);
int synchronizecache10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int sync_nv, int immed);
int synchronizecache16(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int sync_nv, int immed);
int synchronizecache16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int sync_nv, int immed);
int testunitready_clear_ua(struct iscsi_context *iscsi, int lun);
int testunitready(struct iscsi_context *iscsi, int lun);
int testunitready_nomedium(struct iscsi_context *iscsi, int lun);
int testunitready_conflict(struct iscsi_context *iscsi, int lun);
int testunitready_sanitize(struct iscsi_context *iscsi, int lun);
int unmap(struct iscsi_context *iscsi, int lun, int anchor, struct unmap_list *list, int list_len);
int unmap_writeprotected(struct iscsi_context *iscsi, int lun, int anchor, struct unmap_list *list, int list_len);
int unmap_nomedium(struct iscsi_context *iscsi, int lun, int anchor, struct unmap_list *list, int list_len);
int verify10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify10_miscompare(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify12_miscompare(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify16_miscompare(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int verify16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data);
int write10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write10_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int writesame10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame10_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame16_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writesame16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data);
int writeverify10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify10_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify12_writeprotected(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify12_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify16_writeprotected(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);
int writeverify16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data);

int set_swp(struct iscsi_context *iscsi, int lun);
int clear_swp(struct iscsi_context *iscsi, int lun);


#endif	/* _ISCSI_SUPPORT_H_ */
