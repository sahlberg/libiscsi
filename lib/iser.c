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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include "slist.h"
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <inttypes.h>
#include "iscsi.h"
#include "iser-private.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"
#include <sys/eventfd.h>
#include <limits.h>
#include <poll.h>


#ifndef container_of
/**
  * container_of - cast a member of a structure out to the containing structure
  * @ptr:        the pointer to the member.
  * @type:       the type of the container struct this is embedded in.
  * @member:     the name of the member within the struct.
  *
 */
#define container_of(ptr, type, member) \
    ((type *) ((uint8_t *)(ptr) - offsetof(type, member)))
#endif


#ifdef __linux

/* MUST keep in sync with socket.c */
union socket_address {
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr sa;
};

static int cq_handle(struct iser_conn *iser_conn);
static int iscsi_iser_revive_queued_pdus(struct iscsi_context *iscsi);
static int iscsi_iser_cm_event(struct iscsi_context *iscsi);

/*
 * iscsi_iser_get_fd() - Return completion queue
 *                       event channel file descriptor.
 */
static int
iscsi_iser_get_fd(struct iscsi_context *iscsi)
{
	return iscsi->fd;
}

/*
 * iscsi_iser_which_events() - Which events to wait for on file descriptor
 * @iscsi_context:    iscsi_context Unused
 *
 * Notes:
 * CQ can only create POLLIN events, hence this function
 * will return same value of 1 each time.
 * Being used in QEMU iscsi block so we need compatability with TCP
 */
static int
iscsi_iser_which_events(struct iscsi_context *iscsi)
{
	/* iSER is waiting to events from CQ that are always POLLIN */
	return 1;
}

/*
 * iscsi_iser_service() - Processing CQ events
 * @iscsi_context:    iscsi_context
 * @revents:          which events to handle
 *
 * Notes:
 * CQ can only create POLLIN events, hence this function
 * will poll the cq for completion until boundary or emptiness.
 */
static int
iscsi_iser_service(struct iscsi_context *iscsi, int revents)
{
	int ret = 0;
	struct iser_conn *iser_conn = iscsi->opaque;

	if (iscsi->pending_reconnect) {
		if (time(NULL) >= iscsi->next_reconnect) {
			return iscsi_reconnect(iscsi);
		} else {
			if (iscsi->old_iscsi) {
				return 0;
			}
		}
	}

	if (revents == POLLIN)
		ret = iscsi->is_connected ? cq_handle(iser_conn) : iscsi_iser_cm_event(iscsi);
	else if (!revents) {
		ret = iscsi_iser_cm_event(iscsi);
	} else {
		iscsi_set_error(iscsi, "revents is not POLLIN");
		return -1;
	}

	if (ret) {
		return iscsi_service_reconnect_if_loggedin(iscsi);
	}

	return iscsi_iser_revive_queued_pdus(iscsi);
}

static inline int
fls(int x)
{
    if (!x)
        return 0;

    return sizeof(int) * 8 - __builtin_clz(x);
}

static inline void
iser_buf_chunk_tree_propagate(int8_t *tree, int pos, int level) {
	int value;

	for (pos >>= 1, level++; pos; pos >>= 1, level++) {
		if (tree[pos << 1] == level - 1) {
			if (tree[(pos << 1) | 1] == level - 1) {
				value = level;
			} else {
				value = level - 1;
			}
		} else {
			value = (tree[pos << 1] > tree[(pos << 1) | 1] ?
			         tree[pos << 1] : tree[(pos << 1) | 1]);
		}

		if (value == tree[pos])
			break;

		tree[pos] = value;
	}
}

static inline void
iser_buf_chunk_free(struct iser_buf_chunk *chunk, void *ptr) {
	int8_t *tree = chunk->tree;
	int unit = ((unsigned char *)ptr - chunk->buf) >> DATA_BUFFER_UNIT_SHIFT;
	int pos = unit + DATA_BUFFER_CHUNK_UNITS;
	int level = 1;

	for (; tree[pos]; pos >>= 1, level++) {
	}

	tree[pos] = level;
	iser_buf_chunk_tree_propagate(tree, pos, level);
}

static inline void *
iser_buf_chunk_alloc(struct iser_buf_chunk *chunk, int want) {
	int8_t *tree = chunk->tree;
	int pos, level, unit;
	void *result;

	/* satisfy ? */
	if (tree[1] < want) {
		return NULL;
	}

	/* lookup */
	for (pos = 1, level = DATA_BUFFER_CHUNK_UNITS_SHIFT + 1; want < level; level--) {
		pos = pos << 1;
		pos = tree[pos] >= want ? pos : (pos | 1);
	}

	/* pick the node */
	tree[pos] = 0;
	unit = (pos << (level - 1)) - DATA_BUFFER_CHUNK_UNITS;
	result = chunk->buf + (unit << DATA_BUFFER_UNIT_SHIFT);

	/* propagate */
	iser_buf_chunk_tree_propagate(tree, pos, level);

	return result;
}

static void
iser_tx_desc_free(struct iscsi_context *iscsi, struct iser_tx_desc *tx_desc)
{
	struct iser_conn *iser_conn = iscsi->opaque;
	struct iser_buf_chunk *chunk = iser_conn->buf_chunk;

	if (tx_desc->data_mr != NULL) {
		for (; chunk != NULL; chunk = chunk->next) {
			if (chunk->mr == tx_desc->data_mr) {
				iser_buf_chunk_free(chunk, tx_desc->data_buff);
				break;
			}
		}
		if (chunk == NULL) {
			ibv_dereg_mr(tx_desc->data_mr);
			iscsi_free(iscsi, tx_desc->data_buff);
		}
	}

	ISCSI_LIST_ADD(&iser_conn->tx_desc, tx_desc);
}

static struct iser_tx_desc *
iser_tx_desc_alloc(struct iscsi_context *iscsi, size_t data_size) {
	struct iser_conn *iser_conn = iscsi->opaque;
	struct iser_tx_desc *tx_desc = iser_conn->tx_desc;
	struct iser_buf_chunk **buf_chunk = &iser_conn->buf_chunk;
	struct iser_buf_chunk *chunk;
	int want, i;
	void *buf;

	if (tx_desc != NULL) {
		ISCSI_LIST_REMOVE(&iser_conn->tx_desc, tx_desc);
	} else {
		tx_desc = iscsi_malloc(iscsi, sizeof(*tx_desc));
		if (tx_desc == NULL) {
			iscsi_set_error(iscsi, "Out-Of-Memory, failed to allocate data buffer");
			return NULL;
		}

		tx_desc->hdr_mr = ibv_reg_mr(iser_conn->pd, tx_desc, ISER_HEADERS_LEN, IBV_ACCESS_LOCAL_WRITE);
		if (tx_desc->hdr_mr == NULL) {
			iscsi_free(iscsi, tx_desc);
			iscsi_set_error(iscsi, "Failed to register data mr");
			return NULL;
		}
	}

	if (data_size == 0) {
		tx_desc->data_buff = NULL;
		tx_desc->data_mr = NULL;
		return tx_desc;
	} else if (data_size > DATA_BUFFER_CHUNK_SIZE) {
		tx_desc->data_buff = iscsi_malloc(iscsi, data_size);
		if (tx_desc->data_buff == NULL) {
			iscsi_set_error(iscsi, "Out-Of-Memory, failed to allocate data buffer");
			goto release;
		}

		tx_desc->data_mr = ibv_reg_mr(iser_conn->pd, tx_desc->data_buff, data_size,
				IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);
		if (tx_desc->data_mr == NULL) {
			iscsi_free(iscsi, tx_desc->data_buff);
			iscsi_set_error(iscsi, "Failed to register data mr");
			goto release;
		}
		return tx_desc;
	}

	want = fls((data_size * 2 - 1) / DATA_BUFFER_UNIT_SIZE / 2) + 1;

	for (; *buf_chunk != NULL; buf_chunk = &(*buf_chunk)->next) {
		buf = iser_buf_chunk_alloc(*buf_chunk, want);
		if (buf != NULL) {
			tx_desc->data_buff = buf;
			tx_desc->data_mr = (*buf_chunk)->mr;
			return tx_desc;
		}
	}

	chunk = iscsi_malloc(iscsi, sizeof(struct iser_buf_chunk));
	if (chunk == NULL) {
		iscsi_set_error(iscsi, "Out-Of-Memory, failed to allocate data buffer");
		goto release;
	}

	chunk->buf = iscsi_malloc(iscsi, DATA_BUFFER_CHUNK_SIZE);
	if (chunk == NULL) {
		iscsi_set_error(iscsi, "Out-Of-Memory, failed to allocate data buffer");
		goto free_chunk;
	}

	chunk->mr = ibv_reg_mr(iser_conn->pd, chunk->buf, DATA_BUFFER_CHUNK_SIZE,
			IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);

	if (chunk->mr == NULL) {
		iscsi_set_error(iscsi, "Failed to register data mr");
		goto free_chunk_buf;
	}

	for (i = 0; i < DATA_BUFFER_CHUNK_UNITS_SHIFT + 1; i++) {
		memset(chunk->tree + (1 << i),
		       DATA_BUFFER_CHUNK_UNITS_SHIFT - i + 1, (1 << i));
	}
	chunk->next = NULL;
	*buf_chunk = chunk;

	tx_desc->data_buff = iser_buf_chunk_alloc(chunk, want);
	tx_desc->data_mr = chunk->mr;

	return tx_desc;

free_chunk_buf:
	iscsi_free(iscsi, chunk->buf);

free_chunk:
	iscsi_free(iscsi, chunk);

release:
	ISCSI_LIST_ADD(&iser_conn->tx_desc, tx_desc);

	return NULL;
}

static void
iser_free_queued_pdu_tx_desc(struct iscsi_context *iscsi)
{
	struct iscsi_pdu *pdu;
	struct iser_pdu *iser_pdu;

	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		iser_pdu = container_of(pdu, struct iser_pdu, iscsi_pdu);
		if (iser_pdu->desc) {
			iser_tx_desc_free(iscsi, iser_pdu->desc);
			iser_pdu->desc = NULL;
		}
	}

	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		iser_pdu = container_of(pdu, struct iser_pdu, iscsi_pdu);
		if (iser_pdu->desc) {
			iser_tx_desc_free(iscsi, iser_pdu->desc);
			iser_pdu->desc = NULL;
		}
	}
}

/*
 * iser_free_rx_descriptors() - freeing descriptors memory
 * @iser_conn:    ib connection context
 */
static void
iser_free_rx_descriptors(struct iser_conn *iser_conn)
{
	struct iser_rx_desc *rx_desc;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;
	int i;

	rx_desc = iser_conn->rx_descs;
	for (i = 0; i < iser_conn->qp_max_recv_dtos; i++, rx_desc++)
		if (ibv_dereg_mr(rx_desc->hdr_mr))
			iscsi_set_error(iscsi, "Failed ti deregister hdr mr");
	iscsi_free(iscsi, iser_conn->rx_descs);

	iser_conn->rx_descs = NULL;

	return;
}

static void
iser_free_reg_mr(struct iser_conn *iser_conn)
{
	struct iser_tx_desc *tx_desc = iser_conn->tx_desc;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;
	struct iser_tx_desc *temp_tx_desc;
	struct iser_buf_chunk *chunk, *temp_chunk;

	while (tx_desc) {
		ibv_dereg_mr(tx_desc->hdr_mr);

		temp_tx_desc = tx_desc;
		tx_desc = tx_desc->next;
		iscsi_free(iscsi, temp_tx_desc);
	}
	iser_conn->tx_desc = NULL;

	for (chunk = iser_conn->buf_chunk; chunk; ) {
		ibv_dereg_mr(chunk->mr);
		iscsi_free(iscsi, chunk->buf);

		temp_chunk = chunk;
		chunk = temp_chunk->next;
		iscsi_free(iscsi, temp_chunk);
	}
	iser_conn->buf_chunk = NULL;

	return;
}

/*
 * iser_free_iser_conn_res() - freeing ib context resources
 * @iser_conn:    ib connection context
 */
static void
iser_free_iser_conn_res(struct iser_conn *iser_conn, bool destroy)
{
	int ret;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	if (iser_conn->qp) {
		rdma_destroy_qp(iser_conn->cma_id);
		iser_conn->qp = NULL;
	}

	if (destroy) {

		iser_free_reg_mr(iser_conn);

		if (iser_conn->rx_descs) {
			iser_free_rx_descriptors(iser_conn);
			iser_conn->rx_descs = NULL;
		}

		if (iser_conn->login_resp_mr) {
			ret = ibv_dereg_mr(iser_conn->login_resp_mr);
			if (ret)
				iscsi_set_error(iscsi, "Failed to deregister login response mr");
		}

		if (iser_conn->login_resp_buf) {
			iscsi_free(iscsi, iser_conn->login_resp_buf);
			iser_conn->login_resp_buf = NULL;
		}

		if (iser_conn->cq) {
			if (iser_conn->cq_nevents > 0) {
				ibv_ack_cq_events(iser_conn->cq, iser_conn->cq_nevents);
				iser_conn->cq_nevents = 0;
			}
			ret = ibv_destroy_cq(iser_conn->cq);
			if (ret)
				iscsi_set_error(iscsi, "Failed to destroy cq");
		}

		if (iser_conn->comp_channel) {
			ret = ibv_destroy_comp_channel(iser_conn->comp_channel);
			if (ret)
				iscsi_set_error(iscsi, "Failed to destroy completion channel");
		}

		if (iser_conn->pd) {
			ret = ibv_dealloc_pd(iser_conn->pd);
			if (ret)
				iscsi_set_error(iscsi, "Failed to deallocate pd");
		}
	}

	return;
}

/*
 * iser_conn_release() - releasing ib resources
 *                       and destroying cm id
 * @iscsi_context:    iscsi context
 */
static void
iser_conn_release(struct iscsi_context *iscsi)
{
	int ret;
	struct iser_conn *iser_conn = iscsi->opaque;

	iser_free_queued_pdu_tx_desc(iscsi);
	iser_free_iser_conn_res(iser_conn, true);

	if (iser_conn->cma_id) {
		ret = rdma_destroy_id(iser_conn->cma_id);
		if (ret)
			iscsi_set_error(iscsi, "Failed destroying cm id");

		iser_conn->cma_id = NULL;
	}

	if (iser_conn->cma_channel != NULL) {
		rdma_destroy_event_channel(iser_conn->cma_channel);
		iser_conn->cma_channel = NULL;
	}

	return;
}

/*
 * iser_conn_terminate() - disconnecting rdma_cm
 * @iser_conn:    ib connection context
 */
static void
iser_conn_terminate(struct iser_conn *iser_conn)
{
	int ret;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	if(iser_conn->rdma_connect_sent) {
		ret = rdma_disconnect(iser_conn->cma_id);
		iser_conn->rdma_connect_sent = 0;
		if (ret)
			iscsi_set_error(iscsi, "Failed to disconnect, conn: 0x%p, err %d\n",
					iser_conn, ret);
	}

	return;
}

/*
 * iscsi_iser_disconnect() - disconnecting iSER and
 *                           freeing resources
 * @iser_conn:    ib connection context
 */
static int
iscsi_iser_disconnect(struct iscsi_context *iscsi) {

	struct iser_conn *iser_conn = iscsi->opaque;

	if (iser_conn->cma_id) {
		iser_conn_terminate(iser_conn);
		iser_conn_release(iscsi);
	}

	if (iscsi->fd != -1) {
		close(iscsi->fd);
	}

	iscsi->fd  = -1;
	iscsi->is_connected = 0;
	iscsi->is_corked = 0;

	return 0;
}

static struct iscsi_pdu*
iscsi_iser_new_pdu(struct iscsi_context *iscsi, __attribute__((unused))size_t size) {

	struct iscsi_pdu *pdu;
	struct iser_pdu *iser_pdu;

	iser_pdu = iscsi_szmalloc(iscsi, sizeof(*iser_pdu));
	pdu = &iser_pdu->iscsi_pdu;
	pdu->indata.data = NULL;

	return pdu;
}

static void
iscsi_iser_free_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	struct iser_pdu *iser_pdu;

	if (pdu == NULL) {
		iscsi_set_error(iscsi, "trying to free NULL pdu");
		return;
	}

	iser_pdu = container_of(pdu, struct iser_pdu, iscsi_pdu);

	if (iser_pdu->desc != NULL) {
		iser_tx_desc_free(iscsi, iser_pdu->desc);
		iser_pdu->desc = NULL;
	}

	if (pdu->outdata.size <= iscsi->smalloc_size) {
		iscsi_sfree(iscsi, pdu->outdata.data);
	} else {
		iscsi_free(iscsi, pdu->outdata.data);
	}
	pdu->outdata.data = NULL;

	if (pdu->indata.size <= iscsi->smalloc_size) {
		iscsi_sfree(iscsi, pdu->indata.data);
	} else {
		iscsi_free(iscsi, pdu->indata.data);
	}

	pdu->indata.data = NULL;

	if (iscsi->outqueue_current == pdu) {
		iscsi->outqueue_current = NULL;
	}

	iscsi_sfree(iscsi, iser_pdu);
}

/**
 ** iser_create_send_desc() - creating send descriptors
 **			   headers
 ** @iser_pdu:    iser pdu including iscsi pdu inside it
 **/
static void
iser_create_send_desc(struct iser_pdu *iser_pdu) {

	unsigned char *iscsi_header = iser_pdu->iscsi_pdu.outdata.data;
	struct iser_tx_desc *tx_desc = iser_pdu->desc;

	memset(&tx_desc->iser_header, 0, sizeof(struct iser_hdr));
	tx_desc->iser_header.flags = ISER_VER;
	tx_desc->num_sge = 1;
	memcpy(tx_desc->iscsi_header, iscsi_header, ISCSI_RAW_HEADER_SIZE);
}

/*
 * iser_post_recvl() - posting login buffer receive request
 *                     on receive queue
 * @iser_conn:    ib connection context
 */
static int
iser_post_recvl(struct iser_conn *iser_conn) {
	struct ibv_recv_wr rx_wr;
	struct ibv_recv_wr *rx_wr_failed;
	struct ibv_sge sge;
	int ret;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	sge.addr   = (uintptr_t)iser_conn->login_resp_buf;
	sge.length = ISER_RX_LOGIN_SIZE;
	sge.lkey   = iser_conn->login_resp_mr->lkey;

	rx_wr.wr_id   = (uintptr_t)iser_conn->login_resp_buf;
	rx_wr.sg_list = &sge;
	rx_wr.num_sge = 1;
	rx_wr.next = NULL;

	iser_conn->post_recv_buf_count++;
	ret = ibv_post_recv(iser_conn->qp, &rx_wr, &rx_wr_failed);
	if (ret) {
		iscsi_set_error(iscsi, "Failed to post recv login response\n");
		iser_conn->post_recv_buf_count--;
		return -1;
	}

	return 0;
}

/*
 * iser_post_send() - posting send requests
 *                    on send queue
 * @iser_conn:        ib connection context
 * @iser_tx_desc:   send descriptor
 * @signal:         signal completion or not
 *
 * Notes:
 * Need to handle signal better
 */
static int
iser_post_send(struct iser_conn *iser_conn, struct iser_tx_desc *tx_desc, bool signal) {

	int ret;
	struct ibv_send_wr send_wr;
	struct ibv_send_wr *send_wr_failed;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	memset(&send_wr, 0, sizeof(send_wr));
	send_wr.next       = NULL;
	send_wr.wr_id      = (uintptr_t)tx_desc;
	send_wr.sg_list    = tx_desc->tx_sg;
	send_wr.num_sge    = tx_desc->num_sge;
	send_wr.opcode     = IBV_WR_SEND;
	send_wr.send_flags = signal ? IBV_SEND_SIGNALED : 0;

	ret = ibv_post_send(iser_conn->qp, &send_wr, &send_wr_failed);
	if (ret) {
		iscsi_set_error(iscsi, "Failed to post send\n");
		return -1;
	}

	return 0;
}

static inline int
get_data_size(struct iser_pdu *iser_pdu)
{
	if (!iser_pdu->iscsi_pdu.scsi_cbdata.task)
		return iser_pdu->iscsi_pdu.outdata.size - ISCSI_RAW_HEADER_SIZE;

	return iser_pdu->iscsi_pdu.scsi_cbdata.task->expxferlen;
}

/*
 * iser_send_control() - sending iscsi pdu of type CONTROL
 *
 * @iser_transport:    iser connection context
 * @iser_pdu:     iser pdu to send
 */
static int
iser_send_control(struct iser_conn *iser_conn, struct iser_pdu *iser_pdu) {

	struct iser_tx_desc *tx_desc;
	int ret;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;
	size_t datalen;

	if (iser_pdu == NULL) {
		iscsi_set_error(iscsi, "Failed in iser_pdu");
		return -1;
	}
	datalen = iser_pdu->iscsi_pdu.outdata.size - ISCSI_RAW_HEADER_SIZE;
	tx_desc = iser_pdu->desc;
	tx_desc->type = ISCSI_CONTROL;

	iser_create_send_desc(iser_pdu);

	if (datalen > 0) {
		char* data = (char*)&iser_pdu->iscsi_pdu.outdata.data[ISCSI_RAW_HEADER_SIZE];
		struct ibv_sge *tx_dsg = &tx_desc->tx_sg[1];

		memcpy(tx_desc->data_buff, data, datalen);

		tx_dsg->addr       = (uintptr_t)tx_desc->data_buff;
		tx_dsg->length     = datalen;
		tx_dsg->lkey       = tx_desc->data_mr->lkey;
		tx_desc->num_sge   = 2;
	}

	ret = iser_post_send(iser_conn, tx_desc, true);
	if (ret) {
		iscsi_set_error(iscsi, "Failed to post send");
		return -1;
	}

	return 0;

}

/*
 * iser_initialize_headers() - Initialize task headers
 *
 * @iser_pdu:     iser pdu
 * @iser_conn:       iser_connection context
 */
static int
iser_initialize_headers(struct iser_pdu *iser_pdu, struct iscsi_context *iscsi)
{
	struct iser_tx_desc *tx_desc;

	tx_desc = iser_tx_desc_alloc(iscsi, get_data_size(iser_pdu));
	if (tx_desc == NULL) {
		return -1;
	}

	iser_pdu->desc = tx_desc;

	tx_desc->tx_sg[0].addr   = (uintptr_t)tx_desc;
	tx_desc->tx_sg[0].length = ISER_HEADERS_LEN;
	tx_desc->tx_sg[0].lkey   = tx_desc->hdr_mr->lkey;

	return 0;
}

/*
 * iser_prepare_read_cmd() - prepareing read command
 *
 * @iser_conn:      ib connection context
 * @iser_pdu:     iser_pdu
 *
 * Notes:
 * In case there isn't buffer from app
 * we create buffer.
 */
static int
iser_prepare_read_cmd(struct iser_conn *iser_conn,struct iser_pdu *iser_pdu)
{
	struct iser_hdr *hdr = &iser_pdu->desc->iser_header;
	struct iser_tx_desc *tx_desc = iser_pdu->desc;
	struct scsi_task *task = iser_pdu->iscsi_pdu.scsi_cbdata.task;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;
	size_t data_size = task->expxferlen;

	if (data_size > 0) {

		if (task->iovector_in.iov == NULL) {
			iser_pdu->iscsi_pdu.indata.data = iscsi_malloc(iscsi, data_size);
			if (iser_pdu->iscsi_pdu.indata.data == NULL) {
				iscsi_set_error(iscsi, "Failed to aloocate data buffer");
				return -1;
			}
			iser_pdu->iscsi_pdu.indata.size = data_size;
		}

		tx_desc->data_dir = DATA_READ;
		hdr->read_va = htobe64((intptr_t)tx_desc->data_buff);
		hdr->read_stag = htobe32((uint32_t)tx_desc->data_mr->rkey);
		hdr->flags |= ISER_RSV;

		return 0;

	} else {
		iscsi_set_error(iscsi, "Read command with no expected transfer length");
		return -1;
	}
}

/*
 * iser_prepare_write_cmd() - preparing write command
 *
 * @iser_conn:	  ib connection context
 * @iser_pdu:	 iser pdu
 */
static int
iser_prepare_write_cmd(struct iser_conn *iser_conn, struct iser_pdu *iser_pdu)
{
	struct iser_hdr *hdr = &iser_pdu->desc->iser_header;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;
	struct iser_tx_desc *tx_desc = iser_pdu->desc;
	struct iscsi_pdu *iscsi_pdu = &iser_pdu->iscsi_pdu;
	struct scsi_iovector *iovector = iscsi_get_scsi_task_iovector_out(iscsi, iscsi_pdu);
	int i, offset = 0;

	if (iovector == NULL) {
		iscsi_set_error(iscsi, "Can't find iovector data for DATA-OUT (RDMA)");
		return -1;
	}

	tx_desc->data_dir = DATA_WRITE;

	for(i = 0 ; i < iovector->niov ; i++) {
		memcpy(&tx_desc->data_buff[offset], iovector->iov[i].iov_base, iovector->iov[i].iov_len);
		offset += iovector->iov[i].iov_len;
	}

	hdr->flags     |= ISER_WSV;
	hdr->write_stag = htobe32((uint32_t)(tx_desc->data_mr->rkey));

	// ImmediateData
	if (iscsi_pdu->payload_len > 0) {
		struct ibv_sge *tx_dsg = &tx_desc->tx_sg[1];

		tx_dsg->addr     = (uintptr_t)tx_desc->data_buff;
		tx_dsg->length   = iscsi_pdu->payload_len;
		tx_dsg->lkey     = tx_desc->data_mr->lkey;
		tx_desc->num_sge = 2;

		hdr->write_va = htobe64((intptr_t)(tx_desc->data_buff + iscsi_pdu->payload_len));
	} else {
		hdr->write_va = htobe64((intptr_t)(tx_desc->data_buff));
	}

	return 0;
}

/*
 * is_control_opcode() - check if iscsi opcode is of type CONTROL
 *
 * @opcode:           iscsi opcode
 */
static bool
is_control_opcode(uint8_t opcode)
{
	bool is_control = false;

	switch (opcode & ISCSI_PDU_REJECT) {
		case ISCSI_PDU_NOP_OUT:
		case ISCSI_PDU_LOGIN_REQUEST:
		case ISCSI_PDU_LOGOUT_REQUEST:
		case ISCSI_PDU_TEXT_REQUEST:
			is_control = true;
			break;
		case ISCSI_PDU_SCSI_REQUEST:
			is_control = false;
			break;
		default:
			is_control = false;
	}

	return is_control;
}

/*
 * iser_send_command() - sending iscsi pdu of type COMMAND
 *
 * @iser_transport:    iser connection context
 * @iser_pdu:     iser pdu to send
 *
 * Nots:
 * Need to fix if failed prepareation return -1
 */
static int
iser_send_command(struct iser_conn *iser_conn, struct iser_pdu *iser_pdu)
{
	struct iser_tx_desc *tx_desc = iser_pdu->desc;
	int err = 0;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	tx_desc->type = ISCSI_COMMAND;

	iser_create_send_desc(iser_pdu);

	if (iser_pdu->desc->iscsi_header[1] & BHSSC_FLAGS_R) {
		err = iser_prepare_read_cmd(iser_conn, iser_pdu);
		if (err) {
			iscsi_set_error(iscsi, "error in prepare read cmd\n");
			return -1;
		}
	} else if (iser_pdu->desc->iscsi_header[1] & BHSSC_FLAGS_W) {
		err = iser_prepare_write_cmd(iser_conn, iser_pdu);
		if (err) {
			iscsi_set_error(iscsi, "error in prepare write cmd\n");
			return -1;
		}
	} else
		iser_pdu->desc->data_dir = DATA_NONE;

	err = iser_post_send(iser_conn, tx_desc, true);
	if (err)
		return -1;
	return 0;
}

static int
iscsi_iser_send_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu) {
	struct iser_pdu *iser_pdu;
	struct iser_conn *iser_conn = iscsi->opaque;
	uint8_t opcode;

	iser_pdu = container_of(pdu, struct iser_pdu, iscsi_pdu);
	opcode = pdu->outdata.data[0];

	iscsi_pdu_set_expstatsn(pdu, iscsi->statsn + 1);
	ISCSI_LIST_ADD_END(&iscsi->waitpdu, pdu);

	/*
         * because of async reconnection, before reconnecting successfully,
	 * the argument 'iscsi' is the 'old_iscsi' with a empty connection.
	 */
	if (!iser_conn)
		return 0;

	if (iser_initialize_headers(iser_pdu, iscsi)) {
		iscsi_set_error(iscsi, "initialize headers Failed\n");
		return -1;
	}

	if (unlikely(is_control_opcode(opcode))) {
		if (iser_send_control(iser_conn, iser_pdu)) {
			iscsi_set_error(iscsi, "iser_send_command Failed\n");
			return -1;
		}
	} else {
		if (iser_send_command(iser_conn, iser_pdu)) {
			iscsi_set_error(iscsi, "iser_send_command Failed\n");
			return -1;
		}
	}

	return 0;
}

static int
iscsi_iser_revive_queued_pdus(struct iscsi_context *iscsi) {
	struct iscsi_pdu *pdu;

	while (iscsi->outqueue != NULL) {
		if (iscsi_serial32_compare(iscsi->outqueue->cmdsn, iscsi->maxcmdsn) > 0) {
			break;
		}

		pdu = iscsi->outqueue;
		ISCSI_LIST_REMOVE(&iscsi->outqueue, pdu);

		if (iscsi_iser_send_pdu(iscsi, pdu) < 0) {
			ISCSI_LIST_ADD(&iscsi->outqueue, pdu);
			return -1;
		}
	}

	return 0;
}


/*
 * iser_queue_pdu() - sending iscsi pdu
 *
 * @iscsi_context:    iscsi context
 * @iscsi_pdu:     iscsi pdu
 *
 * Notes:
 * Need to be compatible to TCP which has real queue,
 * in iSER pdus with cmdsn not exceeds maxcmdsn are already sent.
 */
static int
iscsi_iser_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu) {
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "trying to queue NULL pdu");
		return -1;
	}

	if (pdu->outdata.data[0] == ISCSI_PDU_NOP_OUT &&
		iscsi_iser_cm_event(iscsi) != 0) {
		iscsi_service_reconnect_if_loggedin(iscsi);
		return -1;
	}

	if (iscsi->outqueue != NULL ||
		(iscsi_serial32_compare(pdu->cmdsn, iscsi->maxcmdsn) > 0
		 && !(pdu->outdata.data[0] & ISCSI_PDU_IMMEDIATE))) {
		iscsi_add_to_outqueue(iscsi, pdu);
		return 0;
	}

	return iscsi_iser_send_pdu(iscsi, pdu);
}


/*
 * iser_create_iser_conn_res() - creating ib connections resources
 *
 * @iser_conn:    ib connection context
 */
static int iser_create_iser_conn_res(struct iser_conn *iser_conn) {

	struct ibv_qp_init_attr init_attr;
	int ret;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	memset(&init_attr, 0, sizeof(struct ibv_qp_init_attr));
	init_attr.qp_context       = (void *)iser_conn->cma_id->context;
	init_attr.send_cq	  = iser_conn->cq;
	init_attr.recv_cq	  = iser_conn->cq;
	init_attr.cap.max_send_wr  = ISER_QP_MAX_RECV_DTOS;
	init_attr.cap.max_recv_wr  = ISER_QP_MAX_RECV_DTOS;
	init_attr.cap.max_send_sge = 2;
	init_attr.cap.max_recv_sge = 1;
	init_attr.qp_type	  = IBV_QPT_RC;

	ret = rdma_create_qp(iser_conn->cma_id, iser_conn->pd, &init_attr);
	if (ret) {
		iscsi_set_error(iscsi, "Failed to create qp\n");
		return -1;
	}
	iser_conn->qp = iser_conn->cma_id->qp;

	return ret;
}

void iscsi_set_rdma_ack_timetout(struct iscsi_context *iscsi, unsigned char value)
{
	iscsi->rdma_ack_timeout = value;
	ISCSI_LOG(iscsi, 2, "RDMA ACK TIMEOUT will be set to %d on next rdma qp creation", value);
}

/*
 * iser_rdma_set_option() - set rdma options
 * currently support RDMA ack timeout.
 * @cma_id:    connection manager id
 * @iscsi:   iscsi context
 */
int iser_rdma_set_option(struct rdma_cm_id *cma_id, struct iscsi_context *iscsi) {
	int ret = 0;
	uint8_t timeout = iscsi->rdma_ack_timeout;

	if (timeout) {
#ifdef HAVE_RDMA_ACK_TIMEOUT
		/* calculate according to the formula 4.096 * 2^(timeout) usec.
		   Ex, 8 means 1048.576 usec (0.00104 sec) */
		ret = rdma_set_option(cma_id, RDMA_OPTION_ID, RDMA_OPTION_ID_ACK_TIMEOUT, &timeout, sizeof(timeout));
		if (ret) {
			ISCSI_LOG(iscsi, 1, "failed to set RDMA ACK TIMEOUT: %s", strerror(errno));
		} else {
			ISCSI_LOG(iscsi, 3, "RDMA ACK TIMEOUT set to %d", timeout);
		}
#else
		ISCSI_LOG(iscsi, 1, "failed to set RDMA ACK TIMEOUT(%d) for cma_id(%p), please update your kernel&librdmacm", timeout, cma_id);
#endif
	}

	return ret;
}

/*
 * iser_addr_handler() - handles RDMA_CM_EVENT_ADDR_RESOLVED
 *                       event in rdma_cm
 *
 * @cma_id:    connection manager id
 */
static int iser_addr_handler(struct rdma_cm_id *cma_id) {
	struct iscsi_context *iscsi = cma_id->context;
	struct iser_conn *iser_conn = iscsi->opaque;
	int ret, flags;

	iser_rdma_set_option(cma_id, iscsi);

	ret = rdma_resolve_route(cma_id, 1000);
	if (ret) {
		iscsi_set_error(iscsi, "Failed resolving address\n");
		return -1;
	}

	iser_conn->pd = ibv_alloc_pd(cma_id->verbs);
	if (!iser_conn->pd) {
		iscsi_set_error(iscsi, "Failed to alloc pd\n");
		return -1;
	}
	iser_conn->comp_channel = ibv_create_comp_channel(cma_id->verbs);
	if (!iser_conn->comp_channel) {
		iscsi_set_error(iscsi, "Failed to create comp channel");
		goto pd_error;
	}

	flags = fcntl(iser_conn->comp_channel->fd, F_GETFL);
	ret = fcntl(iser_conn->comp_channel->fd, F_SETFL, flags | O_NONBLOCK);
	if (ret) {
		iscsi_set_error(iscsi, "Failed to set channel to nonblocking");
		return -1;
	}

	iser_conn->cq = ibv_create_cq(cma_id->verbs,
				      ISER_MAX_CQ_LEN,
				      iser_conn,
				      iser_conn->comp_channel,
				      0);
	if (!iser_conn->cq) {
		iscsi_set_error(iscsi, "Failed to create cq\n");
		goto pd_error;
	}
	iser_conn->cq_nevents = 0;

	if (ibv_req_notify_cq(iser_conn->cq, 0)) {
		iscsi_set_error(iscsi, "ibv_req_notify_cq failed\n");
		goto cq_error;
	}

	iser_conn->login_resp_buf = iscsi_malloc(iscsi, ISER_RX_LOGIN_SIZE);
	if (!iser_conn->login_resp_buf) {
		iscsi_set_error(iscsi, "Failed to allocate memory for login_resp_buf\n");
		goto cq_error;
	}

	iser_conn->login_resp_mr = ibv_reg_mr(iser_conn->pd, iser_conn->login_resp_buf,
					    ISER_RX_LOGIN_SIZE, IBV_ACCESS_LOCAL_WRITE);
	if(!iser_conn->login_resp_mr) {
		iscsi_set_error(iscsi, "Failed to reg login_resp_mr\n");
		iscsi_free(iscsi, iser_conn->login_resp_buf);
		goto cq_error;
	}

	return 0;

cq_error:
	ibv_destroy_cq(iser_conn->cq);

pd_error:
	ibv_dealloc_pd(iser_conn->pd);

	return -1;
}
/*
 * iser_route_handler() - handles RDMA_CM_EVENT_ROUTE_RESOLVED
 *                        event in rdma_cm
 *
 * @cma_id:    connection manager id
 */
static int iser_route_handler(struct rdma_cm_id *cma_id) {
	struct rdma_conn_param conn_param;
	struct iser_cm_hdr req_hdr;
	struct iscsi_context* iscsi = cma_id->context;
	int ret;
	struct iser_conn *iser_conn = iscsi->opaque;

	ret = iser_create_iser_conn_res(iser_conn);
	if (ret) {
		iscsi_set_error(iscsi, "Failed to create ib conn res\n");
		goto login_mr_error;
	}

	memset(&conn_param, 0, sizeof(struct rdma_conn_param));
	conn_param.responder_resources = 4;
	conn_param.retry_count	 = 7;
	conn_param.rnr_retry_count     = 6;

	memset(&req_hdr, 0, sizeof(req_hdr));
	req_hdr.flags = (ISER_ZBVA_NOT_SUPPORTED |
			 ISER_SEND_W_INV_NOT_SUPPORTED);
	conn_param.private_data	 = (void *)&req_hdr;
	conn_param.private_data_len     = sizeof(struct iser_cm_hdr);

	ret = rdma_connect(cma_id, &conn_param);
	if (ret) {
		iscsi_set_error(iscsi, "conn %p failure connecting: %d", iser_conn, ret);
		return -1;
	}
	iser_conn->rdma_connect_sent = 1;
	return ret;

login_mr_error:
	ibv_dereg_mr(iser_conn->login_resp_mr);

	ibv_destroy_cq(iser_conn->cq);

	ibv_dealloc_pd(iser_conn->pd);

	return -1;
}

/*
 * iser_alloc_rx_descriptors() - allocation receive descriptors
 *
 * @iser_conn:     ib connection context
 * @cmds_max:    maximum in flight commands
 */
static int
iser_alloc_rx_descriptors(struct iser_conn *iser_conn, int cmds_max)
{
	int i,j;
	struct iser_rx_desc *rx_desc;
	struct ibv_sge       *rx_sg;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	iser_conn->qp_max_recv_dtos = cmds_max;
	iser_conn->min_posted_rx = ISER_MIN_POSTED_RX;

	iser_conn->num_rx_descs = cmds_max;

	iser_conn->rx_descs = iscsi_malloc(iscsi, iser_conn->num_rx_descs * sizeof(*rx_desc));
	if (!iser_conn->rx_descs) {
		iscsi_set_error(iscsi, "Failed to allocate rx descriptors\n");
		return -1;
	}

	rx_desc = iser_conn->rx_descs;

	for (i = 0; i < iser_conn->qp_max_recv_dtos; i++, rx_desc++)  {
		rx_desc->hdr_mr = ibv_reg_mr(iser_conn->pd, rx_desc, ISER_RX_PAYLOAD_SIZE, IBV_ACCESS_LOCAL_WRITE);

		if (rx_desc->hdr_mr == NULL) {
			iscsi_set_error(iscsi, "Failed to register (%i) reg_mr\n", i);
			goto fail_alloc_mrs;
		}

		rx_sg = &rx_desc->rx_sg;
		rx_sg->addr   = (uintptr_t)rx_desc;
		rx_sg->length = ISER_RX_PAYLOAD_SIZE;
		rx_sg->lkey   = rx_desc->hdr_mr->lkey;
	}

	iser_conn->rx_desc_head = 0;

	return 0;

fail_alloc_mrs:

	rx_desc = iser_conn->rx_descs;
	for (j = 0; j < i ; j++, rx_desc++)
		ibv_dereg_mr(rx_desc->hdr_mr);

	iscsi_free(iscsi, iser_conn->rx_descs);

	return -1;
}

/*
 * iser_post_recvm() - posting receive requests
 *
 * @iser_conn:  ib connection context
 * @count:    amount of receive requests to post on receive queue
 */
static int
iser_post_recvm(struct iser_conn *iser_conn, int count)
{
	struct ibv_recv_wr *rx_wr, *rx_wr_failed;
	int i, ret;
	unsigned int my_rx_head = iser_conn->rx_desc_head;
	struct iser_rx_desc *rx_desc;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	for (rx_wr = iser_conn->rx_wr, i = 0; i < count; i++, rx_wr++) {
		rx_desc		= &iser_conn->rx_descs[my_rx_head];
		rx_wr->wr_id	= (uintptr_t)rx_desc;
		rx_wr->sg_list	= &rx_desc->rx_sg;
		rx_wr->num_sge	= 1;
		rx_wr->next	= rx_wr + 1;
		my_rx_head = (my_rx_head + 1) % iser_conn->qp_max_recv_dtos;
	}

	rx_wr--;
	rx_wr->next = NULL; /* mark end of work requests list */

	iser_conn->post_recv_buf_count += count;
	ret = ibv_post_recv(iser_conn->qp, iser_conn->rx_wr, &rx_wr_failed);
	if (ret) {
		iscsi_set_error(iscsi, "posting %d rx bufs, ib_post_recv failed ret=%d", count, ret);
		iser_conn->post_recv_buf_count -= count;
	} else
		iser_conn->rx_desc_head = my_rx_head;

	return ret;
}

/**
 * iser_rcv_completion() - handling and processing receive completion
 *
 * @rx_desc:       receive descriptor
 * @iser_conn:       ib connection context
 *
 * Notes:
 * After changing mrs to in-advanced mrs need to add
 * commant about memcpy of data from iSER buffer to
 * App buffer
 */
static int
iser_rcv_completion(struct iser_rx_desc *rx_desc,
		    struct iser_conn *iser_conn)
{
	struct iscsi_in_pdu in;
	int empty, err;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	in.hdr = (unsigned char*)rx_desc->iscsi_header;
	in.data_pos = iscsi_get_pdu_data_size(&in.hdr[0]);
	in.data = (unsigned char*)rx_desc->data;

	enum iscsi_opcode opcode = in.hdr[0] & 0x3f;
	uint32_t itt = scsi_get_uint32(&in.hdr[16]);

	if (opcode == ISCSI_PDU_NOP_IN && itt == 0xffffffff)
		goto no_waitpdu;

	if (opcode == ISCSI_PDU_ASYNC_MSG)
		goto no_waitpdu;

	struct iscsi_pdu *iscsi_pdu;
	struct iser_pdu *iser_pdu;
	for (iscsi_pdu = iscsi->waitpdu ; iscsi_pdu ; iscsi_pdu = iscsi_pdu->next) {
		if(iscsi_pdu->itt == itt)
			break;
	}

	iser_pdu = container_of(iscsi_pdu, struct iser_pdu, iscsi_pdu);

	/* in case of read completion we need to copy data     *
	 * from pre-allocated buffers into application buffers */

	if (iser_pdu->desc->type == ISCSI_COMMAND &&
		iser_pdu->desc->data_dir == DATA_READ) {

		int i, offset = 0;
		struct scsi_task *task = iser_pdu->iscsi_pdu.scsi_cbdata.task;
		struct scsi_iovector *iovector_in = &task->iovector_in;

		if (iovector_in->iov == NULL) {
			memcpy(iser_pdu->iscsi_pdu.indata.data, &iser_pdu->desc->data_buff[offset],
				 iser_pdu->iscsi_pdu.indata.size);
		} else {
			for (i = 0 ; i < iovector_in->niov ; i++) {
				memcpy(iovector_in->iov[i].iov_base, &iser_pdu->desc->data_buff[offset],
					iovector_in->iov[i].iov_len);
				offset += iovector_in->iov[i].iov_len;
			}
		}
	}

no_waitpdu:
	/* decrementing conn->post_recv_buf_count only --after-- freeing the   *
	 * task eliminates the need to worry on tasks which are completed in   *
	 * parallel to the execution of iser_conn_term. So the code that waits *
	 * for the posted rx bufs refcount to become zero handles everything   */
	iser_conn->post_recv_buf_count--;

	if ((unsigned char *)rx_desc != iser_conn->login_resp_buf) {
		empty = iser_conn->qp_max_recv_dtos - iser_conn->post_recv_buf_count;
		if (empty >= iser_conn->min_posted_rx) {
			err = iser_post_recvm(iser_conn, empty);
			if (err) {
				return -1;
			}
		}
	} else if (iscsi->is_loggedin) {
		if(iser_alloc_rx_descriptors(iser_conn, 255)) {
			iscsi_set_error(iscsi, "iser_alloc_rx_descriptors Failed\n");
			return -1;
		}
		err = iser_post_recvm(iser_conn, ISER_MIN_POSTED_RX);
		if (err) {
			return -1;
		}
	} else {
		if (iser_post_recvl(iser_conn)) {
			iscsi_set_error(iscsi, "Failed Post Recv login");
			return -1;
		}
	}

	return iscsi_process_pdu(iscsi, &in);
}

/**
 * iser_sndcompletion() - handling send completion
 *
 * @tx_desc:       send descriptor
 * @iser_conn:       ib connection context
 *
 */
static int
iser_snd_completion(struct iser_tx_desc *tx_desc,
		    struct iser_conn *iser_conn)
{
	return 0;
}

/**
 * iser_handle_wc() - handling work completion
 *
 * @wc:            work completion
 * @iser_conn:       ib connection context
 *
 */
static int iser_handle_wc(struct ibv_wc *wc,struct iser_conn *iser_conn)
{
	struct iser_tx_desc *tx_desc;
	struct iser_rx_desc *rx_desc;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	if (wc->status == IBV_WC_SUCCESS) {
		if (wc->opcode == IBV_WC_RECV) {
			rx_desc = (struct iser_rx_desc *)(uintptr_t)wc->wr_id;

			return iser_rcv_completion(rx_desc, iser_conn);
		} else
		if (wc->opcode == IBV_WC_SEND) {
			tx_desc = (struct iser_tx_desc *)(uintptr_t)wc->wr_id;

			return iser_snd_completion(tx_desc, iser_conn);
		} else {
			iscsi_set_error(iscsi, "Unknown wc opcode %d\n", wc->opcode);

			return -1;
		}
	} else {
		if (wc->status != IBV_WC_WR_FLUSH_ERR) {
			ISCSI_LOG(iscsi, 3, "wr id %lx status %d vend_err %x\n",
					wc->wr_id, wc->status, wc->vendor_err);
			return iscsi_service_reconnect_if_loggedin(iscsi);
		} else {
			iscsi_set_error(iscsi, "flush error: wr id %" PRIx64 "\n", wc->wr_id);

			return 0;
		}
	}
}

/**
 * cq_event_handler() - polling and handling completions
 *
 * @iser_conn:       ib connection context
 *
 * Notes:
 * Need to decide about how much competion to poll
 * each time.
 */
static int cq_event_handler(struct iser_conn *iser_conn)
{
	struct ibv_wc wc[16];
	unsigned int i;
	unsigned int n;
	unsigned int completed = 0;

	while ((n = ibv_poll_cq(iser_conn->cq, 16, wc)) > 0) {
		for (i = 0; i < n; i++)
			if (iser_handle_wc(&wc[i], iser_conn))
				return -1;

		completed += n;
		if (completed >= 512)
			break;
	}

	return 0;
}

/**
 * cq_handle() - handling completion queue event
 *
 * @iser_conn:       ib connection context
 *
 * Notes:
 * Need to check if it is possible
 * to get cq event except POLLIN.
 */
static int cq_handle(struct iser_conn *iser_conn)
{
	void *ev_ctx = NULL;
	int ret;
	struct iscsi_context *iscsi = iser_conn->cma_id->context;

	ret = ibv_get_cq_event(iser_conn->comp_channel, &iser_conn->cq, &ev_ctx);
	if (ret) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			iscsi_set_error(iscsi, "failed get cq event %s", strerror(errno));
			return -1;
		}
		goto out;
	}

	ret = ibv_req_notify_cq(iser_conn->cq, 0);

	if (++iser_conn->cq_nevents >= INT_MAX) {
		ibv_ack_cq_events(iser_conn->cq, iser_conn->cq_nevents);
		iser_conn->cq_nevents = 0;
	}

	if (ret) {
		iscsi_set_error(iscsi, "failed notify or ack CQ");
		return -1;
	}

out:
	ret = cq_event_handler(iser_conn);
	if (ret) {
		iscsi_set_error(iscsi, "failed CQ handler");
		return -1;
	}

	return 0;
}

/*
 * iser_connected_handler() - handles RDMA_CM_EVENT_ESTABLISHED
 *                            event in rdma_cm
 *
 * @cma_id:    connection manager id
 *
 */
static int iser_connected_handler(struct rdma_cm_id *cma_id) {

	struct iscsi_context *iscsi = cma_id->context;
	struct iser_conn *iser_conn = iscsi->opaque;

	if (dup2(iser_conn->comp_channel->fd, iscsi->fd) == -1) {
		return -1;
	}

	iser_conn->post_recv_buf_count = 0;
	iscsi->is_connected = 1;

	if (iser_post_recvl(iser_conn)) {
		iscsi_set_error(iscsi, "Failed Post Recv login");
		return -1;
	}

	return 0;
}

/*
 * iser_cma_handler() - handles rdma connection manager events
 *
 * @iser_conn:   ib connection context
 * @cma_id:    connection manager id
 * @event:     rdma cm event
 *
 */
static int
iser_cma_handler(struct rdma_cm_id *cma_id, struct rdma_cm_event *event) {
	int ret = 0;

	switch(event->event) {

		case RDMA_CM_EVENT_ADDR_RESOLVED:
			ret = iser_addr_handler(cma_id);
			break;
		case RDMA_CM_EVENT_ROUTE_RESOLVED:
			ret = iser_route_handler(cma_id);
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			ret = iser_connected_handler(cma_id);
			break;
		case RDMA_CM_EVENT_ADDR_ERROR:
		case RDMA_CM_EVENT_ROUTE_ERROR:
		case RDMA_CM_EVENT_CONNECT_ERROR:
		case RDMA_CM_EVENT_UNREACHABLE:
		case RDMA_CM_EVENT_REJECTED:
			ret = -1;
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
		case RDMA_CM_EVENT_ADDR_CHANGE:
		case RDMA_CM_EVENT_TIMEWAIT_EXIT:
			ret = -1;
			break;
		default:
			ret = -1;
			break;
	}
	return ret;
}

static int
iscsi_iser_cm_event(struct iscsi_context *iscsi) {
	struct iser_conn *iser_conn = iscsi->opaque;
	struct rdma_event_channel *channel = iser_conn->cma_channel;
	struct rdma_cm_event *event;
	int ret;

	while (rdma_get_cm_event(channel, &event) == 0) {
		ret = iser_cma_handler(iser_conn->cma_id, event);
		rdma_ack_cm_event(event);

		if (ret) {
			if (iscsi->socket_status_cb != NULL) {
				/* connect failed, cleanup itself */
				if (iser_conn->cma_id) {
					iser_conn_terminate(iser_conn);
					iser_conn_release(iscsi);
					iser_conn->cma_id = NULL;
				}

				iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR, NULL, iscsi->connect_data);
				iscsi->socket_status_cb = NULL;
			}
			return -1;
		}

		if (iscsi->is_connected && iscsi->socket_status_cb != NULL) {
			iscsi->socket_status_cb(iscsi, SCSI_STATUS_GOOD, NULL, iscsi->connect_data);
			iscsi->socket_status_cb = NULL;
		}
	}

	if (errno != EAGAIN && errno != EWOULDBLOCK) {
		iscsi_set_error(iscsi, "Failed to get RDMA-CM Event\n");
		return -1;
	}

	return 0;
}

/*
 * iscsi_iser_connect() - creating rdma connection manager
 *                        and connection it to target.
 *
 * @iscsi:    iscsi context
 * @sa:       socket address for rdma cm connect
 * @ai_family unused
 *
 * Notes:
 * Need to move iser_reg_mr(headers) to iser_connected_handler.
 */
static int
iscsi_iser_connect(struct iscsi_context *iscsi, union socket_address *sa,__attribute__((unused)) int ai_family) {

	struct iser_conn *iser_conn = iscsi->opaque;
	int flag;

	if (iscsi->old_iscsi && iscsi->fd != iscsi->old_iscsi->fd) {
		struct iscsi_context *old_iscsi = iscsi->old_iscsi;
		struct iser_conn *old_iser_conn = old_iscsi->opaque;

		iscsi->fd = old_iscsi->fd;

		if (old_iser_conn) {
			if (old_iser_conn->cma_id) {
				iser_conn_terminate(old_iser_conn);
				iser_conn_release(old_iscsi);
			}

			iscsi_free(old_iscsi, old_iser_conn);
			old_iscsi->opaque = NULL;
		}
	}

	/* create mock file descriptor */
	if (iscsi->fd == -1) {
		iscsi->fd = eventfd(0, EFD_CLOEXEC);

		if (iscsi->fd < 0) {
			return -1;
		}
	}

	iser_conn->cma_channel = rdma_create_event_channel();

	if (iser_conn->cma_channel == NULL) {
		iscsi_set_error(iscsi, "Failed creating Event Channel\n");
		return -1;
	}

	flag = fcntl(iser_conn->cma_channel->fd, F_GETFL);
	if (fcntl(iser_conn->cma_channel->fd, F_SETFL, flag | O_NONBLOCK) < 0) {
		iscsi_set_error(iscsi, "Cannot set event channel to non blocking");
		goto error;
	}

	if (rdma_create_id(iser_conn->cma_channel, &iser_conn->cma_id, (void *)iscsi, RDMA_PS_TCP)) {
		iscsi_set_error(iscsi, "Failed create channel_id");
		goto error;
	}

	if (dup2(iser_conn->cma_channel->fd, iscsi->fd) < 0) {
		iscsi_set_error(iscsi, "Failed dup event channel fd");
		goto error;
	}

	if(rdma_resolve_addr(iser_conn->cma_id, NULL, &sa->sa, 2000)) {
		iscsi_set_error(iscsi, "Failed resolve address");
		goto error;
	}

	return 0;

error:
	if (iser_conn->cma_id) {
		rdma_destroy_id(iser_conn->cma_id);
		iser_conn->cma_id = NULL;
	}
	if (iser_conn->cma_channel) {
		rdma_destroy_event_channel(iser_conn->cma_channel);
		iser_conn->cma_channel = NULL;
	}

	return -1;
}

static iscsi_transport iscsi_transport_iser = {
	.connect      = iscsi_iser_connect,
	.queue_pdu    = iscsi_iser_queue_pdu,
	.new_pdu      = iscsi_iser_new_pdu,
	.disconnect   = iscsi_iser_disconnect,
	.free_pdu     = iscsi_iser_free_pdu,
	.service      = iscsi_iser_service,
	.get_fd       = iscsi_iser_get_fd,
	.which_events = iscsi_iser_which_events,
};

void iscsi_init_iser_transport(struct iscsi_context *iscsi)
{
	iscsi->drv = &iscsi_transport_iser;
	iscsi->opaque = iscsi_zmalloc(iscsi, sizeof(struct iser_conn));
	iscsi->transport = ISER_TRANSPORT;
	/* Update iSCSI params as per iSER transport */
	iscsi->initiator_max_recv_data_segment_length = ISCSI_DEF_MAX_RECV_SEG_LEN;
	iscsi->target_max_recv_data_segment_length = ISCSI_DEF_MAX_RECV_SEG_LEN;

	/* ensure smalloc_size is enough for iser_pdu */
	while (iscsi->smalloc_size < sizeof(struct iser_pdu)) {
		iscsi->smalloc_size <<= 1;
	}
}

#endif
