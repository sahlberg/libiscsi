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
#ifndef __iscsi_private_h__
#define __iscsi_private_h__

#include <stdint.h>
#include <time.h>

#if defined(_WIN32)
#include <basetsd.h>
#define ssize_t SSIZE_T
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#define ISCSI_RAW_HEADER_SIZE			48
#define ISCSI_DIGEST_SIZE			 4

#define ISCSI_HEADER_SIZE(hdr_digest) (ISCSI_RAW_HEADER_SIZE	\
  + (hdr_digest == ISCSI_HEADER_DIGEST_NONE?0:ISCSI_DIGEST_SIZE))

#define SMALL_ALLOC_MAX_FREE (128) /* must be power of 2 */

struct iscsi_in_pdu {
	struct iscsi_in_pdu *next;

	long long hdr_pos;
	unsigned char *hdr;

	long long data_pos;
	unsigned char *data;
};
void iscsi_free_iscsi_in_pdu(struct iscsi_context *iscsi, struct iscsi_in_pdu *in);

/* size of chap response field */
#define CHAP_R_SIZE 16

/* max length of chap challange */
#define MAX_CHAP_C_LENGTH 2048

struct iscsi_context {
	struct iscsi_transport *drv;
	void *opaque;
	enum iscsi_transport_type transport;

	char initiator_name[MAX_STRING_SIZE+1];
	char target_name[MAX_STRING_SIZE+1];
	char target_address[MAX_STRING_SIZE+1];  /* If a redirect */
	char connected_portal[MAX_STRING_SIZE+1];
	char portal[MAX_STRING_SIZE+1];
	char alias[MAX_STRING_SIZE+1];
	char bind_interfaces[MAX_STRING_SIZE+1];

	char user[MAX_STRING_SIZE+1];
	char passwd[MAX_STRING_SIZE+1];
	char chap_c[MAX_CHAP_C_LENGTH+1];

	char target_user[MAX_STRING_SIZE+1];
	char target_passwd[MAX_STRING_SIZE+1];
	uint32_t target_chap_i;
	unsigned char target_chap_r[CHAP_R_SIZE];

	char error_string[MAX_STRING_SIZE+1];

	enum iscsi_session_type session_type;
	unsigned char isid[6];
	uint32_t itt;
	uint32_t cmdsn;
	uint32_t min_cmdsn_waiting;
	uint32_t expcmdsn;
	uint32_t maxcmdsn;
	uint32_t statsn;
	enum iscsi_header_digest want_header_digest;
	enum iscsi_header_digest header_digest;

	int fd;
	int is_connected;
	int is_corked;

	int tcp_user_timeout;
	int tcp_keepcnt;
	int tcp_keepintvl;
	int tcp_keepidle;
	int tcp_syncnt;
	int tcp_nonblocking;

	int current_phase;
	int next_phase;
#define ISCSI_LOGIN_SECNEG_PHASE_OFFER_CHAP         0
#define ISCSI_LOGIN_SECNEG_PHASE_SELECT_ALGORITHM   1
#define ISCSI_LOGIN_SECNEG_PHASE_SEND_RESPONSE      2
	int secneg_phase;
	int login_attempts;
	int is_loggedin;
	int bind_interfaces_cnt;
	int nops_in_flight;

	int chap_a;
	int chap_i;

	iscsi_command_cb socket_status_cb;
	void *connect_data;

	struct iscsi_pdu *outqueue;
	struct iscsi_pdu *outqueue_current;
	struct iscsi_pdu *waitpdu;

	struct iscsi_in_pdu *incoming;

	uint32_t max_burst_length;
	uint32_t first_burst_length;
	uint32_t initiator_max_recv_data_segment_length;
	uint32_t target_max_recv_data_segment_length;
	enum iscsi_initial_r2t want_initial_r2t;
	enum iscsi_initial_r2t use_initial_r2t;
	enum iscsi_immediate_data want_immediate_data;
	enum iscsi_immediate_data use_immediate_data;

	int lun;
	int no_auto_reconnect;
	int reconnect_deferred;
	int reconnect_max_retries;
	int pending_reconnect;

	int log_level;
	iscsi_log_fn log_fn;

	int mallocs;
	int reallocs;
	int frees;
	int smallocs;
	void* smalloc_ptrs[SMALL_ALLOC_MAX_FREE];
	int smalloc_free;
	size_t smalloc_size;
	int cache_allocations;

	time_t next_reconnect;
	int scsi_timeout;
	struct iscsi_context *old_iscsi;
	int retry_cnt;
	int no_ua_on_reconnect;
};

#define ISCSI_PDU_IMMEDIATE		       0x40

#define ISCSI_PDU_TEXT_FINAL		       0x80
#define ISCSI_PDU_TEXT_CONTINUE		       0x40

#define ISCSI_PDU_LOGIN_TRANSIT		       0x80
#define ISCSI_PDU_LOGIN_CONTINUE	       0x40
#define ISCSI_PDU_LOGIN_CSG_SECNEG	       0x00
#define ISCSI_PDU_LOGIN_CSG_OPNEG	       0x04
#define ISCSI_PDU_LOGIN_CSG_FF		       0x0c
#define ISCSI_PDU_LOGIN_NSG_SECNEG	       0x00
#define ISCSI_PDU_LOGIN_NSG_OPNEG	       0x01
#define ISCSI_PDU_LOGIN_NSG_FF		       0x03

#define ISCSI_PDU_SCSI_FINAL		       0x80
#define ISCSI_PDU_SCSI_READ		       0x40
#define ISCSI_PDU_SCSI_WRITE		       0x20
#define ISCSI_PDU_SCSI_ATTR_UNTAGGED	       0x00
#define ISCSI_PDU_SCSI_ATTR_SIMPLE	       0x01
#define ISCSI_PDU_SCSI_ATTR_ORDERED	       0x02
#define ISCSI_PDU_SCSI_ATTR_HEADOFQUEUE	       0x03
#define ISCSI_PDU_SCSI_ATTR_ACA		       0x04

#define ISCSI_PDU_DATA_FINAL		       0x80
#define ISCSI_PDU_DATA_ACK_REQUESTED	       0x40
#define ISCSI_PDU_DATA_BIDIR_OVERFLOW  	       0x10
#define ISCSI_PDU_DATA_BIDIR_UNDERFLOW         0x08
#define ISCSI_PDU_DATA_RESIDUAL_OVERFLOW       0x04
#define ISCSI_PDU_DATA_RESIDUAL_UNDERFLOW      0x02
#define ISCSI_PDU_DATA_CONTAINS_STATUS	       0x01

enum iscsi_opcode {
	ISCSI_PDU_NOP_OUT                        = 0x00,
	ISCSI_PDU_SCSI_REQUEST                   = 0x01,
	ISCSI_PDU_SCSI_TASK_MANAGEMENT_REQUEST   = 0x02,
	ISCSI_PDU_LOGIN_REQUEST                  = 0x03,
	ISCSI_PDU_TEXT_REQUEST                   = 0x04,
	ISCSI_PDU_DATA_OUT                       = 0x05,
	ISCSI_PDU_LOGOUT_REQUEST                 = 0x06,
	ISCSI_PDU_NOP_IN                         = 0x20,
	ISCSI_PDU_SCSI_RESPONSE                  = 0x21,
	ISCSI_PDU_SCSI_TASK_MANAGEMENT_RESPONSE  = 0x22,
	ISCSI_PDU_LOGIN_RESPONSE                 = 0x23,
	ISCSI_PDU_TEXT_RESPONSE                  = 0x24,
	ISCSI_PDU_DATA_IN                        = 0x25,
	ISCSI_PDU_LOGOUT_RESPONSE                = 0x26,
	ISCSI_PDU_R2T                            = 0x31,
	ISCSI_PDU_ASYNC_MSG                      = 0x32,
	ISCSI_PDU_REJECT                         = 0x3f,
	ISCSI_PDU_NO_PDU                         = 0xff
};

struct iscsi_scsi_cbdata {
	iscsi_command_cb          callback;
	void                     *private_data;
	struct scsi_task         *task;
};

struct iscsi_pdu {
	struct iscsi_pdu *next;

/* There will not be a response to this pdu, so delete it once it is sent on the wire. Don't put it on the wait-queue */
#define ISCSI_PDU_DELETE_WHEN_SENT	0x00000001
/* When reconnecting, just drop all these PDUs. Don't re-queue them.
 * This includes any DATA-OUT PDU as well as all NOPs.
 */
#define ISCSI_PDU_DROP_ON_RECONNECT	0x00000004
/* stop sending after this PDU has been sent */
#define ISCSI_PDU_CORK_WHEN_SENT	0x00000008

	uint32_t flags;

	uint32_t lun;
	uint32_t itt;
	uint32_t cmdsn;
	uint32_t datasn;
	enum iscsi_opcode response_opcode;

	iscsi_command_cb callback;
	void *private_data;

	/* Used to track writing the iscsi header to the socket */
	struct iscsi_data outdata; /* Header for PDU to send */
	size_t outdata_written;	   /* How much of the header we have written */

	/* Used to track writing the payload data to the socket */
	uint32_t payload_offset;   /* Offset of payload data to write */
	uint32_t payload_len;      /* Amount of payload data to write */
	uint32_t payload_written;  /* How much of the payload we have written */


	struct iscsi_data indata;

	struct iscsi_scsi_cbdata scsi_cbdata;
	time_t scsi_timeout;
	uint32_t expxferlen;
};

struct iscsi_pdu *iscsi_allocate_pdu(struct iscsi_context *iscsi,
				     enum iscsi_opcode opcode,
				     enum iscsi_opcode response_opcode,
				     uint32_t itt,
				     uint32_t flags);
void iscsi_free_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);
void iscsi_pdu_set_pduflags(struct iscsi_pdu *pdu, unsigned char flags);
void iscsi_pdu_set_immediate(struct iscsi_pdu *pdu);
void iscsi_pdu_set_ttt(struct iscsi_pdu *pdu, uint32_t ttt);
void iscsi_pdu_set_cmdsn(struct iscsi_pdu *pdu, uint32_t cmdsn);
void iscsi_pdu_set_rcmdsn(struct iscsi_pdu *pdu, uint32_t rcmdsn);
void iscsi_pdu_set_lun(struct iscsi_pdu *pdu, uint32_t lun);
void iscsi_pdu_set_expstatsn(struct iscsi_pdu *pdu, uint32_t expstatsnsn);
void iscsi_pdu_set_expxferlen(struct iscsi_pdu *pdu, uint32_t expxferlen);
void iscsi_pdu_set_itt(struct iscsi_pdu *pdu, uint32_t itt);
void iscsi_pdu_set_ritt(struct iscsi_pdu *pdu, uint32_t ritt);
void iscsi_pdu_set_datasn(struct iscsi_pdu *pdu, uint32_t datasn);
void iscsi_pdu_set_bufferoffset(struct iscsi_pdu *pdu, uint32_t bufferoffset);
void iscsi_cancel_pdus(struct iscsi_context *iscsi);
int iscsi_pdu_add_data(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
		       unsigned char *dptr, int dsize);
int iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);
int iscsi_add_data(struct iscsi_context *iscsi, struct iscsi_data *data,
		   unsigned char *dptr, int dsize, int pdualignment);

struct scsi_task;
void iscsi_pdu_set_cdb(struct iscsi_pdu *pdu, struct scsi_task *task);

int iscsi_get_pdu_data_size(const unsigned char *hdr);
int iscsi_get_pdu_padding_size(const unsigned char *hdr);
int iscsi_process_pdu(struct iscsi_context *iscsi, struct iscsi_in_pdu *in);

int iscsi_process_login_reply(struct iscsi_context *iscsi,
			      struct iscsi_pdu *pdu,
			      struct iscsi_in_pdu *in);
int iscsi_process_text_reply(struct iscsi_context *iscsi,
			     struct iscsi_pdu *pdu,
			     struct iscsi_in_pdu *in);
int iscsi_process_logout_reply(struct iscsi_context *iscsi,
			       struct iscsi_pdu *pdu,
			       struct iscsi_in_pdu *in);
int iscsi_process_scsi_reply(struct iscsi_context *iscsi,
			     struct iscsi_pdu *pdu,
			     struct iscsi_in_pdu *in);
int iscsi_process_scsi_data_in(struct iscsi_context *iscsi,
			       struct iscsi_pdu *pdu,
			       struct iscsi_in_pdu *in,
			       int *is_finished);
int iscsi_process_nop_out_reply(struct iscsi_context *iscsi,
				struct iscsi_pdu *pdu,
				struct iscsi_in_pdu *in);
int iscsi_process_task_mgmt_reply(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
				  struct iscsi_in_pdu *in);
int iscsi_process_r2t(struct iscsi_context *iscsi, struct iscsi_pdu *pdu,
		      struct iscsi_in_pdu *in);
int iscsi_process_reject(struct iscsi_context *iscsi,
				struct iscsi_in_pdu *in);
int iscsi_send_target_nop_out(struct iscsi_context *iscsi, uint32_t ttt, uint32_t lun);

#if defined(_WIN32)
void iscsi_set_error(struct iscsi_context *iscsi, const char *error_string,
		     ...);
#else
void iscsi_set_error(struct iscsi_context *iscsi, const char *error_string,
		     ...) __attribute__((format(printf, 2, 3)));
#endif

struct scsi_iovector *iscsi_get_scsi_task_iovector_in(struct iscsi_context *iscsi, struct iscsi_in_pdu *in);
struct scsi_iovector *iscsi_get_scsi_task_iovector_out(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);
void scsi_task_reset_iov(struct scsi_iovector *iovector);

void* iscsi_malloc(struct iscsi_context *iscsi, size_t size);
void* iscsi_zmalloc(struct iscsi_context *iscsi, size_t size);
void* iscsi_realloc(struct iscsi_context *iscsi, void* ptr, size_t size);
void iscsi_free(struct iscsi_context *iscsi, void* ptr);
char* iscsi_strdup(struct iscsi_context *iscsi, const char* str);
void* iscsi_smalloc(struct iscsi_context *iscsi, size_t size);
void* iscsi_szmalloc(struct iscsi_context *iscsi, size_t size);
void iscsi_sfree(struct iscsi_context *iscsi, void* ptr);

uint32_t crc32c(uint8_t *buf, int len);

struct scsi_task *iscsi_scsi_get_task_from_pdu(struct iscsi_pdu *pdu);

void iscsi_decrement_iface_rr(void);

#define ISCSI_LOG(iscsi, level, format, ...) \
	do { \
		if (level <= iscsi->log_level && iscsi->log_fn) { \
			iscsi_log_message(iscsi, level, format, ## __VA_ARGS__); \
		} \
	} while (0)

void
iscsi_log_message(struct iscsi_context *iscsi, int level, const char *format, ...);

void
iscsi_add_to_outqueue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

int iscsi_serial32_compare(uint32_t s1, uint32_t s2);

uint32_t iscsi_itt_post_increment(struct iscsi_context *iscsi);

void iscsi_timeout_scan(struct iscsi_context *iscsi);

void iscsi_reconnect_cb(struct iscsi_context *iscsi _U_, int status,
                        void *command_data, void *private_data);

struct iscsi_pdu *iscsi_tcp_new_pdu(struct iscsi_context *iscsi, size_t size);

void iscsi_init_tcp_transport(struct iscsi_context *iscsi);

void iscsi_tcp_free_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

int iscsi_service_reconnect_if_loggedin(struct iscsi_context *iscsi);

void iscsi_dump_pdu_header(struct iscsi_context *iscsi, unsigned char *data);

union socket_address;

typedef struct iscsi_transport {
	int (*connect)(struct iscsi_context *iscsi, union socket_address *sa, int ai_family);
	int (*queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);
	struct iscsi_pdu* (*new_pdu)(struct iscsi_context *iscsi, size_t size);
	int (*disconnect)(struct iscsi_context *iscsi);
	void (*free_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);
	int (*service)(struct iscsi_context *iscsi, int revents);
	int (*get_fd)(struct iscsi_context *iscsi);
	int (*which_events)(struct iscsi_context *iscsi);
} iscsi_transport;

#ifdef __cplusplus
}
#endif

#endif /* __iscsi_private_h__ */
