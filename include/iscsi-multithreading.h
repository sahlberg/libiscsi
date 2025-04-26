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
typedef HANDLE libiscsi_sem_t;
typedef DWORD iscsi_tid_t;
#elif defined(HAVE_PTHREAD)
#include <pthread.h>
typedef pthread_t libiscsi_thread_t;

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



int iscsi_mt_sem_init(libiscsi_sem_t *sem, int value);
int iscsi_mt_sem_destroy(libiscsi_sem_t *sem);
int iscsi_mt_sem_post(libiscsi_sem_t *sem);
int iscsi_mt_sem_wait(libiscsi_sem_t *sem);

#endif /* HAVE_MULTITHREADING */

/*
 * We always have access to mutex functions even if multithreading
 * is not enabled.
 */
#if defined(HAVE_PTHREAD)
typedef pthread_mutex_t libiscsi_mutex_t;
/*
 * If this is enabled we check for the following locking violations, at the
 * (slight) cost of performance:
 * - Thread holding the lock again tries to lock.
 * - Thread not holding the lock tries to unlock.
 *
 * This is very useful for catching any coding errors.
 * The performance hit is not very significant so you can leave it enabled,
 * but if you really care then once the code has been vetted, this can be
 * undef'ed to get the perf back.
 */
#define DEBUG_PTHREAD_LOCKING_VIOLATIONS

static inline int iscsi_mt_mutex_init(libiscsi_mutex_t *mutex)
{
	int ret;
#ifdef DEBUG_PTHREAD_LOCKING_VIOLATIONS
	pthread_mutexattr_t attr;

	ret = pthread_mutexattr_init(&attr);
	if (ret != 0) {
		return ret;
	}

	ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
	if (ret != 0) {
		return ret;
	}

	ret = pthread_mutex_init(mutex, &attr);
	if (ret != 0) {
		return ret;
	}
#else
	ret = pthread_mutex_init(mutex, NULL);
	assert(ret == 0);
#endif
	return ret;
}

static inline int iscsi_mt_mutex_destroy(libiscsi_mutex_t *mutex)
{
	return pthread_mutex_destroy(mutex);
}

static inline int iscsi_mt_mutex_lock(libiscsi_mutex_t *mutex)
{
	return pthread_mutex_lock(mutex);
}

static inline int iscsi_mt_mutex_unlock(libiscsi_mutex_t *mutex)
{
	return pthread_mutex_unlock(mutex);
}

typedef pthread_spinlock_t libiscsi_spinlock_t;
        static inline int iscsi_mt_spin_init(libiscsi_spinlock_t *spinlock, int shared)
{
	return pthread_spin_init(spinlock, shared);
}
static inline int iscsi_mt_spin_destroy(libiscsi_spinlock_t *spinlock)
{
	return pthread_spin_destroy(spinlock);
}
static inline int iscsi_mt_spin_lock(libiscsi_spinlock_t *spinlock)
{
	return pthread_spin_lock(spinlock);
}
static inline int iscsi_mt_spin_unlock(libiscsi_spinlock_t *spinlock)
{
	return pthread_spin_unlock(spinlock);
}
        
#elif defined(WIN32)
typedef HANDLE libiscsi_mutex_t;
        static inline int iscsi_mt_mutex_init(libiscsi_mutex_t* mutex)
{
    *mutex = CreateSemaphoreA(NULL, 1, 1, NULL);
    return 0;
}

static inline int iscsi_mt_mutex_destroy(libiscsi_mutex_t* mutex)
{
    CloseHandle(*mutex);
    return 0;
}

static inline int iscsi_mt_mutex_lock(libiscsi_mutex_t* mutex)
{
    while (WaitForSingleObject(*mutex, INFINITE) != WAIT_OBJECT_0);
    return 0;
}

static inline int iscsi_mt_mutex_unlock(libiscsi_mutex_t* mutex)
{
    ReleaseSemaphore(*mutex, 1, NULL);
    return 0;
}

#else

typedef const int libiscsi_mutex_t;
#define iscsi_mt_mutex_init(x) ;
#define iscsi_mt_mutex_destroy(x) ;
#define iscsi_mt_mutex_lock(x) ;
#define iscsi_mt_mutex_unlock(x) ;
typedef const int libiscsi_spinlock_t;
#define iscsi_mt_spin_init(x) ;
#define iscsi_mt_spin_destroy(x) ;
#define iscsi_mt_spin_lock(x) ;
#define iscsi_mt_spin_unlock(x) ;

#endif /* mutex */


#ifdef __cplusplus
}
#endif

#endif /* !_LIBISCSI_MULTITHREADING_H_ */
