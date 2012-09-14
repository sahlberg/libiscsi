/*
   Copyright (C) 2011, 2012 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

#include <sys/syscall.h>
#include <dlfcn.h>

static const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:ld-iscsi";

#define ISCSI_MAX_FD  255

struct iscsi_fd_list {
       int is_iscsi;
       int dup2fd;
       int in_flight;
       struct iscsi_context *iscsi;
       int lun;
       uint32_t block_size;
       uint64_t num_blocks;
       off_t offset;
};

static struct iscsi_fd_list iscsi_fd_list[ISCSI_MAX_FD];

int (*real_open)(__const char *path, int flags, mode_t mode);

int open(const char *path, int flags, mode_t mode)
{
	int fd;

	if (!strncmp(path, "iscsi:", 6)) {
		struct iscsi_context *iscsi;
		struct iscsi_url *iscsi_url;
		struct scsi_task *task;
		struct scsi_readcapacity10 *rc10;

		iscsi = iscsi_create_context(initiator);
		if (iscsi == NULL) {
			fprintf(stderr, "ld-iscsi: Failed to create context\n");
			errno = ENOMEM;
			return -1;
		}

		iscsi_url = iscsi_parse_full_url(iscsi, path);
		if (iscsi_url == NULL) {
			fprintf(stderr, "ld-iscsi: Failed to parse URL: %s\n", 
				iscsi_get_error(iscsi));
			iscsi_destroy_context(iscsi);
			errno = EINVAL;
			return -1;
		}

		iscsi_set_targetname(iscsi, iscsi_url->target);
		iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
		iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

		if (iscsi_url->user != NULL) {
			if (iscsi_set_initiator_username_pwd(iscsi, iscsi_url->user, iscsi_url->passwd) != 0) {
				fprintf(stderr, "Failed to set initiator username and password\n");
				iscsi_destroy_context(iscsi);
				errno = ENOMEM;
				return -1;
			}
		}

		if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
			fprintf(stderr, "ld-iscsi: Login Failed. %s\n", iscsi_get_error(iscsi));
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = EIO;
			return -1;
		}

		task = iscsi_readcapacity10_sync(iscsi, iscsi_url->lun, 0, 0);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "ld-iscsi: failed to send readcapacity command\n");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = EIO;
			return -1;
		}

		rc10 = scsi_datain_unmarshall(task);
		if (rc10 == NULL) {
			fprintf(stderr, "ld-iscsi: failed to unmarshall readcapacity10 data\n");
			scsi_free_scsi_task(task);
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = EIO;
			return -1;
		}

		fd = iscsi_get_fd(iscsi);
		if (fd >= ISCSI_MAX_FD) {
			fprintf(stderr, "ld-iscsi: Too many files open\n");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = ENFILE;
			return -1;
		}		

		iscsi_fd_list[fd].is_iscsi   = 1;
		iscsi_fd_list[fd].dup2fd     = -1;
		iscsi_fd_list[fd].iscsi      = iscsi;
		iscsi_fd_list[fd].block_size = rc10->block_size;
		iscsi_fd_list[fd].num_blocks = rc10->lba + 1;
		iscsi_fd_list[fd].offset     = 0;
		iscsi_fd_list[fd].lun        = iscsi_url->lun;

		scsi_free_scsi_task(task);
		iscsi_destroy_url(iscsi_url);

		return fd;
	}

        return real_open(path, flags, mode);
}

int (*real_close)(int fd);

int close(int fd)
{
	if (iscsi_fd_list[fd].is_iscsi == 1) {
		int i;

		if (iscsi_fd_list[fd].dup2fd >= 0) {
			iscsi_fd_list[fd].is_iscsi = 0;
			iscsi_fd_list[fd].dup2fd   = -1;
			real_close(fd);
			return 0;
		}

		/* are there any FDs dup2ed onto this ? */
		for(i = 0; i < ISCSI_MAX_FD; i++) {
			if (iscsi_fd_list[i].dup2fd == fd) {
				break;
			}
		}
		if (i < ISCSI_MAX_FD) {
			int j;

			/* yes there are DUPs onto fd, make i the new real device and repoint all other
			 * duplicates
			 */
			memcpy(&iscsi_fd_list[i], &iscsi_fd_list[fd], sizeof(struct iscsi_fd_list));
			iscsi_fd_list[i].dup2fd = -1;

			memset(&iscsi_fd_list[fd], 0, sizeof(struct iscsi_fd_list));
			iscsi_fd_list[fd].dup2fd = -1;

			iscsi_fd_list[i].iscsi->fd = i;
			real_close(fd);

			for(j = 0; j < ISCSI_MAX_FD; j++) {
				if (j != i && iscsi_fd_list[j].dup2fd == fd) {
					iscsi_fd_list[j].dup2fd = i;
				}
			}

			return 0;
		}

		iscsi_fd_list[fd].is_iscsi = 0;
		iscsi_fd_list[fd].dup2fd   = -1;
		iscsi_destroy_context(iscsi_fd_list[fd].iscsi);
		iscsi_fd_list[fd].iscsi    = NULL;

		return 0;
	}

        return real_close(fd);
}

int (*real_fxstat)(int ver, int fd, struct stat *buf);

int __fxstat(int ver, int fd, struct stat *buf)
{
	if (iscsi_fd_list[fd].is_iscsi == 1) {
		if (iscsi_fd_list[fd].dup2fd >= 0) {
			return __fxstat(ver, iscsi_fd_list[fd].dup2fd, buf);
		}

		memset(buf, 0, sizeof(struct stat));
		buf->st_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IFREG;
		buf->st_size = iscsi_fd_list[fd].num_blocks * iscsi_fd_list[fd].block_size;
		buf->st_blksize = iscsi_fd_list[fd].block_size;

		return 0;
	}

	return real_fxstat(ver, fd, buf);
}


int (*real_lxstat)(int ver, __const char *path, struct stat *buf);

int __lxstat(int ver, const char *path, struct stat *buf)
{
	if (!strncmp(path, "iscsi:", 6)) {
		int fd, ret;

		fd = open(path, 0, 0);
		if (fd == -1) {
			return fd;
		}

		ret = __fxstat(ver, fd, buf);
		close(fd);
		return ret;		
	}

	return real_lxstat(ver, path, buf);
}

int (*real_xstat)(int ver, __const char *path, struct stat *buf);

int __xstat(int ver, const char *path, struct stat *buf)
{
	return __lxstat(ver, path, buf);
}

ssize_t (*real_read)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count)
{
	if ((iscsi_fd_list[fd].is_iscsi == 1) && (iscsi_fd_list[fd].in_flight == 0)) {
		uint64_t offset;
		uint32_t num_blocks;
		struct scsi_task *task;

		if (iscsi_fd_list[fd].dup2fd >= 0) {
			return read(iscsi_fd_list[fd].dup2fd, buf, count);
		}
		offset = iscsi_fd_list[fd].offset / iscsi_fd_list[fd].block_size * iscsi_fd_list[fd].block_size;
		num_blocks = (iscsi_fd_list[fd].offset - offset + count + iscsi_fd_list[fd].block_size - 1) / iscsi_fd_list[fd].block_size;

		iscsi_fd_list[fd].in_flight = 1;
		task = iscsi_read10_sync(iscsi_fd_list[fd].iscsi, iscsi_fd_list[fd].lun, offset / iscsi_fd_list[fd].block_size, num_blocks * iscsi_fd_list[fd].block_size, iscsi_fd_list[fd].block_size, 0, 0, 0, 0, 0);
		iscsi_fd_list[fd].in_flight = 0;
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			fprintf(stderr, "ld-iscsi: failed to send read10 command\n");
			errno = EIO;
			return -1;
		}

		memcpy(buf, &task->datain.data[iscsi_fd_list[fd].offset - offset], count);
		iscsi_fd_list[fd].offset += count;

		scsi_free_scsi_task(task);

		return count;
	}

	return real_read(fd, buf, count);
}


int (*real_dup2)(int oldfd, int newfd);

int dup2(int oldfd, int newfd)
{
	close(newfd);

	if (iscsi_fd_list[oldfd].is_iscsi == 1) {
		int ret;
		if (iscsi_fd_list[oldfd].dup2fd >= 0) {
			return dup2(iscsi_fd_list[oldfd].dup2fd, newfd);
		}

		ret = real_dup2(oldfd, newfd);
		if (ret < 0) {
			return ret;
		}

		iscsi_fd_list[newfd].is_iscsi = 1;
		iscsi_fd_list[newfd].dup2fd   = oldfd;

		return newfd;
	}

	return real_dup2(oldfd, newfd);
}


static void __attribute__((constructor)) _init(void)
{
	int i;

	for(i = 0; i < ISCSI_MAX_FD; i++) {
		iscsi_fd_list[i].dup2fd = -1;
	}

	real_open = dlsym(RTLD_NEXT, "open");
	if (real_open == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(open)\n");
		exit(10);
	}

	real_close = dlsym(RTLD_NEXT, "close");
	if (real_close == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(close)\n");
		exit(10);
	}

	real_fxstat = dlsym(RTLD_NEXT, "__fxstat");
	if (real_fxstat == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(__fxstat)\n");
		exit(10);
	}

	real_lxstat = dlsym(RTLD_NEXT, "__lxstat");
	if (real_lxstat == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(__lxstat)\n");
		exit(10);
	}
	real_xstat = dlsym(RTLD_NEXT, "__xstat");
	if (real_xstat == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(__xstat)\n");
		exit(10);
	}

	real_read = dlsym(RTLD_NEXT, "read");
	if (real_read == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(read)\n");
		exit(10);
	}

	real_dup2 = dlsym(RTLD_NEXT, "dup2");
	if (real_dup2 == NULL) {
		fprintf(stderr, "ld_iscsi: Failed to dlsym(dup2)\n");
		exit(10);
	}
}
