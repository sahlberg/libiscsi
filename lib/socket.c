/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

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
		iscsi_set_error(iscsi, "Invalid target:%s  "
				"Failed to convert to ip address.", addr);
		free(addr);
		return -1;
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
	int events = POLLIN;

	if (iscsi->is_connected == 0) {
		events |= POLLOUT;
	}

	if (iscsi->outqueue) {
		events |= POLLOUT;
	}
	return events;
}

static int
iscsi_read_from_socket(struct iscsi_context *iscsi)
{
	int available;
	int size;
	unsigned char *buf;
	ssize_t count;

	if (ioctl(iscsi->fd, FIONREAD, &available) != 0) {
		iscsi_set_error(iscsi, "ioctl FIONREAD returned error : "
				"%d", errno);
		return -1;
	}
	if (available == 0) {
		iscsi_set_error(iscsi, "no data readable in socket, "
				"socket is closed");
		return -1;
	}
	size = iscsi->insize - iscsi->inpos + available;
	buf = malloc(size);
	if (buf == NULL) {
		iscsi_set_error(iscsi, "failed to allocate %d bytes for "
				"input buffer", size);
		return -1;
	}
	if (iscsi->insize > iscsi->inpos) {
		memcpy(buf, iscsi->inbuf + iscsi->inpos,
		       iscsi->insize - iscsi->inpos);
		iscsi->insize -= iscsi->inpos;
		iscsi->inpos   = 0;
	}

	count = read(iscsi->fd, buf + iscsi->insize, available);
	if (count == -1) {
		if (errno == EINTR) {
			free(buf);
			buf = NULL;
			return 0;
		}
		iscsi_set_error(iscsi, "read from socket failed, "
				"errno:%d", errno);
		free(buf);
		buf = NULL;
		return -1;
	}

	free(iscsi->inbuf);

	iscsi->inbuf   = buf;
	iscsi->insize += count;

	while (1) {
		if (iscsi->insize - iscsi->inpos < ISCSI_RAW_HEADER_SIZE) {
			return 0;
		}
		count = iscsi_get_pdu_size(iscsi,
					   iscsi->inbuf + iscsi->inpos);
		if (iscsi->insize + iscsi->inpos < count) {
			return 0;
		}
		if (iscsi_process_pdu(iscsi, iscsi->inbuf + iscsi->inpos,
				      count) != 0) {
			iscsi->inpos += count;
			return -1;
		}
		iscsi->inpos += count;
		if (iscsi->inpos == iscsi->insize) {
			free(iscsi->inbuf);
			iscsi->inbuf = NULL;
			iscsi->insize = 0;
			iscsi->inpos = 0;
		}
		if (iscsi->inpos > iscsi->insize) {
			iscsi_set_error(iscsi, "inpos > insize. bug!");
			return -1;
		}
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
			SLIST_ADD_END(&iscsi->waitpdu, pdu);
		}
	}
	return 0;
}

int
iscsi_service(struct iscsi_context *iscsi, int revents)
{
	if (revents & POLLERR) {
		iscsi_set_error(iscsi, "iscsi_service: POLLERR, "
				"socket error.");
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

