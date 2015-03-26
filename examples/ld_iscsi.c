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
#include <asm/fcntl.h>

#include "iscsi.h"
#include "iscsi-private.h"
#include "scsi-lowlevel.h"

#include <sys/syscall.h>
#include <dlfcn.h>
#include <inttypes.h>

static const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:ld-iscsi";

#define ISCSI_MAX_FD  255

static int debug = 0;

#define LD_ISCSI_DPRINTF(level,fmt,args...) do { if ((debug) >= level) {fprintf(stderr,"ld_iscsi: ");fprintf(stderr, (fmt), ##args); fprintf(stderr,"\n");} } while (0);

struct iscsi_fd_list {
       int is_iscsi;
       int dup2fd;
       int in_flight;
       struct iscsi_context *iscsi;
       int lun;
       uint32_t block_size;
       uint64_t num_blocks;
       off_t offset;
       mode_t mode;
       int get_lba_status;
       struct scsi_lba_status_descriptor lbasd_cached;
       int lbasd_cache_valid;
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
		struct scsi_readcapacity16 *rc16;

		if (mode & O_NONBLOCK) {
			LD_ISCSI_DPRINTF(0,"Non-blocking I/O is currently not supported");
			errno = EINVAL;
			return -1;
		}

		iscsi = iscsi_create_context(initiator);
		if (iscsi == NULL) {
			LD_ISCSI_DPRINTF(0,"Failed to create context");
			errno = ENOMEM;
			return -1;
		}

		iscsi_url = iscsi_parse_full_url(iscsi, path);
		if (iscsi_url == NULL) {
			LD_ISCSI_DPRINTF(0,"Failed to parse URL: %s\n", iscsi_get_error(iscsi));
			iscsi_destroy_context(iscsi);
			errno = EINVAL;
			return -1;
		}

		iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
		iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

		if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
			LD_ISCSI_DPRINTF(0,"Login Failed. %s\n", iscsi_get_error(iscsi));
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = EIO;
			return -1;
		}

		task = iscsi_readcapacity16_sync(iscsi, iscsi_url->lun);
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			LD_ISCSI_DPRINTF(0,"failed to send readcapacity command");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = EIO;
			return -1;
		}

		rc16 = scsi_datain_unmarshall(task);
		if (rc16 == NULL) {
			LD_ISCSI_DPRINTF(0,"failed to unmarshall readcapacity10 data");
			scsi_free_scsi_task(task);
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = EIO;
			return -1;
		}

        LD_ISCSI_DPRINTF(4,"readcapacity16_sync: block_size: %d, num_blocks: %"PRIu64,rc16->block_length,rc16->returned_lba + 1);

		fd = iscsi_get_fd(iscsi);
		if (fd >= ISCSI_MAX_FD) {
			LD_ISCSI_DPRINTF(0,"Too many files open");
			iscsi_destroy_url(iscsi_url);
			iscsi_destroy_context(iscsi);
			errno = ENFILE;
			return -1;
		}

		iscsi_fd_list[fd].is_iscsi   = 1;
		iscsi_fd_list[fd].dup2fd     = -1;
		iscsi_fd_list[fd].iscsi      = iscsi;
		iscsi_fd_list[fd].block_size = rc16->block_length;
		iscsi_fd_list[fd].num_blocks = rc16->returned_lba + 1;
		iscsi_fd_list[fd].offset     = 0;
		iscsi_fd_list[fd].lun        = iscsi_url->lun;
		iscsi_fd_list[fd].mode       = mode;

		if (getenv("LD_ISCSI_GET_LBA_STATUS") != NULL) {
			iscsi_fd_list[fd].get_lba_status = atoi(getenv("LD_ISCSI_GET_LBA_STATUS"));
			if (rc16->lbpme == 0){
				LD_ISCSI_DPRINTF(1,"Logical unit is fully provisioned. Will skip get_lba_status tasks");
				iscsi_fd_list[fd].get_lba_status = 0;
			}
		}

		scsi_free_scsi_task(task);
		iscsi_destroy_url(iscsi_url);

		return fd;
	}

	return real_open(path, flags, mode);
}

int open64(const char *path, int flags, mode_t mode)
{
	return open(path, flags | O_LARGEFILE, mode);
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

off_t (*real_lseek)(int fd, off_t offset, int whence);

off_t lseek(int fd, off_t offset, int whence) {
	if (iscsi_fd_list[fd].is_iscsi == 1) {
		off_t new_offset;
		off_t size = iscsi_fd_list[fd].num_blocks*iscsi_fd_list[fd].block_size;
		switch (whence) {
			case SEEK_SET:
				new_offset = offset;
				break;
			case SEEK_CUR:
				new_offset = iscsi_fd_list[fd].offset+offset;
				break;
			case SEEK_END:
				new_offset = size + offset;
				break;
			default:
				errno = EINVAL;
				return -1;
		}
		if (new_offset < 0 || new_offset > size) {
			errno = EINVAL;
			return -1;
		}
		iscsi_fd_list[fd].offset=new_offset;
		return iscsi_fd_list[fd].offset;
	}

	return real_lseek(fd, offset, whence);
}

ssize_t (*real_read)(int fd, void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count)
{
	if ((iscsi_fd_list[fd].is_iscsi == 1) && (iscsi_fd_list[fd].in_flight == 0)) {
		uint64_t offset;
		uint64_t num_blocks, lba;
		struct scsi_task *task;
		struct scsi_get_lba_status *lbas;

		if (iscsi_fd_list[fd].dup2fd >= 0) {
			return read(iscsi_fd_list[fd].dup2fd, buf, count);
		}
		offset = iscsi_fd_list[fd].offset / iscsi_fd_list[fd].block_size * iscsi_fd_list[fd].block_size;
		num_blocks = (iscsi_fd_list[fd].offset - offset + count + iscsi_fd_list[fd].block_size - 1) / iscsi_fd_list[fd].block_size;
		lba = offset / iscsi_fd_list[fd].block_size;

		/* Don't try to read beyond the last LBA */
		if (lba >= iscsi_fd_list[fd].num_blocks) {
			return 0;
		}
		/* Trim num_blocks requested to last lba */
		if ((lba + num_blocks) > iscsi_fd_list[fd].num_blocks) {
			num_blocks = iscsi_fd_list[fd].num_blocks - lba;
			count = num_blocks * iscsi_fd_list[fd].block_size;
		}

		iscsi_fd_list[fd].in_flight = 1;
        if (iscsi_fd_list[fd].get_lba_status != 0) {
			uint32_t i;
			uint32_t _num_allocated=0;
			uint32_t _num_blocks=0;

			if (iscsi_fd_list[fd].lbasd_cache_valid==1) {
				LD_ISCSI_DPRINTF(5,"cached get_lba_status_descriptor is lba %"PRIu64", num_blocks %d, provisioning %d",iscsi_fd_list[fd].lbasd_cached.lba,iscsi_fd_list[fd].lbasd_cached.num_blocks,iscsi_fd_list[fd].lbasd_cached.provisioning);
			    if (iscsi_fd_list[fd].lbasd_cached.provisioning != 0x00 && lba >= iscsi_fd_list[fd].lbasd_cached.lba && lba+num_blocks < iscsi_fd_list[fd].lbasd_cached.lba+iscsi_fd_list[fd].lbasd_cached.num_blocks)
			    {
					LD_ISCSI_DPRINTF(4,"skipped read16_sync for non-allocated blocks: lun %d, lba %"PRIu64", num_blocks: %"PRIu64", block_size: %d, offset: %"PRIu64" count: %lu",iscsi_fd_list[fd].lun,lba,num_blocks,iscsi_fd_list[fd].block_size,offset,(unsigned long)count);
					memset(buf, 0x00, count);
					iscsi_fd_list[fd].offset += count;
					iscsi_fd_list[fd].in_flight = 0;
					return count;
				}
			}
			LD_ISCSI_DPRINTF(4,"get_lba_status_sync: lun %d, lba %"PRIu64", num_blocks: %"PRIu64,iscsi_fd_list[fd].lun,lba,num_blocks);
			task = iscsi_get_lba_status_sync(iscsi_fd_list[fd].iscsi, iscsi_fd_list[fd].lun, lba, 8+16);
			if (task == NULL || task->status != SCSI_STATUS_GOOD) {
				LD_ISCSI_DPRINTF(0,"failed to send get_lba_status command");
				iscsi_fd_list[fd].in_flight = 0;
				errno = EIO;
				return -1;
			}
			lbas = scsi_datain_unmarshall(task);
			if (lbas == NULL) {
				LD_ISCSI_DPRINTF(0,"failed to unmarshall get_lba_status data");
				scsi_free_scsi_task(task);
				iscsi_fd_list[fd].in_flight = 0;
				errno = EIO;
				return -1;
			}

			LD_ISCSI_DPRINTF(5,"get_lba_status: num_descriptors: %d",lbas->num_descriptors);
			for (i=0;i<lbas->num_descriptors;i++) {
				struct scsi_lba_status_descriptor *lbasd = &lbas->descriptors[i];
				LD_ISCSI_DPRINTF(5,"get_lba_status_descriptor %d, lba %"PRIu64", num_blocks %d, provisioning %d",i,lbasd->lba,lbasd->num_blocks,lbasd->provisioning);
				if (lbasd->lba != _num_blocks+lba) {
					LD_ISCSI_DPRINTF(0,"get_lba_status response is non-continuous");
					scsi_free_scsi_task(task);
					iscsi_fd_list[fd].in_flight = 0;
					errno = EIO;
					return -1;
			    }
				_num_allocated+=(lbasd->provisioning==0x00)?lbasd->num_blocks:0;
				_num_blocks+=lbasd->num_blocks;
				iscsi_fd_list[fd].lbasd_cached=lbas->descriptors[i];
				iscsi_fd_list[fd].lbasd_cache_valid=1;
			}
			scsi_free_scsi_task(task);
            if (_num_allocated == 0 && _num_blocks >= num_blocks) {
		        LD_ISCSI_DPRINTF(4,"skipped read16_sync for non-allocated blocks: lun %d, lba %"PRIu64", num_blocks: %"PRIu64", block_size: %d, offset: %"PRIu64" count: %lu",iscsi_fd_list[fd].lun,lba,num_blocks,iscsi_fd_list[fd].block_size,offset,(unsigned long)count);
				memset(buf, 0x00, count);
		        iscsi_fd_list[fd].offset += count;
		        iscsi_fd_list[fd].in_flight = 0;
		        return count;
			}
		}

		LD_ISCSI_DPRINTF(4,"read16_sync: lun %d, lba %"PRIu64", num_blocks: %"PRIu64", block_size: %d, offset: %"PRIu64" count: %lu",iscsi_fd_list[fd].lun,lba,num_blocks,iscsi_fd_list[fd].block_size,offset,(unsigned long)count);

		task = iscsi_read16_sync(iscsi_fd_list[fd].iscsi, iscsi_fd_list[fd].lun, lba, num_blocks * iscsi_fd_list[fd].block_size, iscsi_fd_list[fd].block_size, 0, 0, 0, 0, 0);
		iscsi_fd_list[fd].in_flight = 0;
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			LD_ISCSI_DPRINTF(0,"failed to send read16 command");
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

ssize_t (*real_pread)(int fd, void *buf, size_t count, off_t offset);
ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
	if ((iscsi_fd_list[fd].is_iscsi == 1 && iscsi_fd_list[fd].in_flight == 0)) {
		off_t old_offset;
		if ((old_offset = lseek(fd, 0, SEEK_CUR)) < 0) {
			errno = EIO;
			return -1;
		}
		if (lseek(fd, offset, SEEK_SET) < 0) {
			return -1;
		}
		if (read(fd, buf, count) < 0) {
			lseek(fd, old_offset, SEEK_SET);
			return -1;
		}
		lseek(fd, old_offset, SEEK_SET);
		return count;
	}
	return real_pread(fd, buf, count, offset);
}

ssize_t (*real_write)(int fd, const void *buf, size_t count);

ssize_t write(int fd, const void *buf, size_t count)
{
	if ((iscsi_fd_list[fd].is_iscsi == 1) && (iscsi_fd_list[fd].in_flight == 0)) {
		uint64_t offset;
		uint64_t num_blocks, lba;
		struct scsi_task *task;

		if (iscsi_fd_list[fd].dup2fd >= 0) {
			return write(iscsi_fd_list[fd].dup2fd, buf, count);
		}
		if (iscsi_fd_list[fd].offset%iscsi_fd_list[fd].block_size) {
			errno = EINVAL;
			return -1;
		}
		if (count%iscsi_fd_list[fd].block_size) {
			errno = EINVAL;
			return -1;
		}

                iscsi_fd_list[fd].lbasd_cache_valid = 0;

		offset = iscsi_fd_list[fd].offset;
		num_blocks = count/iscsi_fd_list[fd].block_size;
		lba = offset / iscsi_fd_list[fd].block_size;

		/* Don't try to read beyond the last LBA */
		if (lba >= iscsi_fd_list[fd].num_blocks) {
			return 0;
		}
		/* Trim num_blocks requested to last lba */
		if ((lba + num_blocks) > iscsi_fd_list[fd].num_blocks) {
			num_blocks = iscsi_fd_list[fd].num_blocks - lba;
			count = num_blocks * iscsi_fd_list[fd].block_size;
		}

		iscsi_fd_list[fd].in_flight = 1;
		LD_ISCSI_DPRINTF(4,"write16_sync: lun %d, lba %"PRIu64", num_blocks: %"PRIu64", block_size: %d, offset: %"PRIu64" count: %lu",iscsi_fd_list[fd].lun,lba,num_blocks,iscsi_fd_list[fd].block_size,offset,(unsigned long)count);
		task = iscsi_write16_sync(iscsi_fd_list[fd].iscsi, iscsi_fd_list[fd].lun, lba, (unsigned char *) buf, count, iscsi_fd_list[fd].block_size, 0, 0, 0, 0, 0);
		iscsi_fd_list[fd].in_flight = 0;
		if (task == NULL || task->status != SCSI_STATUS_GOOD) {
			LD_ISCSI_DPRINTF(0,"failed to send write16 command");
			errno = EIO;
			return -1;
		}

		iscsi_fd_list[fd].offset += count;
		scsi_free_scsi_task(task);

		return count;
	}

	return real_write(fd, buf, count);
}

ssize_t (*real_pwrite)(int fd, const void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
	if ((iscsi_fd_list[fd].is_iscsi == 1 && iscsi_fd_list[fd].in_flight == 0)) {
		off_t old_offset;
		if ((old_offset = lseek(fd, 0, SEEK_CUR)) < 0) {
			errno = EIO;
			return -1;
		}
		if (lseek(fd, offset, SEEK_SET) < 0) {
			return -1;
		}
		if (write(fd, buf, count) < 0) {
			lseek(fd, old_offset, SEEK_SET);
			return -1;
		}
		lseek(fd, old_offset, SEEK_SET);
		return count;
	}
	return real_pwrite(fd, buf, count, offset);
}

int (*real_dup2)(int oldfd, int newfd);

int dup2(int oldfd, int newfd)
{
	if (iscsi_fd_list[newfd].is_iscsi) {
		return real_dup2(oldfd, newfd);
	}

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

#if defined(_LARGEFILE64_SOURCE) && _FILE_OFFSET_BITS != 64

int (*real_fxstat64)(int ver, int fd, struct stat64 *buf);

int __fxstat64(int ver, int fd, struct stat64 *buf)
{
	if (iscsi_fd_list[fd].is_iscsi == 1) {
		if (iscsi_fd_list[fd].dup2fd >= 0) {
			return __fxstat64(ver, iscsi_fd_list[fd].dup2fd, buf);
		}

		memset(buf, 0, sizeof(struct stat64));
		buf->st_mode = S_IRUSR | S_IRGRP | S_IROTH | S_IFREG;
		buf->st_size = iscsi_fd_list[fd].num_blocks * iscsi_fd_list[fd].block_size;
		return 0;
	}

	return real_fxstat64(ver, fd, buf);
}


int (*real_lxstat64)(int ver, __const char *path, struct stat64 *buf);

int __lxstat64(int ver, const char *path, struct stat64 *buf)
{
	if (!strncmp(path, "iscsi:", 6)) {
		int fd, ret;

		fd = open64(path, 0, 0);
		if (fd == -1) {
			return fd;
		}

		ret = __fxstat64(ver, fd, buf);
		close(fd);
		return ret;
	}

	return real_lxstat64(ver, path, buf);
}


int (*real_xstat64)(int ver, __const char *path, struct stat64 *buf);

int __xstat64(int ver, const char *path, struct stat64 *buf)
{
	return __lxstat64(ver, path, buf);
}

#endif

static void __attribute__((constructor)) _init(void)
{
	int i;

	for(i = 0; i < ISCSI_MAX_FD; i++) {
		iscsi_fd_list[i].dup2fd = -1;
	}

	if (getenv("LD_ISCSI_DEBUG") != NULL) {
		debug = atoi(getenv("LD_ISCSI_DEBUG"));
	}

	real_open = dlsym(RTLD_NEXT, "open");
	if (real_open == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(open)");
		exit(10);
	}

	real_close = dlsym(RTLD_NEXT, "close");
	if (real_close == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(close)");
		exit(10);
	}

	real_fxstat = dlsym(RTLD_NEXT, "__fxstat");
	if (real_fxstat == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(__fxstat)");
		exit(10);
	}

	real_lxstat = dlsym(RTLD_NEXT, "__lxstat");
	if (real_lxstat == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(__lxstat)");
		exit(10);
	}
	real_xstat = dlsym(RTLD_NEXT, "__xstat");
	if (real_xstat == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(__xstat)");
		exit(10);
	}

	real_lseek = dlsym(RTLD_NEXT, "lseek");
	if (real_lseek == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(lseek)");
		exit(10);
	}

	real_read = dlsym(RTLD_NEXT, "read");
	if (real_read == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(read)");
		exit(10);
	}

	real_pread = dlsym(RTLD_NEXT, "pread");
	if (real_pread == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(pread)");
		exit(10);
	}

	real_write = dlsym(RTLD_NEXT, "write");
	if (real_write == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(write)");
		exit(10);
	}

	real_pwrite = dlsym(RTLD_NEXT, "pwrite");
	if (real_pwrite == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(pwrite)");
		exit(10);
	}

	real_dup2 = dlsym(RTLD_NEXT, "dup2");
	if (real_dup2 == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(dup2)");
		exit(10);
	}

#if defined(_LARGEFILE64_SOURCE) && _FILE_OFFSET_BITS != 64
	real_fxstat64 = dlsym(RTLD_NEXT, "__fxstat64");
	if (real_fxstat64 == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(__fxstat64)");
	}

	real_lxstat64 = dlsym(RTLD_NEXT, "__lxstat64");
	if (real_lxstat64 == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(_lxstat64)");
	}

	real_xstat64 = dlsym(RTLD_NEXT, "__xstat64");
	if (real_xstat64 == NULL) {
		LD_ISCSI_DPRINTF(0,"Failed to dlsym(__xstat64)");
	}
#endif
}
