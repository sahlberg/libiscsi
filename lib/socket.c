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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef AROS
#include "aros/aros_compat.h"
#endif

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include "win32/win32_compat.h"
#else
#include <strings.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#endif

#ifdef NEED_SYS_FILIO_H
#include <sys/filio.h>
#endif

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#ifdef HAVE_PTHREAD
#include <signal.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include "scsi-lowlevel.h"
#include "iscsi.h"
#include "iscsi-private.h"
#include "slist.h"

static uint32_t iface_rr = 0;
struct iscsi_transport;

/* MUST keep in sync with iser.c */
union socket_address {
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr sa;
};

void
iscsi_add_to_outqueue(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	struct iscsi_pdu *current;
	struct iscsi_pdu *last = NULL;

	if (iscsi->scsi_timeout > 0) {
		pdu->scsi_timeout = time(NULL) + iscsi->scsi_timeout;
	} else {
		pdu->scsi_timeout = 0;
	}

        iscsi_mt_spin_lock(&iscsi->iscsi_lock);

        current = iscsi->outqueue;
        if (iscsi->outqueue == NULL) {
		iscsi->outqueue = pdu;
		pdu->next = NULL;
                goto finished;
	}
        
	/* queue pdus in ascending order of CmdSN. 
	 * ensure that pakets with the same CmdSN are kept in FIFO order.
	 * immediate PDUs are queued in front of queue with the CmdSN
	 * of the first cmd pdu in the outqueue.
	 */

	if (pdu->outdata.data[0] & ISCSI_PDU_IMMEDIATE) {
		do {
			if ((current->outdata.data[0] & 0x3f) != ISCSI_PDU_DATA_OUT) {
				iscsi_pdu_set_cmdsn(pdu, current->cmdsn);
				break;
			}
			current = current->next;
		} while (current != NULL);

		current = iscsi->outqueue;
	}

	do {
		if (iscsi_serial32_compare(pdu->cmdsn, current->cmdsn) < 0 ||
			(pdu->outdata.data[0] & ISCSI_PDU_IMMEDIATE && !(current->outdata.data[0] & ISCSI_PDU_IMMEDIATE))) {
			/* insert PDU before the current */
			if (last != NULL) {
				last->next = pdu;
			} else {
				iscsi->outqueue = pdu;
			}
			pdu->next = current;
                        goto finished;
		}
		last = current;
		current = current->next;
	} while (current != NULL);
	
	last->next = pdu;
	pdu->next = NULL;

 finished:
        iscsi_mt_spin_unlock(&iscsi->iscsi_lock);
        
#if defined(HAVE_MULTITHREADING) && defined(HAVE_PTHREAD)
        if(iscsi->multithreading_enabled) {
                if (current == NULL && pdu == iscsi->outqueue) {
                        pthread_kill(iscsi->service_thread, SIGUSR1);
                }
        } else {
#endif
                if (iscsi->outqueue == pdu) {
                        iscsi->drv->service(iscsi, POLLOUT);
                }
#if defined(HAVE_MULTITHREADING) && defined(HAVE_PTHREAD)
        }
#endif

        return;
}

void iscsi_decrement_iface_rr() {
        /* TODO QQQ use an atomic here */
	iface_rr--;
}

static int set_nonblocking(int fd)
{
#if defined(_WIN32)
	unsigned long opt = 1;
	return ioctlsocket(fd, FIONBIO, &opt);
#else
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
	return fcntl(fd, F_SETFL, v | O_NONBLOCK);
#endif
}

static int set_tcp_sockopt(int sockfd, int optname, int value)
{
	int level;

	#ifndef SOL_TCP
	struct protoent *buf;

	if ((buf = getprotobyname("tcp")) != NULL)
		level = buf->p_proto;
	else
		return -1;
	#else
		level = SOL_TCP;
	#endif

	return setsockopt(sockfd, level, optname, (char *)&value, sizeof(value));
}

#ifndef TCP_USER_TIMEOUT
#define TCP_USER_TIMEOUT        18
#endif

static int set_tcp_user_timeout(struct iscsi_context *iscsi)
{
	if (set_tcp_sockopt(iscsi->fd, TCP_USER_TIMEOUT, iscsi->tcp_user_timeout) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp user timeout. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "TCP_USER_TIMEOUT set to %d",iscsi->tcp_user_timeout);
	return 0;
}

#ifndef TCP_SYNCNT
#define TCP_SYNCNT        7
#endif

static int set_tcp_syncnt(struct iscsi_context *iscsi)
{
	if (set_tcp_sockopt(iscsi->fd, TCP_SYNCNT, iscsi->tcp_syncnt) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp syn retries. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "TCP_SYNCNT set to %d",iscsi->tcp_syncnt);
	return 0;
}

static int iscsi_tcp_connect(struct iscsi_context *iscsi, union socket_address *sa, int ai_family) {

	int socksize;

	switch (ai_family) {
	case AF_INET:
                socksize = sizeof(struct sockaddr_in);
                break;
	case AF_INET6:
                socksize = sizeof(struct sockaddr_in6);
                break;
        default:
		iscsi_set_error(iscsi, "Unknown address family :%d. "
				"Only IPv4/IPv6 supported so far.",
				ai_family);
                return -1;
        }

	iscsi->fd = socket(ai_family, SOCK_STREAM, 0);
	if (iscsi->fd == -1) {
		iscsi_set_error(iscsi, "Failed to open iscsi socket. "
				"Errno:%s(%d).", strerror(errno), errno);
		return -1;
	}

	if (iscsi->old_iscsi && iscsi->fd != iscsi->old_iscsi->fd) {
		if (iscsi_dup2(iscsi, iscsi->fd, iscsi->old_iscsi->fd) == -1) {
			return -1;
		}
		close(iscsi->fd);
		iscsi->fd = iscsi->old_iscsi->fd;
	}

	iscsi->tcp_nonblocking = !set_nonblocking(iscsi->fd);

	iscsi_set_tcp_keepalive(iscsi, iscsi->tcp_keepidle, iscsi->tcp_keepcnt, iscsi->tcp_keepintvl);

	if (iscsi->tcp_user_timeout > 0) {
		set_tcp_user_timeout(iscsi);
	}

	if (iscsi->tcp_syncnt > 0) {
		set_tcp_syncnt(iscsi);
	}

#if __linux
	if (iscsi->bind_interfaces[0]) {
		char *pchr = iscsi->bind_interfaces, *pchr2;
		int iface_n = iface_rr++%iscsi->bind_interfaces_cnt;
		int iface_c = 0;
		do {
			pchr2 = strchr(pchr,',');
			if (iface_c == iface_n) {
				if (pchr2) pchr2[0]=0x00;
				break;
			}
			if (pchr2) {pchr=pchr2+1;}
			iface_c++;
		} while (pchr2);

		int res = setsockopt(iscsi->fd, SOL_SOCKET, SO_BINDTODEVICE, pchr, strlen(pchr));
		if (res < 0) {
			ISCSI_LOG(iscsi,1,"failed to bind to interface '%s': %s",pchr,strerror(errno));
		} else {
			ISCSI_LOG(iscsi,3,"successfully bound to interface '%s'",pchr);
		}
		if (pchr2) pchr2[0]=',';
	}
#endif

	if (set_tcp_sockopt(iscsi->fd, TCP_NODELAY, 1) != 0) {
		ISCSI_LOG(iscsi,1,"failed to set TCP_NODELAY sockopt: %s",strerror(errno));
	} else {
		ISCSI_LOG(iscsi,3,"TCP_NODELAY set to 1");
	}

	if (connect(iscsi->fd, &sa->sa, socksize) != 0
#if defined(_WIN32)
            && WSAGetLastError() != WSAEWOULDBLOCK
#endif
            && errno != EINPROGRESS) {
		iscsi_set_error(iscsi, "Connect failed with errno : "
			"%s(%d)", strerror(errno), errno);
		close(iscsi->fd);
		iscsi->fd = -1;
		return -1;
	}

	return 0;
}



int
iscsi_connect_async(struct iscsi_context *iscsi, const char *portal,
		    iscsi_command_cb cb, void *private_data)
{
	int port = 3260;
	char *str;
	char *addr, *host;
	struct addrinfo *ai = NULL;
	union socket_address sa;
	int socksize;
	bool portal_is_ip;

	ISCSI_LOG(iscsi, 2, "connecting to portal %s",portal);

	if (iscsi->fd != -1) {
		iscsi_set_error(iscsi,
				"Trying to connect but already connected.");
		return -1;
	}

	addr = iscsi_strdup(iscsi, portal);
	if (addr == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: "
				"Failed to strdup portal address.");
		return -1;
	}
	host = addr;

	/* check if we have a target portal group tag */
	str = strrchr(host, ',');
	if (str != NULL) {
		str[0] = 0;
	}

	str = strrchr(host, ':');
	if (str != NULL) {
		if (strchr(str, ']') == NULL) {
			if (str != NULL) {
				port = atoi(str+1);
				str[0] = 0;
			}
		}
	}

	/* ipv6 in [...] form ? */
	if (host[0] == '[') {
		host ++;
		str = strchr(host, ']');
		if (str == NULL) {
			iscsi_free(iscsi, addr);
			iscsi_set_error(iscsi, "Invalid target:%s  "
				"Missing ']' in IPv6 address", portal);
			return -1;
		}
		*str = 0;
	}

        /* check if we got an ip address or hostname for portal */
	portal_is_ip = inet_pton(AF_INET, host, &sa) == 1 ||
		       inet_pton(AF_INET6, host, &sa) == 1;

	/* is it a hostname ? */
	if (getaddrinfo(host, NULL, NULL, &ai) != 0) {
		iscsi_free(iscsi, addr);
		iscsi_set_error(iscsi, "Invalid target:%s  "
			"Can not resolv into IPv4/v6.", portal);
		return -1;
	}
	iscsi_free(iscsi, addr);

	memset(&sa, 0, sizeof(sa));
	switch (ai->ai_family) {
	case AF_INET:
		socksize = sizeof(struct sockaddr_in);
		memcpy(&sa.sin, ai->ai_addr, socksize);
                sa.sin.sin_family = AF_INET;
		sa.sin.sin_port = htons(port);
#ifdef HAVE_SOCK_SIN_LEN
		sa.sin.sin_len = socksize;
#endif
		if (!portal_is_ip) {
			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &sa.sin.sin_addr, ip, sizeof(ip));
			ISCSI_LOG(iscsi, 2, "portal resolved to ipv4 address %s", ip);
		}
		break;
#ifdef HAVE_SOCKADDR_IN6
	case AF_INET6:
		socksize = sizeof(struct sockaddr_in6);
		memcpy(&sa.sin6, ai->ai_addr, socksize);
                sa.sin6.sin6_family = AF_INET6;
		sa.sin6.sin6_port = htons(port);
#ifdef HAVE_SOCK_SIN_LEN
		sa.sin6.sin6_len = socksize;
#endif
		if (!portal_is_ip) {
			char ip[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &sa.sin6.sin6_addr, ip, sizeof(ip));
			ISCSI_LOG(iscsi, 2, "portal resolved to ipv6 address %s", ip);
		}
		break;
#endif
	default:
		iscsi_set_error(iscsi, "Unknown address family :%d. "
				"Only IPv4/IPv6 supported so far.",
				ai->ai_family);
		freeaddrinfo(ai);
		return -1;

	}

	iscsi->socket_status_cb  = cb;
	iscsi->connect_data      = private_data;

	if (iscsi->drv->connect(iscsi, &sa, ai->ai_family) < 0) {
		iscsi_set_error(iscsi, "Couldn't connect transport: %s",
                                iscsi_get_error(iscsi));
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);
	strncpy(iscsi->connected_portal, portal, MAX_STRING_SIZE);
	return 0;
}

static int
iscsi_tcp_disconnect(struct iscsi_context *iscsi)
{
	if (iscsi->fd == -1) {
		iscsi_set_error(iscsi, "Trying to disconnect "
				"but not connected");
		return -1;
	}

	if (iscsi->old_iscsi && iscsi->old_iscsi->fd == iscsi->fd) {
		/* Reserve this fd because old_iscsi->fd will be reused */
	} else {
		close(iscsi->fd);
	}

	if (!(iscsi->pending_reconnect && iscsi->old_iscsi) &&
	    iscsi->connected_portal[0]) {
		ISCSI_LOG(iscsi, 2, "disconnected from portal %s",iscsi->connected_portal);
	}

	iscsi->fd  = -1;
	iscsi->is_connected = 0;
	iscsi->is_corked = 0;

	return 0;
}


int
iscsi_disconnect(struct iscsi_context *iscsi)
{
	if (!iscsi || !iscsi->drv || !iscsi->drv->disconnect)
		return -1;

        return iscsi->drv->disconnect(iscsi);
}

static int
iscsi_tcp_get_fd(struct iscsi_context *iscsi)
{
	if (iscsi->old_iscsi) {
		return iscsi->old_iscsi->fd;
	}
	return iscsi->fd;
}

int
iscsi_get_fd(struct iscsi_context *iscsi)
{
	return iscsi->drv->get_fd(iscsi);
}

static int
iscsi_tcp_which_events(struct iscsi_context *iscsi)
{
	int events = iscsi->is_connected ? POLLIN : POLLOUT;

	if (iscsi->pending_reconnect && iscsi->old_iscsi &&
		time(NULL) < iscsi->next_reconnect) {
		return 0;
	}

        iscsi_mt_spin_lock(&iscsi->iscsi_lock);
	if (iscsi->outqueue_current ||
	    (iscsi->outqueue && !iscsi->is_corked &&
	     (iscsi_serial32_compare(iscsi->outqueue->cmdsn, iscsi->maxcmdsn) <= 0 ||
	      iscsi->outqueue->outdata.data[0] & ISCSI_PDU_IMMEDIATE)
	    )
	   ) {
		events |= POLLOUT;
	}
        iscsi_mt_spin_unlock(&iscsi->iscsi_lock);
	return events;
}

int
iscsi_which_events(struct iscsi_context *iscsi)
{
	return iscsi->drv->which_events(iscsi);
}

int
iscsi_queue_length(struct iscsi_context *iscsi)
{
	int i = 0;
	struct iscsi_pdu *pdu;

        iscsi_mt_spin_lock(&iscsi->iscsi_lock);
	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		i++;
	}
	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		i++;
	}
	if (iscsi->is_connected == 0) {
		i++;
	}
        iscsi_mt_spin_unlock(&iscsi->iscsi_lock);

	return i;
}

int
iscsi_out_queue_length(struct iscsi_context *iscsi)
{
	int i = 0;
	struct iscsi_pdu *pdu;

        iscsi_mt_spin_lock(&iscsi->iscsi_lock);
	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		i++;
	}
        iscsi_mt_spin_unlock(&iscsi->iscsi_lock);

	return i;
}

ssize_t
iscsi_iovector_readv_writev(struct iscsi_context *iscsi, struct scsi_iovector *iovector, uint32_t pos, ssize_t count, uint32_t *data_digest_ptr, int do_write)
{
	struct scsi_iovec *iov, *iov2;
	int niov;
	uint32_t len2;
	size_t _len2;
	ssize_t n;
	const char *rw = do_write ? "write" : "read";

	if (iovector->iov == NULL) {
		iscsi_set_error(iscsi, "%s: iovector empty buffer", rw);
		errno = EINVAL;
		return -1;
	}

	if (pos < iovector->offset) {
		iscsi_set_error(iscsi, "%s: iovector reset. pos(%d) is smaller than"
				"current offset(%ld)", rw, pos, iovector->offset);
		errno = EINVAL;
		return -1;
	}

	if (iovector->niov <= iovector->consumed) {
		/* someone issued a read/write but did not provide enough user buffers for all the data.
		 * maybe someone tried to read just 512 bytes off a MMC device?
		 */
		iscsi_set_error(iscsi, "%s: iovector consumed(%d) exceeds niov(%d)",
				rw, iovector->consumed, iovector->niov);
		errno = EINVAL;
		return -1;
	}

	/* iov is a pointer to the first iovec to pass */
	iov = &iovector->iov[iovector->consumed];
	pos -= iovector->offset;

	/* forward until iov points to the first iov to pass */
	while (pos >= iov->iov_len) {
		iovector->offset += iov->iov_len;
		iovector->consumed++;
		pos -= iov->iov_len;
		if (iovector->niov <= iovector->consumed) {
			iscsi_set_error(iscsi, "%s: iovector consumed(%d) exceeds niov(%d) on head",
					rw, iovector->consumed, iovector->niov);
			errno = EINVAL;
			return -1;
		}
		iov = &iovector->iov[iovector->consumed];
	}

	iov2 = iov;         /* iov2 is a pointer to the last iovec to pass */
	niov = 1;           /* number of iovectors to pass */
	len2 = pos + count; /* adjust length of iov2 */

	/* forward until iov2 points to the last iovec we pass later. it might
	   happen that we have a lot of iovectors but are limited by count */
	while (len2 > iov2->iov_len) {
		niov++;
		if (iovector->niov < iovector->consumed + niov) {
			iscsi_set_error(iscsi, "%s: iovector consumed(%d) + niov(%d)"
					" exceeds niov(%d) on tail",
					rw, iovector->consumed, niov, iovector->niov);
			errno = EINVAL;
			return -1;
		}
		len2 -= iov2->iov_len;
		iov2 = &iovector->iov[iovector->consumed + niov - 1];
	}

	/* we might limit the length of the last iovec we pass to readv/writev
	   store its orignal length to restore it later */
	_len2 = iov2->iov_len;

	/* adjust base+len of start iovec and len of last iovec */
	iov2->iov_len = len2;
	iov->iov_base = (void*) ((uintptr_t)iov->iov_base + pos);
	iov->iov_len -= pos;

	if (do_write) {
		n = writev(iscsi->fd, (struct iovec*) iov, niov);
	} else {
		n = readv(iscsi->fd, (struct iovec*) iov, niov);
	}

	/* Update the data digest */
	if (data_digest_ptr && n > 0) {
		int i;
		size_t bytes_to_crc = n;
		struct iovec *iov_ptr = (struct iovec*)iov;

		for ( i=0; iov_ptr && i<niov && bytes_to_crc; iov_ptr++, i++) {
			size_t chunk = MIN(bytes_to_crc, iov_ptr->iov_len);
			*data_digest_ptr = crc32c_chain(*data_digest_ptr, iov_ptr->iov_base, chunk);
			bytes_to_crc -= chunk;
		}
	}

	/* restore original values */
	iov->iov_base = (void*) ((uintptr_t)iov->iov_base - pos);
	iov->iov_len += pos;
	iov2->iov_len = _len2;

	if (n > count) {
		/* we read/write more bytes than expected, this MUST not happen */
		errno = EINVAL;
		return -1;
	}
	if ((n <= 0) && (errno != EAGAIN) && (errno == EINTR)) {
		iscsi_set_error(iscsi, "%s: socket failed, errno:%d", rw, errno);
	}
	return n;
}

static int
iscsi_read_from_socket(struct iscsi_context *iscsi)
{
	struct iscsi_in_pdu *in;
	ssize_t hdr_size, data_size, count, padding_size, data_segment_len;
	bool do_data_digest = (iscsi->data_digest != ISCSI_DATA_DIGEST_NONE);
        int ret = -1;
        
	do {
		hdr_size = ISCSI_HEADER_SIZE(iscsi->header_digest);
		if (iscsi->incoming == NULL) {
			iscsi->incoming = iscsi_zmalloc(iscsi, sizeof(struct iscsi_in_pdu));
			if (iscsi->incoming == NULL) {
				iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu");
                                goto finished;
			}
                }
		if (iscsi->incoming->hdr == NULL) {
			crc32c_init(&(iscsi->incoming->calculated_data_digest));
			iscsi->incoming->hdr = iscsi_malloc(iscsi, hdr_size);
			if (iscsi->incoming->hdr == NULL) {
				iscsi_set_error(iscsi, "Out-of-memory");
                                goto finished;
			}
		}
		in = iscsi->incoming;

		/* first we must read the header, including any digests */
		if (in->hdr_pos < hdr_size) {
			/* try to only read the header, the socket is nonblocking, so
			 * no need to limit the read to what is available in the socket
			 */
			count = hdr_size - in->hdr_pos;
			count = recv(iscsi->fd, (void *)&in->hdr[in->hdr_pos],
                                     count, 0);
			if (count == 0) {
				/* remote side has closed the socket. */
                                goto finished;
			}
			if (count < 0) {
				if (errno == EINTR || errno == EAGAIN) {
					break;
				}
				iscsi_set_error(iscsi, "read from socket failed, "
					"errno:%d", errno);
                                goto finished;
			}
			in->hdr_pos  += count;
		}

		if (in->hdr_pos < hdr_size) {
			/* we don't have the full header yet, so return */
			break;
		}

		padding_size = iscsi_get_pdu_padding_size(&in->hdr[0]);
		data_segment_len = iscsi_get_pdu_data_size(&in->hdr[0]);
		data_size = data_segment_len + padding_size;

		if (data_segment_len < 0 || data_segment_len > (ssize_t)iscsi->initiator_max_recv_data_segment_length) {
			iscsi_set_error(iscsi, "Invalid datasegmentlen received from target (%d)", (int)data_segment_len);
                        goto finished;
		}
		if (data_size != 0) {
			unsigned char padding_buf[3];
			unsigned char *buf = padding_buf;
			struct scsi_iovector * iovector_in;

			count = data_size - in->data_pos;

			/* first try to see if we already have a user buffer */
			iovector_in = iscsi_get_scsi_task_iovector_in(iscsi, in);
			if (iovector_in != NULL && count > padding_size) {
				uint32_t offset = scsi_get_uint32(&in->hdr[40]);
				count = iscsi_iovector_readv_writev(iscsi, iovector_in, in->data_pos + offset, count - padding_size, do_data_digest ? &(in->calculated_data_digest) : NULL, 0);
			} else {
				if (iovector_in == NULL) {
					if (in->data == NULL) {
						in->data = iscsi_malloc(iscsi, data_size);
						if (in->data == NULL) {
							iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu->data(%d)", (int)data_size);
                                                        goto finished;
						}
					}
					buf = &in->data[in->data_pos];
				}
				count = recv(iscsi->fd, (void *)buf, count, 0);
				if (do_data_digest && count > 0)
					in->calculated_data_digest = crc32c_chain(in->calculated_data_digest, buf, count);
			}
			if (count == 0) {
				/* remote side has closed the socket. */
                                goto finished;
			}
			if (count < 0) {
				if (errno == EINTR || errno == EAGAIN) {
					break;
				}
                                goto finished;
			}
			in->data_pos += count;
		}

		if (in->data_pos < data_size) {
			break;
		}

		/* Handle Data Digest receive */
		if (data_size != 0 && do_data_digest &&
			in->received_data_digest_bytes < ISCSI_DIGEST_SIZE) {

			count = recv(iscsi->fd, (void *)(in->data_digest_buf + in->received_data_digest_bytes), ISCSI_DIGEST_SIZE - in->received_data_digest_bytes, 0);
			if (count == 0) {
				/* remote side has closed the socket. */
                                goto finished;
			}
			if (count < 0) {
				if (errno == EINTR || errno == EAGAIN) {
					break;
				}
                                goto finished;
			}
			in->received_data_digest_bytes += count;

			if (in->received_data_digest_bytes < ISCSI_DIGEST_SIZE) {
				break;
			}
		}

                iscsi->incoming = NULL;
		if (iscsi_process_pdu(iscsi, in) != 0) {
			iscsi_free_iscsi_in_pdu(iscsi, in);
                        goto finished;
		}
		iscsi_free_iscsi_in_pdu(iscsi, in);
        } while (iscsi->tcp_nonblocking && iscsi->waitpdu && iscsi->is_loggedin); //QQQ break the loop

        ret = 0;
 finished:
	return ret;
}

static int iscsi_pdu_update_headerdigest(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	uint32_t crc;

	if (pdu->outdata.size < ISCSI_RAW_HEADER_SIZE + ISCSI_DIGEST_SIZE) {
		iscsi_set_error(iscsi, "PDU too small (%u) to contain header digest",
				(unsigned int) pdu->outdata.size);
		return -1;
	}

	crc = crc32c(pdu->outdata.data, ISCSI_RAW_HEADER_SIZE);

	pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+3] = (crc >> 24);
	pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+2] = (crc >> 16);
	pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+1] = (crc >>  8);
	pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+0] = (crc);
	return 0;
}

static int
iscsi_write_to_socket(struct iscsi_context *iscsi)
{
	ssize_t count, data_segment_len;
	size_t total;
	struct iscsi_pdu *pdu;
	static char padding_buf[3];
	int socket_flags = 0;
	bool do_data_digest = (iscsi->data_digest != ISCSI_DATA_DIGEST_NONE), execute_data_digest = false;
#ifdef MSG_NOSIGNAL
	socket_flags |= MSG_NOSIGNAL;
#elif SO_NOSIGPIPE
	socket_flags |= SO_NOSIGPIPE;
#endif

	if (iscsi->fd == -1) {
		iscsi_set_error(iscsi, "trying to write but not connected");
		return -1;
	}

	while (iscsi->outqueue || iscsi->outqueue_current) {
		if (iscsi->outqueue_current == NULL) {
			if (iscsi->is_corked) {
				/* connection is corked we are not allowed to send
				 * additional PDUs */
				ISCSI_LOG(iscsi, 6, "iscsi_write_to_socket: socket is corked");
				return 0;
			}
			
			if (iscsi_serial32_compare(iscsi->outqueue->cmdsn, iscsi->maxcmdsn) > 0
				&& !(iscsi->outqueue->outdata.data[0] & ISCSI_PDU_IMMEDIATE)) {
				/* stop sending for non-immediate PDUs. maxcmdsn is reached */
				ISCSI_LOG(iscsi, 6,
				          "iscsi_write_to_socket: maxcmdsn reached (outqueue[0]->cmdsnd %08x > maxcmdsn %08x)",
				          iscsi->outqueue->cmdsn, iscsi->maxcmdsn);
				return 0;
			}

			/* pop first element of the outqueue */
			if (iscsi_serial32_compare(iscsi->outqueue->cmdsn, iscsi->expcmdsn) < 0 &&
				(iscsi->outqueue->outdata.data[0] & 0x3f) != ISCSI_PDU_DATA_OUT) {
				iscsi_set_error(iscsi, "iscsi_write_to_socket: outqueue[0]->cmdsn < expcmdsn (%08x < %08x) opcode %02x",
				                iscsi->outqueue->cmdsn, iscsi->expcmdsn, iscsi->outqueue->outdata.data[0] & 0x3f);
				return -1;
			}
                        
                        iscsi_mt_spin_lock(&iscsi->iscsi_lock);
			iscsi->outqueue_current = iscsi->outqueue;
			
			/* set exp statsn */
			if((iscsi->outqueue->outdata.data[0] & 0x3f) != ISCSI_PDU_DATA_OUT)
				iscsi_pdu_set_expstatsn(iscsi->outqueue_current, iscsi->statsn + 1);
			else
				iscsi_pdu_set_expstatsn(iscsi->outqueue_current, iscsi->statsn);

			/* calculate header checksum */
			if (iscsi->header_digest != ISCSI_HEADER_DIGEST_NONE &&
				iscsi_pdu_update_headerdigest(iscsi, iscsi->outqueue_current) != 0) {
                                iscsi_mt_spin_unlock(&iscsi->iscsi_lock);
				return -1;
			}

			ISCSI_LIST_REMOVE(&iscsi->outqueue, iscsi->outqueue_current);
			if (!(iscsi->outqueue_current->flags & ISCSI_PDU_DELETE_WHEN_SENT)) {
				/* we have to add the pdu to the waitqueue already here
				   since the storage might sent a R2T as soon as it has
				   received the header. if we sent immediate data in a
				   cmd PDU the R2T might get lost otherwise. */
				ISCSI_LIST_ADD_END(&iscsi->waitpdu, iscsi->outqueue_current);
			}
                        iscsi_mt_spin_unlock(&iscsi->iscsi_lock);
		}

		pdu = iscsi->outqueue_current;
		pdu->outdata.size = (pdu->outdata.size + 3) & 0xfffffffc;

		/* Write header and any immediate data */
		if (pdu->outdata_written < pdu->outdata.size) {
			count = send(iscsi->fd,
				     (void *)(pdu->outdata.data +
                                              pdu->outdata_written),
				     pdu->outdata.size - pdu->outdata_written,
				     socket_flags);
			if (count == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return 0;
				}
				iscsi_set_error(iscsi, "Error when writing to "
						"socket :%d", errno);
				return -1;
			}
			pdu->outdata_written += count;
		}
		/* if we havent written the full header yet. */
		if (pdu->outdata_written != pdu->outdata.size) {
			return 0;
		}

		
		if (do_data_digest) {
			data_segment_len = iscsi_get_pdu_data_size(pdu->outdata.data);

			if (data_segment_len && !pdu->payload_len)
				execute_data_digest = true;
			else
				execute_data_digest = false;
		}

		if (execute_data_digest && !pdu->outdata_digest_computed) {
			uint8_t ahslen = pdu->outdata.data[4];
			uint8_t head_len = iscsi->header_digest != ISCSI_HEADER_DIGEST_NONE ? 52 : 48;
			uint32_t offset = head_len + ahslen * 4;
			pdu->calculated_data_digest = crc32c_chain(pdu->calculated_data_digest, pdu->outdata.data + offset, pdu->outdata.size - offset);
			pdu->outdata_digest_computed = true;
		}

		/* Write any iovectors that might have been passed to us */
		while (pdu->payload_written < pdu->payload_len) {
			struct scsi_iovector* iovector_out;

			iovector_out = iscsi_get_scsi_task_iovector_out(iscsi, pdu);

			if (iovector_out == NULL) {
				iscsi_set_error(iscsi, "Can't find iovector data for DATA-OUT");
				return -1;
			}

			count = iscsi_iovector_readv_writev(iscsi,
				iovector_out,
				pdu->payload_offset + pdu->payload_written,
				pdu->payload_len - pdu->payload_written, do_data_digest ? &(pdu->calculated_data_digest) : NULL, 1);
			if (count == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return 0;
				}
				return -1;
			}

			pdu->payload_written += count;
		}

		total = pdu->payload_len;
		total = (total + 3) & 0xfffffffc;

		/* Write padding */
		if (pdu->payload_written < total) {
			count = send(iscsi->fd, padding_buf, total - pdu->payload_written, socket_flags);
			if (count == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					return 0;
				}
				iscsi_set_error(iscsi, "Error when writing to "
						"socket :%d", errno);
				return -1;
			}

			if (do_data_digest)
				pdu->calculated_data_digest = crc32c_chain(pdu->calculated_data_digest, (uint8_t *)padding_buf, count);

			pdu->payload_written += count;
		}

		/* if we havent written the full padding yet. */
		if (pdu->payload_written < total) {
			return 0;
		}

		/*
		 * Maybe update the total again, and write the digest, but only if
		 * 1. DataDigest has been negociated, and
		 * 2. We have actually written some data
		 */
		if (execute_data_digest || (do_data_digest && pdu->payload_written)) {
			uint32_t data_digest = crc32c_chain_done(pdu->calculated_data_digest);
			char data_digest_buf[ISCSI_DIGEST_SIZE];

			total += ISCSI_DIGEST_SIZE;

			data_digest_buf[3] = (data_digest >> 24);
			data_digest_buf[2] = (data_digest >> 16);
			data_digest_buf[1] = (data_digest >>  8);
			data_digest_buf[0] = (data_digest);

			/* Write data digest */
			if (pdu->payload_written < total) {
				int todo = total - pdu->payload_written;
				count = send(iscsi->fd, data_digest_buf + (ISCSI_DIGEST_SIZE - todo), todo, socket_flags);
				if (count == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						return 0;
					}
					iscsi_set_error(iscsi, "Error when writing to "
							"socket :%d", errno);
					return -1;
				}

				pdu->payload_written += count;
			}
		}

		/* if we havent written everything yet. */
		if (pdu->payload_written != total) {
			return 0;
		}

		if (pdu->flags & ISCSI_PDU_CORK_WHEN_SENT) {
			iscsi->is_corked = 1;
		}
		if (pdu->flags & ISCSI_PDU_DELETE_WHEN_SENT) {
			iscsi->drv->free_pdu(iscsi, pdu);
		}
		iscsi->outqueue_current = NULL;
	}
	return 0;
}

int
iscsi_service_reconnect_if_loggedin(struct iscsi_context *iscsi)
{
	if (iscsi->is_loggedin) {
		if (iscsi_reconnect(iscsi) == 0) {
			return 0;
		}
	}
	if (iscsi->old_iscsi) {
		if (!iscsi->pending_reconnect) {
			iscsi_reconnect_cb(iscsi, SCSI_STATUS_ERROR, NULL, NULL);
		}
		return 0;
	}
	iscsi_set_error(iscsi, "iscsi_service_reconnect_if_loggedin. Can not "
			"reconnect right now.\n");
	return -1;
}

static int
iscsi_tcp_service(struct iscsi_context *iscsi, int revents)
{
	if (iscsi->fd < 0) {
		return 0;
	}

	if (iscsi->pending_reconnect) {
		if (time(NULL) >= iscsi->next_reconnect) {
			return iscsi_reconnect(iscsi);
		} else {
			if (iscsi->old_iscsi) {
				goto check_timeout;
			}
		}
	}

	if (revents & POLLERR) {
		int err = 0;
		socklen_t err_size = sizeof(err);

		if (getsockopt(iscsi->fd, SOL_SOCKET, SO_ERROR,
			       (char *)&err, &err_size) != 0 || err != 0) {
			if (err == 0) {
				err = errno;
			}
			iscsi_set_error(iscsi, "iscsi_service: socket error "
					"%s(%d).",
					strerror(err), err);
		} else {
			iscsi_set_error(iscsi, "iscsi_service: POLLERR, "
					"Unknown socket error.");
		}
		if (iscsi->socket_status_cb) {
			iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR, NULL,
						iscsi->connect_data);
			iscsi->socket_status_cb = NULL;
		}
		return iscsi_service_reconnect_if_loggedin(iscsi);
	}
	if (revents & POLLHUP) {
		iscsi_set_error(iscsi, "iscsi_service: POLLHUP, "
				"socket error.");
		if (iscsi->socket_status_cb) {
			iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR, NULL,
						iscsi->connect_data);
			iscsi->socket_status_cb = NULL;
		}
		return iscsi_service_reconnect_if_loggedin(iscsi);
	}

	if (iscsi->is_connected == 0 && revents&POLLOUT) {
		int err = 0;
		socklen_t err_size = sizeof(err);
		struct sockaddr_in local;
		socklen_t local_l = sizeof(local);

		if (getsockopt(iscsi->fd, SOL_SOCKET, SO_ERROR,
			       (char *)&err, &err_size) != 0 || err != 0) {
			if (err == 0) {
				err = errno;
			}
			iscsi_set_error(iscsi, "iscsi_service: socket error "
					"%s(%d) while connecting.",
					strerror(err), err);
			if (iscsi->socket_status_cb) {
				iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR,
							NULL, iscsi->connect_data);
				iscsi->socket_status_cb = NULL;
			}

			return iscsi_service_reconnect_if_loggedin(iscsi);
		}

		if (getsockname(iscsi->fd, (struct sockaddr *) &local, &local_l) == 0) {
			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip));
			ISCSI_LOG(iscsi, 2, "connection established (%s:%u -> %s)", ip,
			          (unsigned)ntohs(local.sin_port), iscsi->connected_portal);
		}

		iscsi->is_connected = 1;
		if (iscsi->socket_status_cb) {
			iscsi->socket_status_cb(iscsi, SCSI_STATUS_GOOD, NULL,
						iscsi->connect_data);
			iscsi->socket_status_cb = NULL;
		}
		return 0;
	}

	if (revents & POLLIN) {
		if (iscsi_read_from_socket(iscsi) != 0) {
			ISCSI_LOG(iscsi, 1, "%s", iscsi_get_error(iscsi));
			return iscsi_service_reconnect_if_loggedin(iscsi);
		}
	}
        
	if (revents & POLLOUT) {
		if (iscsi_write_to_socket(iscsi) != 0) {
			ISCSI_LOG(iscsi, 1, "%s", iscsi_get_error(iscsi));
			return iscsi_service_reconnect_if_loggedin(iscsi);
		}
	}

check_timeout:
	iscsi_timeout_scan(iscsi);

	if (iscsi->old_iscsi) {
		iscsi_timeout_scan(iscsi->old_iscsi);
	}

	return 0;
}

int
iscsi_service(struct iscsi_context *iscsi, int revents)
{
	return iscsi->drv->service(iscsi, revents);
}

static void iscsi_tcp_queue_pdu(struct iscsi_context *iscsi,
                               struct iscsi_pdu *pdu)
{
	iscsi_add_to_outqueue(iscsi, pdu);
}

void
iscsi_free_iscsi_in_pdu(struct iscsi_context *iscsi, struct iscsi_in_pdu *in)
{
	iscsi_free(iscsi, in->hdr);
	iscsi_free(iscsi, in->data);
	in->data=NULL;
	iscsi_free(iscsi, in);
	in=NULL;
}

void iscsi_set_tcp_syncnt(struct iscsi_context *iscsi, int value)
{
	iscsi->tcp_syncnt=value;
	ISCSI_LOG(iscsi, 2, "TCP_SYNCNT will be set to %d on next socket creation",value);
}

void iscsi_set_tcp_user_timeout(struct iscsi_context *iscsi, int value)
{
	iscsi->tcp_user_timeout=value;
	ISCSI_LOG(iscsi, 2, "TCP_USER_TIMEOUT will be set to %dms on next socket creation",value);
}

void iscsi_set_tcp_keepidle(struct iscsi_context *iscsi, int value)
{
	iscsi->tcp_keepidle=value;
	ISCSI_LOG(iscsi, 2, "TCP_KEEPIDLE will be set to %d on next socket creation",value);
}

void iscsi_set_tcp_keepcnt(struct iscsi_context *iscsi, int value)
{
	iscsi->tcp_keepcnt=value;
	ISCSI_LOG(iscsi, 2, "TCP_KEEPCNT will be set to %d on next socket creation",value);
}

void iscsi_set_tcp_keepintvl(struct iscsi_context *iscsi, int value)
{
	iscsi->tcp_keepintvl=value;
	ISCSI_LOG(iscsi, 2, "TCP_KEEPINTVL will be set to %d on next socket creation",value);
}

int iscsi_set_tcp_keepalive(struct iscsi_context *iscsi, int idle, int count, int interval)
{
#ifdef SO_KEEPALIVE
	int value = 1;
	if (setsockopt(iscsi->fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&value, sizeof(value)) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set socket option SO_KEEPALIVE. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "SO_KEEPALIVE set to %d",value);
#ifdef TCP_KEEPCNT
	if (set_tcp_sockopt(iscsi->fd, TCP_KEEPCNT, count) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp keepalive count. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "TCP_KEEPCNT set to %d",count);
#endif
#ifdef TCP_KEEPINTVL
	if (set_tcp_sockopt(iscsi->fd, TCP_KEEPINTVL, interval) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp keepalive interval. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "TCP_KEEPINTVL set to %d",interval);
#endif
#ifdef TCP_KEEPIDLE
	if (set_tcp_sockopt(iscsi->fd, TCP_KEEPIDLE, idle) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp keepalive idle. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "TCP_KEEPIDLE set to %d",idle);
#endif
#endif

	return 0;
}

void iscsi_set_bind_interfaces(struct iscsi_context *iscsi, char * interfaces)
{
#if __linux
	strncpy(iscsi->bind_interfaces,interfaces,MAX_STRING_SIZE);
	iscsi->bind_interfaces_cnt=0;
	char * pchr = interfaces;
	char * pchr2 = NULL;
	do {
		pchr2 = strchr(pchr,',');
		if (pchr2) {pchr=pchr2+1;}
		iscsi->bind_interfaces_cnt++;
	} while (pchr2);
	ISCSI_LOG(iscsi,2,"will bind to one of the following %d interface(s) on next socket creation: %s",iscsi->bind_interfaces_cnt,interfaces);
	if (!iface_rr) iface_rr=rand()%iscsi->bind_interfaces_cnt+1;
#else
	ISCSI_LOG(iscsi,1,"binding to an interface is not supported on your OS");
#endif
}

#if defined(_MSC_VER) && _MSC_VER < 1900
static iscsi_transport iscsi_transport_tcp = {
	iscsi_tcp_connect,
	iscsi_tcp_queue_pdu,
	iscsi_tcp_new_pdu,
	iscsi_tcp_disconnect,
	iscsi_tcp_free_pdu,
	iscsi_tcp_service,
	iscsi_tcp_get_fd,
	iscsi_tcp_which_events,
};
#else
static iscsi_transport iscsi_transport_tcp = {
	.connect      = iscsi_tcp_connect,
	.queue_pdu    = iscsi_tcp_queue_pdu,
	.new_pdu      = iscsi_tcp_new_pdu,
	.disconnect   = iscsi_tcp_disconnect,
	.free_pdu     = iscsi_tcp_free_pdu,
	.service      = iscsi_tcp_service,
	.get_fd       = iscsi_tcp_get_fd,
	.which_events = iscsi_tcp_which_events,
};
#endif

void iscsi_init_tcp_transport(struct iscsi_context *iscsi)
{
	iscsi->drv = &iscsi_transport_tcp;
	iscsi->transport = TCP_TRANSPORT;
}
