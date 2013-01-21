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
void logging(int level, const char *format, ...);

extern uint32_t block_size;
extern uint64_t num_blocks;
extern int lbpme;
extern int lbppb;
extern int lbpme;
extern int data_loss;
extern int removable;
extern enum scsi_inquiry_peripheral_device_type device_type;
extern int sccs;
extern int encserv;


struct iscsi_context *iscsi_context_login(const char *initiatorname, const char *url, int *lun);

struct iscsi_async_state {
	struct scsi_task *task;
	int status;
	int finished;
};
void wait_until_test_finished(struct iscsi_context *iscsi, struct iscsi_async_state *test_state);

struct iscsi_pdu;
int (*local_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);



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

int register_and_ignore(struct iscsi_context *iscsi, int lun,
    unsigned long long key);
int register_key(struct iscsi_context *iscsi, int lun,
    unsigned long long sark, unsigned long long rk);
int verify_key_presence(struct iscsi_context *iscsi, int lun,
    unsigned long long key, int present);
int reregister_key_fails(struct iscsi_context *iscsi, int lun,
    unsigned long long sark);
int reserve(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int release(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int verify_reserved_as(struct iscsi_context *iscsi, int lun,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int verify_read_works(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int verify_write_works(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int verify_read_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int verify_write_fails(struct iscsi_context *iscsi, int lun, unsigned char *buf);
int testunitready(struct iscsi_context *iscsi, int lun);
int testunitready_nomedium(struct iscsi_context *iscsi, int lun);
int testunitready_conflict(struct iscsi_context *iscsi, int lun);
int prefetch10(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int immed, int group);
int prefetch10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int immed, int group);
int prefetch10_nomedium(struct iscsi_context *iscsi, int lun, uint32_t lba, int num_blocks, int immed, int group);
int prefetch16(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int immed, int group);
int prefetch16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int immed, int group);
int prefetch16_nomedium(struct iscsi_context *iscsi, int lun, uint64_t lba, int num_blocks, int immed, int group);
int read6(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, unsigned char *data);
int read6_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, unsigned char *data);
int read10(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read10_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read10_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int read16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int readcapacity10(struct iscsi_context *iscsi, int lun, uint32_t lba, int pmi);
int verify10(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify10_nomedium(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify10_miscompare(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify10_lbaoutofrange(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify12(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify12_nomedium(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify12_miscompare(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify12_lbaoutofrange(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint32_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify16(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify16_nomedium(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify16_miscompare(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int verify16_lbaoutofrange(struct iscsi_context *iscsi, int lun, unsigned char *data, uint32_t datalen, uint64_t lba, int vprotect, int dpo, int bytchk, int blocksize);
int write12(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write12_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16_invalidfieldincdb(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);
int write16_lbaoutofrange(struct iscsi_context *iscsi, int lun, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data);


int inquiry(struct iscsi_context *iscsi, int lun, int evpd, int page_code, int maxsize);

#endif	/* _ISCSI_SUPPORT_H_ */
