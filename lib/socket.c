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

#if defined(WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define ioctl ioctlsocket
#define close closesocket
#else
#include "config.h"
#include <strings.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/ioctl.h>
#endif

#ifdef NEED_SYS_FILIO_H
#include <sys/filio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "slist.h"

static uint32_t iface_rr = 0;

void iscsi_decrement_iface_rr() {
	iface_rr--;
}

static void set_nonblocking(int fd)
{
#if defined(WIN32)
	unsigned long opt = 1;
	ioctlsocket(fd, FIONBIO, &opt);
#else
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, v | O_NONBLOCK);
#endif
}

int set_tcp_sockopt(int sockfd, int optname, int value)
{
	int level;

	#if defined(__FreeBSD__) || defined(__sun)
	struct protoent *buf;

	if ((buf = getprotobyname("tcp")) != NULL)
		level = buf->p_proto;
	else
		return -1;
	#else
		level = SOL_TCP;
	#endif

	return setsockopt(sockfd, level, optname, &value, sizeof(value));
}

#ifndef TCP_USER_TIMEOUT
#define TCP_USER_TIMEOUT        18
#endif

int set_tcp_user_timeout(struct iscsi_context *iscsi)
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

int set_tcp_syncnt(struct iscsi_context *iscsi)
{
	if (set_tcp_sockopt(iscsi->fd, TCP_SYNCNT, iscsi->tcp_syncnt) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp syn retries. Error %s(%d)", strerror(errno), errno);
		return -1;
	}
	ISCSI_LOG(iscsi, 3, "TCP_SYNCNT set to %d",iscsi->tcp_syncnt);
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
	int socksize;

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

	/* is it a hostname ? */
	if (getaddrinfo(host, NULL, NULL, &ai) != 0) {
		iscsi_free(iscsi, addr);
		iscsi_set_error(iscsi, "Invalid target:%s  "
			"Can not resolv into IPv4/v6.", portal);
		return -1;
 	}
	iscsi_free(iscsi, addr);

	switch (ai->ai_family) {
	case AF_INET:
		socksize = sizeof(struct sockaddr_in);
		((struct sockaddr_in *)(ai->ai_addr))->sin_port = htons(port);
#ifdef HAVE_SOCK_SIN_LEN
		((struct sockaddr_in *)(ai->ai_addr))->sin_len = socksize;
#endif
		break;
	case AF_INET6:
		socksize = sizeof(struct sockaddr_in6);
		((struct sockaddr_in6 *)(ai->ai_addr))->sin6_port = htons(port);
#ifdef HAVE_SOCK_SIN_LEN
		((struct sockaddr_in6 *)(ai->ai_addr))->sin6_len = socksize;
#endif
		break;
	default:
		iscsi_set_error(iscsi, "Unknown address family :%d. "
				"Only IPv4/IPv6 supported so far.",
				ai->ai_family);
		freeaddrinfo(ai);
		return -1;

	}

	iscsi->fd = socket(ai->ai_family, SOCK_STREAM, 0);
	if (iscsi->fd == -1) {
		freeaddrinfo(ai);
		iscsi_set_error(iscsi, "Failed to open iscsi socket. "
				"Errno:%s(%d).", strerror(errno), errno);
		return -1;

	}

	iscsi->socket_status_cb  = cb;
	iscsi->connect_data      = private_data;

	set_nonblocking(iscsi->fd);

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

	if (connect(iscsi->fd, ai->ai_addr, socksize) != 0
	    && errno != EINPROGRESS) {
		iscsi_set_error(iscsi, "Connect failed with errno : "
				"%s(%d)", strerror(errno), errno);
		close(iscsi->fd);
		iscsi->fd = -1;
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);

	strncpy(iscsi->connected_portal,portal,MAX_STRING_SIZE);

	return 0;
}

int
iscsi_disconnect(struct iscsi_context *iscsi)
{
	if (iscsi->fd == -1) {
		iscsi_set_error(iscsi, "Trying to disconnect "
				"but not connected");
		return -1;
	}

	close(iscsi->fd);

	if (iscsi->connected_portal[0]) {
		ISCSI_LOG(iscsi, 2, "disconnected from portal %s",iscsi->connected_portal);
	}

	iscsi->fd  = -1;
	iscsi->is_connected = 0;

	return 0;
}

int
iscsi_get_fd(struct iscsi_context *iscsi)
{
	return iscsi->fd;
}

int
iscsi_which_events(struct iscsi_context *iscsi)
{
	int events = iscsi->is_connected ? POLLIN : POLLOUT;

	if (iscsi->outqueue && iscsi->outqueue->cmdsn <= iscsi->maxcmdsn) {
	 	events |= POLLOUT;
	}
	return events;
}

int
iscsi_queue_length(struct iscsi_context *iscsi)
{
	int i = 0;
	struct iscsi_pdu *pdu;

	for (pdu = iscsi->outqueue; pdu; pdu = pdu->next) {
		i++;
	}
	for (pdu = iscsi->waitpdu; pdu; pdu = pdu->next) {
		i++;
	}
	if (iscsi->is_connected == 0) {
		i++;
	}

	return i;
}

static int
iscsi_read_from_socket(struct iscsi_context *iscsi)
{
	struct iscsi_in_pdu *in;
	ssize_t data_size, count;

	if (iscsi->incoming == NULL) {
		iscsi->incoming = iscsi_zmalloc(iscsi, sizeof(struct iscsi_in_pdu));
		if (iscsi->incoming == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu");
			return -1;
		}
	}
	in = iscsi->incoming;

	/* first we must read the header, including any digests */
	if (in->hdr_pos < ISCSI_HEADER_SIZE) {
		/* try to only read the header, the socket is nonblocking, so
		 * no need to limit the read to what is available in the socket
		 */
		count = ISCSI_HEADER_SIZE - in->hdr_pos;
		count = recv(iscsi->fd, &in->hdr[in->hdr_pos], count, 0);
		if (count == 0) {
			return -1;
		}
		if (count < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				return 0;
			}
			iscsi_set_error(iscsi, "read from socket failed, "
				"errno:%d", errno);
			return -1;
		}
		in->hdr_pos  += count;
	}

	if (in->hdr_pos < ISCSI_HEADER_SIZE) {
		/* we dont have the full header yet, so return */
		return 0;
	}

	data_size = iscsi_get_pdu_data_size(&in->hdr[0]);
	if (data_size < 0 || data_size > iscsi->initiator_max_recv_data_segment_length) {
		iscsi_set_error(iscsi, "Invalid data size received from target (%d)", (int)data_size);
		return -1;
	}
	if (data_size != 0) {
		unsigned char *buf = NULL;

		count = data_size - in->data_pos;

		/* first try to see if we already have a user buffer */
		buf = iscsi_get_user_in_buffer(iscsi, in, in->data_pos, &count);
		/* if not, allocate one */
		if (buf == NULL) {
			if (in->data == NULL) {
				in->data = iscsi_malloc(iscsi, data_size);
				if (in->data == NULL) {
					iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu->data(%d)", (int)data_size);
					return -1;
				}
			}
			buf = &in->data[in->data_pos];
		}

		count = recv(iscsi->fd, buf, count, 0);
		if (count == 0) {
			return -1;
		}
		if (count < 0) {
			if (errno == EINTR || errno == EAGAIN) {
				return 0;
			}
			iscsi_set_error(iscsi, "read from socket failed, "
				"errno:%d", errno);
			return -1;
		}
		in->data_pos += count;
	}

	if (in->data_pos < data_size) {
		return 0;
	}

	SLIST_ADD_END(&iscsi->inqueue, in);
	iscsi->incoming = NULL;


	while (iscsi->inqueue != NULL) {
		struct iscsi_in_pdu *current = iscsi->inqueue;

		if (iscsi_process_pdu(iscsi, current) != 0) {
			return -1;
		}
		SLIST_REMOVE(&iscsi->inqueue, current);
		iscsi_free_iscsi_in_pdu(iscsi, current);
	}


	return 0;
}

static int
iscsi_write_to_socket(struct iscsi_context *iscsi)
{
	ssize_t count;

	if (iscsi->fd == -1) {
		iscsi_set_error(iscsi, "trying to write but not connected");
		return -1;
	}

	while (iscsi->outqueue) {
		ssize_t total;

		if (iscsi->outqueue->cmdsn > iscsi->maxcmdsn) {
			/* stop sending. maxcmdsn is reached */
			return 0;
		}

		total = iscsi->outqueue->outdata.size;
		total = (total + 3) & 0xfffffffc;

		count = send(iscsi->fd,
			      iscsi->outqueue->outdata.data
			      + iscsi->outqueue->written,
			      total - iscsi->outqueue->written,
			      0);
		if (count == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}
			iscsi_set_error(iscsi, "Error when writing to "
					"socket :%d", errno);
			return -1;
		}

		iscsi->outqueue->written += count;
		if (iscsi->outqueue->written == total) {
			struct iscsi_pdu *pdu = iscsi->outqueue;

			SLIST_REMOVE(&iscsi->outqueue, pdu);
			if (pdu->flags & ISCSI_PDU_DELETE_WHEN_SENT) {
				iscsi_free_pdu(iscsi, pdu);
			} else {
				SLIST_ADD_END(&iscsi->waitpdu, pdu);
			}
		}
	}
	return 0;
}

static inline int
iscsi_service_reconnect_if_loggedin(struct iscsi_context *iscsi)
{
	if (iscsi->is_loggedin) {
		if (iscsi_reconnect(iscsi) == 0) {
			return 0;
		}
	}
	return -1;
}

int
iscsi_service(struct iscsi_context *iscsi, int revents)
{
	if (revents & POLLERR) {
		int err = 0;
		socklen_t err_size = sizeof(err);

		if (getsockopt(iscsi->fd, SOL_SOCKET, SO_ERROR,
			       &err, &err_size) != 0 || err != 0) {
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

	if (iscsi->is_connected == 0 && iscsi->fd != -1 && revents&POLLOUT) {
		int err = 0;
		socklen_t err_size = sizeof(err);
		if (getsockopt(iscsi->fd, SOL_SOCKET, SO_ERROR,
			       &err, &err_size) != 0 || err != 0) {
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

		ISCSI_LOG(iscsi, 2, "connection to %s established",iscsi->connected_portal);

		iscsi->is_connected = 1;
		if (iscsi->socket_status_cb) {
			iscsi->socket_status_cb(iscsi, SCSI_STATUS_GOOD, NULL,
						iscsi->connect_data);
			iscsi->socket_status_cb = NULL;
		}
		return 0;
	}

	if (iscsi->is_connected && revents & POLLOUT && iscsi->outqueue != NULL) {
		if (iscsi_write_to_socket(iscsi) != 0) {
			return iscsi_service_reconnect_if_loggedin(iscsi);
		}
	}
	if (iscsi->is_connected && revents & POLLIN) {
		if (iscsi_read_from_socket(iscsi) != 0) {
			return iscsi_service_reconnect_if_loggedin(iscsi);
		}
	}

	return 0;
}

int
iscsi_queue_pdu(struct iscsi_context *iscsi, struct iscsi_pdu *pdu)
{
	if (pdu == NULL) {
		iscsi_set_error(iscsi, "trying to queue NULL pdu");
		return -1;
	}

	if (iscsi->header_digest != ISCSI_HEADER_DIGEST_NONE) {
		unsigned long crc;

		if (pdu->outdata.size < ISCSI_RAW_HEADER_SIZE + 4) {
			iscsi_set_error(iscsi, "PDU too small (%d) to contain header digest",
					pdu->outdata.size);
			return -1;
		}

		crc = crc32c((char *)pdu->outdata.data, ISCSI_RAW_HEADER_SIZE);

		pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+3] = (crc >> 24)&0xff;
		pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+2] = (crc >> 16)&0xff;
		pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+1] = (crc >>  8)&0xff;
		pdu->outdata.data[ISCSI_RAW_HEADER_SIZE+0] = (crc)      &0xff;
	}

	SLIST_ADD_END(&iscsi->outqueue, pdu);

	return 0;
}

void
iscsi_free_iscsi_in_pdu(struct iscsi_context *iscsi, struct iscsi_in_pdu *in)
{
	iscsi_free(iscsi, in->data);
	in->data=NULL;
	iscsi_free(iscsi, in);
	in=NULL;
}

void
iscsi_free_iscsi_inqueue(struct iscsi_context *iscsi, struct iscsi_in_pdu *inqueue)
{
	while (inqueue != NULL) {
	      struct iscsi_in_pdu *next = inqueue->next;
	      iscsi_free_iscsi_in_pdu(iscsi, inqueue);
	      inqueue = next;
	}
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
	if (setsockopt(iscsi->fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) != 0) {
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
