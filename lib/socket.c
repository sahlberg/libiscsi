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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "slist.h"

static void set_nonblocking(int fd)
{
#if defined(WIN32)
#else
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, v | O_NONBLOCK);
#endif
}

int
iscsi_connect_async(struct iscsi_context *iscsi, const char *portal,
		    iscsi_command_cb cb, void *private_data)
{
	int tpgt = -1;
	int port = 3260;
	char *str;
	char *addr, *host;
	struct addrinfo *ai = NULL;
	int socksize;

	if (iscsi->fd != -1) {
		iscsi_set_error(iscsi,
				"Trying to connect but already connected.");
		return -1;
	}

	addr = strdup(portal);
	if (addr == NULL) {
		iscsi_set_error(iscsi, "Out-of-memory: "
				"Failed to strdup portal address.");
		return -1;
	}
	host = addr;

	/* check if we have a target portal group tag */
	str = strrchr(host, ',');
	if (str != NULL) {
		tpgt = atoi(str+1);
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
			free(addr);
			iscsi_set_error(iscsi, "Invalid target:%s  "
				"Missing ']' in IPv6 address", portal);
			return -1;
		}
		*str = 0;		
	}

	/* is it a hostname ? */
	if (getaddrinfo(host, NULL, NULL, &ai) != 0) {
		free(addr);
		iscsi_set_error(iscsi, "Invalid target:%s  "
			"Can not resolv into IPv4/v6.", portal);
		return -1;
 	}
	free(addr);

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

	if (iscsi->outqueue) {
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
	int socket_count = 0;

	if (ioctl(iscsi->fd, FIONREAD, &socket_count) != 0) {
		iscsi_set_error(iscsi, "Socket failure. Socket FIONREAD failed");
		return -1;
	}
	if (socket_count == 0) {
		iscsi_set_error(iscsi, "Socket failure. Socket is readable but no bytes available in FIONREAD");
		return -1;
	}

	if (iscsi->incoming == NULL) {
		iscsi->incoming = malloc(sizeof(struct iscsi_in_pdu));
		if (iscsi->incoming == NULL) {
			iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu");
			return -1;
		}
		memset(iscsi->incoming, 0, sizeof(struct iscsi_in_pdu));
	}
	in = iscsi->incoming;

	/* first we must read the header, including any digests */
	if (in->hdr_pos < ISCSI_HEADER_SIZE) {
		/* try to only read the header, and make sure we don't
		 * read more than is available in the socket;
		 */
		count = ISCSI_HEADER_SIZE - in->hdr_pos;
		if (socket_count < count) {
			count = socket_count;
		}
		count = recv(iscsi->fd, &in->hdr[in->hdr_pos], count, 0);
		if (count < 0) {
			if (errno == EINTR) {
				return 0;
			}
			iscsi_set_error(iscsi, "read from socket failed, "
				"errno:%d", errno);
			return -1;
		}
		if (count == 0) {
			return 0;
		}
		in->hdr_pos  += count;
		socket_count -= count;
	}

	if (in->hdr_pos < ISCSI_HEADER_SIZE) {
		/* we dont have the full header yet, so return */
		return 0;
	}

	data_size = iscsi_get_pdu_data_size(&in->hdr[0]);
	if (data_size != 0) {
		unsigned char *buf = NULL;

		/* No more data right now */
		if (socket_count == 0) {
			return 0;
		}
		count = data_size - in->data_pos;
		if (count > socket_count) {
			count = socket_count;
		}

		/* first try to see if we already have a user buffer */
		buf = iscsi_get_user_in_buffer(iscsi, in, in->data_pos, &count);
		/* if not, allocate one */
		if (buf == NULL) {
			if (in->data == NULL) {
				in->data = malloc(data_size);
				if (in->data == NULL) {
					iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu->data(%d)", (int)data_size);
					return -1;
				}
			}
			buf = &in->data[in->data_pos];
		}

		count = recv(iscsi->fd, buf, count, 0);
		if (count < 0) {
			if (errno == EINTR) {
				return 0;
			}
			iscsi_set_error(iscsi, "read from socket failed, "
				"errno:%d", errno);
			return -1;
		}
		if (count == 0) {
			return 0;
		}
		in->data_pos += count;
		socket_count -= count;
	}

	if (in->data_pos < data_size) {
		return 0;
	}

	SLIST_ADD_END(&iscsi->inqueue, in);
	iscsi->incoming = NULL;


	while (iscsi->inqueue != NULL) {
		struct iscsi_in_pdu *in = iscsi->inqueue;

		if (iscsi_process_pdu(iscsi, in) != 0) {
			return -1;
		}
		SLIST_REMOVE(&iscsi->inqueue, in);
		iscsi_free_iscsi_in_pdu(in);
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

	while (iscsi->outqueue != NULL) {
		ssize_t total;

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
		iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR, NULL,
					iscsi->connect_data);
		return -1;
	}
	if (revents & POLLHUP) {
		iscsi_set_error(iscsi, "iscsi_service: POLLHUP, "
				"socket error.");
		iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR, NULL,
					iscsi->connect_data);
		return -1;
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
			iscsi->socket_status_cb(iscsi, SCSI_STATUS_ERROR,
						NULL, iscsi->connect_data);
			return -1;
		}

#ifdef HAVE_TCP_KEEPALIVE
		iscsi_set_tcp_keepalive(iscsi, 30, 3, 30);
#endif
		iscsi->is_connected = 1;
		iscsi->socket_status_cb(iscsi, SCSI_STATUS_GOOD, NULL,
					iscsi->connect_data);
		return 0;
	}

	if (revents & POLLOUT && iscsi->outqueue != NULL) {
		if (iscsi_write_to_socket(iscsi) != 0) {
			return -1;
		}
	}
	if (revents & POLLIN) {
		if (iscsi_read_from_socket(iscsi) != 0)
			return -1;
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
iscsi_free_iscsi_in_pdu(struct iscsi_in_pdu *in)
{
	free(in->data);
	free(in);
}

void
iscsi_free_iscsi_inqueue(struct iscsi_in_pdu *inqueue)
{
	while (inqueue != NULL) {
	      struct iscsi_in_pdu *next = inqueue->next;
	      iscsi_free_iscsi_in_pdu(inqueue);
	      inqueue = next;
	}
}

#ifdef HAVE_TCP_KEEPALIVE
int iscsi_set_tcp_keepalive(struct iscsi_context *iscsi, int idle, int count, int interval)
{
	int value;

	value =1;
	if (setsockopt(iscsi->fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value)) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set socket option SO_KEEPALIVE. Error %s(%d)", strerror(errno), errno);
		return -1;
	}

	value = count;
	if (setsockopt(iscsi->fd, SOL_TCP, TCP_KEEPCNT, &value, sizeof(value)) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp keepalive count. Error %s(%d)", strerror(errno), errno);
		return -1;
	}

	value = interval;
	if (setsockopt(iscsi->fd, SOL_TCP, TCP_KEEPINTVL, &value, sizeof(value)) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp keepalive interval. Error %s(%d)", strerror(errno), errno);
		return -1;
	}

	value = idle;
	if (setsockopt(iscsi->fd, SOL_TCP, TCP_KEEPIDLE, &value, sizeof(value)) != 0) {
		iscsi_set_error(iscsi, "TCP: Failed to set tcp keepalive idle. Error %s(%d)", strerror(errno), errno);
		return -1;
	}

	return 0;
}

#endif
#if defined(WIN32)
int poll(struct pollfd *fds, int nfsd, int timeout)
{
	fd_set rfds, wfds, efds;
	int ret;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	if (fds->events & POLLIN) {
		FD_SET(fds->fd, &rfds);
	}
	if (fds->events & POLLOUT) {
		FD_SET(fds->fd, &wfds);
	}
	FD_SET(fds->fd, &efds);
	select(fds->fd + 1, &rfds, &wfds, &efds, NULL);
	fds->revents = 0;
	if (FD_ISSET(fds->fd, &rfds)) {
		fds->revents |= POLLIN;
	}
	if (FD_ISSET(fds->fd, &wfds)) {
		fds->revents |= POLLOUT;
	}
	if (FD_ISSET(fds->fd, &efds)) {
		fds->revents |= POLLHUP;
	}
	return 1;
}
#endif

