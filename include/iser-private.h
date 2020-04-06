/*
   Copyright (c) 2014-2016, Mellanox Technologies, Ltd.  All rights reserved.

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
#ifndef __iser_private_h__
#define __iser_private_h__

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include <strings.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>

#ifdef __linux

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#define unlikely(x)  __builtin_expect (!!(x), 0)

#define ISER_VER                        0x10
#define ISER_WSV                        0x08
#define ISER_RSV                        0x04

#define DATA_BUFFER_UNIT_SHIFT        SHIFT_4K
#define DATA_BUFFER_UNIT_SIZE         SIZE_4K
#define DATA_BUFFER_CHUNK_SHIFT       22
#define DATA_BUFFER_CHUNK_SIZE        (1ULL << DATA_BUFFER_CHUNK_SHIFT)
#define DATA_BUFFER_CHUNK_UNITS_SHIFT (DATA_BUFFER_CHUNK_SHIFT - DATA_BUFFER_UNIT_SHIFT)
#define DATA_BUFFER_CHUNK_UNITS       (1 << DATA_BUFFER_CHUNK_UNITS_SHIFT)

#define ISER_HEADERS_LEN  (sizeof(struct iser_hdr) + ISCSI_RAW_HEADER_SIZE)

#define ISER_RECV_DATA_SEG_LEN  128
#define ISER_RX_PAYLOAD_SIZE    (ISER_HEADERS_LEN + ISER_RECV_DATA_SEG_LEN)

#define ISER_RX_LOGIN_SIZE      (ISER_HEADERS_LEN + ISCSI_DEF_MAX_RECV_SEG_LEN)

#define ISCSI_DEF_MAX_RECV_SEG_LEN 8192

#define	BHSSC_FLAGS_R		0x40
#define	BHSSC_FLAGS_W		0x20

#define ISER_MAX_CQ_LEN 1024

#define ISER_ZBVA_NOT_SUPPORTED         0x80
#define ISER_SEND_W_INV_NOT_SUPPORTED   0x40

enum desc_type {
	ISCSI_CONTROL = 0,
	ISCSI_COMMAND};

enum data_dir{
	DATA_WRITE = 0,
	DATA_READ};

#define SHIFT_4K	12
#define SIZE_4K	(1ULL << SHIFT_4K)
#define MASK_4K	(~(SIZE_4K-1))

#define ISER_DEF_XMIT_CMDS_MAX  512
#define ISER_QP_MAX_RECV_DTOS  (ISER_DEF_XMIT_CMDS_MAX)
#define ISER_MIN_POSTED_RX     (ISER_DEF_XMIT_CMDS_MAX >> 2)


/**
 * struct iser_hdr - iSER header
 *
 * @flags:        flags support (zbva, remote_inv)
 * @rsvd:         reserved
 * @write_stag:   write rkey
 * @write_va:     write virtual address
 * @reaf_stag:    read rkey
 * @read_va:      read virtual address
 */

struct iser_hdr {
	uint8_t    flags;
	uint8_t    rsvd[3];
	uint32_t   write_stag;
	uint64_t   write_va;
	uint32_t   read_stag;
	uint64_t   read_va;
} __attribute__((packed));

/**
 * struct iser_rx_desc - iSER RX descriptor (for recv wr_id)
 *
 * @isr_hdr:       iser header
 * @iscsi_data:    iscsi header
 * @data:          received data segment
 * @rx_sg:         ibv_sge of receive buffer
 * @pad:           padding
 */


struct iser_rx_desc {
	struct iser_hdr              iser_header;
	char                         iscsi_header[ISCSI_RAW_HEADER_SIZE];
	char                         data[ISER_RECV_DATA_SEG_LEN];
	char                         pad[4];
	struct ibv_mr               *hdr_mr;
	struct ibv_sge               rx_sg;
};

static_assert(offsetof(struct iser_rx_desc, hdr_mr) % 8 == 0, "iser_rx_desc is not aligned on 8-byte boundary");

/**
 * struct iser_tx_desc - iSER TX descriptor (for send wr_id)
 *
 * @iser_hdr:      iser header
 * @iscsi_header:  iscsi header (bhs)
 * @tx_sg:         sg[0] points to iser/iscsi headers
 *                 sg[1] optionally points to either of immediate data
 *                 unsolicited data-out or control
 * @num_sge:       number sges used on this TX task
 * @mr:            iser/iscsi headers mr
 * @data_mr:       mr for case we need to allocate mr for read
 * @next:          next descriptor on the list
 */

struct iser_tx_desc {
	struct iser_hdr              iser_header;
	unsigned char                iscsi_header[ISCSI_RAW_HEADER_SIZE];
	struct ibv_sge               tx_sg[2];
	int                          num_sge;
	struct ibv_mr                *hdr_mr;
	char			     *data_buff;
	struct ibv_mr                *data_mr;
	enum desc_type		     type;
	enum data_dir                data_dir;
	struct iser_tx_desc          *next;
};

struct iser_cm_hdr {
	uint8_t      flags;
	uint8_t      rsvd[3];
};

struct iser_pdu {
	struct iscsi_pdu              iscsi_pdu;
	struct iser_tx_desc           *desc;
};

struct iser_buf_chunk {
    unsigned char *buf;
    struct ibv_mr *mr;
    struct iser_buf_chunk *next;
    int8_t tree[DATA_BUFFER_CHUNK_UNITS << 1];
};

struct iser_conn {
	struct rdma_cm_id            *cma_id;
	struct rdma_event_channel    *cma_channel;

	struct ibv_pd                *pd;
	struct ibv_cq                *cq;
	struct ibv_qp                *qp;
	struct ibv_comp_channel      *comp_channel;

	int                          rdma_connect_sent;

	struct ibv_recv_wr           rx_wr[ISER_MIN_POSTED_RX];

	struct ibv_mr                *login_resp_mr;
	unsigned char                *login_resp_buf;

	struct iser_rx_desc          *rx_descs;
	uint32_t                     num_rx_descs;
	unsigned int                 rx_desc_head;

	int                          post_recv_buf_count;
	int                          qp_max_recv_dtos;
	int                          min_posted_rx;
	uint16_t                     max_cmds;

	struct iser_tx_desc          *tx_desc;
	struct iser_buf_chunk        *buf_chunk;
};

void iscsi_init_iser_transport(struct iscsi_context *iscsi);

#endif /* __linux */

#endif   /* __iser_private_h__ */
