/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2025 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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

#ifndef _LIBISCSI_MULTITHREADING_H_
#define _LIBISCSI_MULTITHREADING_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_MULTITHREADING

#ifdef WIN32
typedef HANDLE libiscsi_thread_t;
typedef HANDLE libiscsi_mutex_t;
typedef HANDLE libiscsi_sem_t;
typedef DWORD iscsi_tid_t;
#elif defined(HAVE_PTHREAD)
#include <pthread.h>
typedef pthread_t libiscsi_thread_t;
typedef pthread_mutex_t libiscsi_mutex_t;

#if defined(__APPLE__) && defined(HAVE_DISPATCH_DISPATCH_H)
#include <dispatch/dispatch.h>
typedef dispatch_semaphore_t libiscsi_sem_t;
#else
#include <semaphore.h>
typedef sem_t libiscsi_sem_t;
#endif
#ifdef HAVE_PTHREAD_THREADID_NP
typedef uint64_t iscsi_tid_t;
#else
typedef pid_t iscsi_tid_t;
#endif
#endif /* HAVE_PTHREAD */

iscsi_tid_t iscsi_mt_get_tid(void);
int iscsi_mt_mutex_init(libiscsi_mutex_t *mutex);
int iscsi_mt_mutex_destroy(libiscsi_mutex_t *mutex);
int iscsi_mt_mutex_lock(libiscsi_mutex_t *mutex);
int iscsi_mt_mutex_unlock(libiscsi_mutex_t *mutex);

int iscsi_mt_sem_init(libiscsi_sem_t *sem, int value);
int iscsi_mt_sem_destroy(libiscsi_sem_t *sem);
int iscsi_mt_sem_post(libiscsi_sem_t *sem);
int iscsi_mt_sem_wait(libiscsi_sem_t *sem);

#endif /* HAVE_MULTITHREADING */

#ifdef __cplusplus
}
#endif

#endif /* !_LIBISCSI_MULTITHREADING_H_ */
