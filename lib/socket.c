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
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "iscsi.h"
#include "iscsi-private.h"
#include "slist.h"

static void set_nonblocking(int fd)
{
	unsigned v;
	v = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, v | O_NONBLOCK);
}

int
iscsi_connect_async(struct iscsi_context *iscsi, const char *portal,
		    iscsi_command_cb cb, void *private_data)
{
	int tpgt = -1;
	int port = 3260;
	char *str;
	char *addr;
	struct sockaddr_storage s;
	struct sockaddr_in *sin = (struct sockaddr_in *)&s;
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

	/* check if we have a target portal group tag */
	str = rindex(addr, ',');
	if (str != NULL) {
		tpgt = atoi(str+1);
		str[0] = 0;
	}

	/* XXX need handling for {ipv6 addresses} */
	/* for now, assume all is ipv4 */
	str = rindex(addr, ':');
	if (str != NULL) {
		port = atoi(str+1);
		str[0] = 0;
	}

	sin->sin_family = AF_INET;
	sin->sin_port   = htons(port);
	if (inet_pton(AF_INET, addr, &sin->sin_addr) != 1) {
		struct hostent *he;

		he = gethostbyname(addr);
		if (he == NULL) {
			iscsi_set_error(iscsi, "Invalid target:%s  "
					"Failed to resolve hostname.", addr);
			free(addr);
			return -1;
		}
		if (he->h_addrtype != AF_INET) {
			iscsi_set_error(iscsi, "Invalid target:%s  "
					"Can not resolve into IPv4.", addr);
			free(addr);
			return -1;
		}
		sin->sin_addr.s_addr = *(uint32_t *)he->h_addr_list[0];
	}
	free(addr);

	switch (s.ss_family) {
	case AF_INET:
		iscsi->fd = socket(AF_INET, SOCK_STREAM, 0);
		socksize = sizeof(struct sockaddr_in);
		break;
	default:
		iscsi_set_error(iscsi, "Unknown address family :%d. "
				"Only IPv4 supported so far.", s.ss_family);
		return -1;

	}
#ifdef HAVE_SOCK_SIN_LEN
	s.ss_len = socksize;
#endif

	if (iscsi->fd == -1) {
		iscsi_set_error(iscsi, "Failed to open iscsi socket. "
				"Errno:%s(%d).", strerror(errno), errno);
		return -1;

	}

	iscsi->socket_status_cb  = cb;
	iscsi->connect_data      = private_data;

	set_nonblocking(iscsi->fd);

	if (connect(iscsi->fd, (struct sockaddr *)&s, socksize) != 0
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

static int
iscsi_read_from_socket(struct iscsi_context *iscsi)
{
	struct iscsi_in_pdu *in;
	ssize_t data_size, count;

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
		count = read(iscsi->fd, &in->hdr[in->hdr_pos], ISCSI_HEADER_SIZE - in->hdr_pos);
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
		in->hdr_pos += count;
	}

	if (in->hdr_pos < ISCSI_HEADER_SIZE) {
		/* we dont have the full header yet, so return */
		return 0;
	}

	data_size = iscsi_get_pdu_data_size(&in->hdr[0]);
	if (data_size != 0) {
		if (in->data == NULL) {
			in->data = malloc(data_size);
			if (in->data == NULL) {
				iscsi_set_error(iscsi, "Out-of-memory: failed to malloc iscsi_in_pdu->data(%d)", (int)data_size);
				return -1;
			}
		}

		count = read(iscsi->fd, &in->data[in->data_pos], data_size - in->data_pos);
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

		count = write(iscsi->fd,
			      iscsi->outqueue->outdata.data
			      + iscsi->outqueue->written,
			      total - iscsi->outqueue->written);
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
