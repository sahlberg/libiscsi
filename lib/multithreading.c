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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef AROS
#include "aros_compat.h"
#endif

#ifdef WIN32
#include "win32_compat.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"

#ifdef HAVE_MULTITHREADING

#ifdef WIN32
iscsi_tid_t iscsi_mt_get_tid(void)
{
    return GetCurrentThreadId();
}

/*
 * Windows can not interrupt select() with a signal, so the service thread
 * also polls a UDP socket connected to itself, and a datagram on it does
 * what SIGUSR1 does on POSIX.
 */
static SOCKET create_wake_socket(void)
{
    struct sockaddr_in sin;
    int len = sizeof(sin);
    u_long nonblocking = 1;
    SOCKET s;

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sin.sin_port = 0;
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) != 0 ||
        getsockname(s, (struct sockaddr *)&sin, &len) != 0 ||
        connect(s, (struct sockaddr *)&sin, sizeof(sin)) != 0 ||
        ioctlsocket(s, FIONBIO, &nonblocking) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

void iscsi_mt_service_thread_wake(struct iscsi_context *iscsi)
{
    char c = 0;

    send(iscsi->service_wake_fd, &c, 1, 0);
}

static void* iscsi_mt_service_thread(void* arg)
{
    struct iscsi_context* iscsi = (struct iscsi_context*)arg;
    struct pollfd pfd[2];
    char buf[64];
    int revents;
    int ret;

    iscsi->service_tid = iscsi_mt_get_tid();
    iscsi->multithreading_enabled = 1;

    while (iscsi->multithreading_enabled) {
        pfd[0].fd = iscsi_get_fd(iscsi);
        pfd[0].events = iscsi_which_events(iscsi);
        pfd[0].revents = 0;
        pfd[1].fd = iscsi->service_wake_fd;
        pfd[1].events = POLLIN;
        pfd[1].revents = 0;

        ret = poll(pfd, 2, iscsi->poll_timeout);
        if (ret < 0) {
            iscsi_set_error(iscsi, "Poll failed");
            revents = -1;
        }
        else {
            revents = pfd[0].revents;
            if (pfd[1].revents & POLLIN) {
                /* We were woken up to start writing new PDUs. */
                while (recv(iscsi->service_wake_fd, buf, sizeof(buf), 0) > 0)
                    ;
                revents |= POLLOUT;
            }
        }
        if (iscsi_service(iscsi, revents) < 0) {
            if (revents != -1)
                iscsi_set_error(iscsi, "iscsi_service failed");
        }
        iscsi_mt_service_sync_timeouts(iscsi);
    }
    return NULL;
}

static DWORD WINAPI service_thread_init(LPVOID lpParam)
{
    struct iscsi_context* iscsi;

    iscsi = (struct iscsi_context *)lpParam;
    iscsi_mt_service_thread(iscsi);
    return 0;
}

int iscsi_mt_service_thread_start(struct iscsi_context* iscsi)
{
    iscsi->service_wake_fd = create_wake_socket();
    if (iscsi->service_wake_fd == INVALID_SOCKET) {
        iscsi_set_error(iscsi, "Failed to create the service thread "
                        "wakeup socket");
        return -1;
    }
    iscsi->service_thread = CreateThread(NULL, 1024*1024, service_thread_init, iscsi, 0, NULL);
    if (iscsi->service_thread == NULL) {
        iscsi_set_error(iscsi, "Failed to start service thread");
        closesocket(iscsi->service_wake_fd);
        iscsi->service_wake_fd = INVALID_SOCKET;
        return -1;
    }
    while (iscsi->multithreading_enabled == 0) {
        Sleep(1);
    }
    return 0;
}

void iscsi_mt_service_thread_stop(struct iscsi_context* iscsi)
{
    iscsi->multithreading_enabled = 0;
    iscsi_mt_service_thread_wake(iscsi);
    while (WaitForSingleObject(iscsi->service_thread, INFINITE) != WAIT_OBJECT_0);
    CloseHandle(iscsi->service_thread);
    closesocket(iscsi->service_wake_fd);
    iscsi->service_wake_fd = INVALID_SOCKET;
}

int iscsi_mt_sem_init(libiscsi_sem_t* sem, int value)
{
    *sem = CreateSemaphoreA(NULL, 0, 16, NULL);
    return 0;
}

int iscsi_mt_sem_destroy(libiscsi_sem_t* sem)
{
    CloseHandle(*sem);
    return 0;
}

int iscsi_mt_sem_post(libiscsi_sem_t* sem)
{
    ReleaseSemaphore(*sem, 1, NULL);
    return 0;
}

int iscsi_mt_sem_wait(libiscsi_sem_t* sem)
{
    while (WaitForSingleObject(*sem, INFINITE) != WAIT_OBJECT_0);
    return 0;
}

int iscsi_mt_sem_timedwait(libiscsi_sem_t* sem, int ms)
{
    return WaitForSingleObject(*sem, ms) == WAIT_OBJECT_0 ? 0 : -1;
}

#elif defined(HAVE_PTHREAD) /* WIN32 */

#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <pthread_np.h>
#endif
#if defined(__NetBSD__)
#include <lwp.h>
#endif
#if defined(__illumos__)
#include <thread.h>
#endif

iscsi_tid_t iscsi_mt_get_tid(void)
{
#ifdef HAVE_PTHREAD_THREADID_NP
        iscsi_tid_t tid;
        pthread_threadid_np(NULL, &tid);
        return tid;
#elif defined(__FreeBSD__) || defined(__DragonFly__)
        int tid = pthread_getthreadid_np();
        return tid;
#elif defined(__OpenBSD__)
        pid_t tid = getthrid();
        return tid;
#elif defined(__NetBSD__)
        lwpid_t tid = _lwp_self();
        return tid;
#elif defined(__illumos__)
        thread_t tid = thr_self();
        return tid;
#elif defined(SYS_gettid)
        pid_t tid = syscall(SYS_gettid);
        return tid;
#else
#error "SYS_gettid unavailable on this system"
#endif
}

static void on_sigusr1(int _unused)
{
}

static void *iscsi_mt_service_thread(void *arg)
{
        struct iscsi_context *iscsi = (struct iscsi_context *)arg;
	struct pollfd pfd;
	int revents;
	int ret;
        
        /* set signal to break poll when we need to send more data */
        signal(SIGUSR1, on_sigusr1);
        
        iscsi->service_tid = iscsi_mt_get_tid();
        iscsi->multithreading_enabled = 1;

        /* TODO: add timeout scanning */
	while (iscsi->multithreading_enabled) {
		pfd.fd = iscsi_get_fd(iscsi);
		pfd.events = iscsi_which_events(iscsi);
		pfd.revents = 0;
        
		ret = poll(&pfd, 1, iscsi->poll_timeout);
                if (ret < 0 && errno == EINTR) {
                        /*
                         * Got a signal. Assume it is because we need to start writing new PDUs
                         * to the socket.
                         */
                        revents = POLLOUT;
                        goto call_service;
                }
                if (ret < 0) {
			iscsi_set_error(iscsi, "Poll failed");
			revents = -1;
		} else {
			revents = pfd.revents;
		}
        call_service:                
		if (iscsi_service(iscsi, revents) < 0) {
			if (revents != -1)
				iscsi_set_error(iscsi, "iscsi_service failed");
		}
		iscsi_mt_service_sync_timeouts(iscsi);
	}
        return NULL;
}

int iscsi_mt_service_thread_start(struct iscsi_context *iscsi)
{
        if (pthread_create(&iscsi->service_thread, NULL,
                           &iscsi_mt_service_thread, iscsi)) {
                iscsi_set_error(iscsi, "Failed to start service thread");
                return -1;
        }
        while (iscsi->multithreading_enabled == 0) {
                struct timespec ts = {0, 1000000};
                nanosleep(&ts, NULL);
        }
        return 0;
}

void iscsi_mt_service_thread_stop(struct iscsi_context *iscsi)
{
        iscsi->multithreading_enabled = 0;
        pthread_join(iscsi->service_thread, NULL);
}

void iscsi_mt_service_thread_wake(struct iscsi_context *iscsi)
{
        pthread_kill(iscsi->service_thread, SIGUSR1);
}
        
#if defined(__APPLE__) && defined(HAVE_DISPATCH_DISPATCH_H)
int iscsi_mt_sem_init(libiscsi_sem_t *sem, int value)
{
        if ((*sem = dispatch_semaphore_create(value)) != NULL)
                return 0;
        return -1;
}

int iscsi_mt_sem_destroy(libiscsi_sem_t *sem)
{
        dispatch_release(*sem);
        return 0;
}

int iscsi_mt_sem_post(libiscsi_sem_t *sem)
{
        dispatch_semaphore_signal(*sem);
        return 0;
}

int iscsi_mt_sem_wait(libiscsi_sem_t *sem)
{
        dispatch_semaphore_wait(*sem, DISPATCH_TIME_FOREVER);
        return 0;
}

int iscsi_mt_sem_timedwait(libiscsi_sem_t *sem, int ms)
{
        if (dispatch_semaphore_wait(*sem,
                dispatch_time(DISPATCH_TIME_NOW,
                              (int64_t)ms * NSEC_PER_MSEC)) != 0) {
                return -1;
        }
        return 0;
}

#else
int iscsi_mt_sem_init(libiscsi_sem_t *sem, int value)
{
        return sem_init(sem, 0, value);
}

int iscsi_mt_sem_destroy(libiscsi_sem_t *sem)
{
        return sem_destroy(sem);
}

int iscsi_mt_sem_post(libiscsi_sem_t *sem)
{
        return sem_post(sem);
}

int iscsi_mt_sem_wait(libiscsi_sem_t *sem)
{
        return sem_wait(sem);
}

int iscsi_mt_sem_timedwait(libiscsi_sem_t *sem, int ms)
{
        struct timespec ts;
        int ret;

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += ms / 1000;
        ts.tv_nsec += (ms % 1000) * 1000000L;
        if (ts.tv_nsec >= 1000000000L) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000L;
        }
        while ((ret = sem_timedwait(sem, &ts)) != 0 && errno == EINTR)
                ;
        return ret == 0 ? 0 : -1;
}
#endif

#endif /* HAVE_PTHREAD */

int iscsi_mt_is_service_thread(struct iscsi_context *iscsi)
{
        return iscsi->multithreading_enabled &&
                iscsi->service_tid == iscsi_mt_get_tid();
}

#endif /* HAVE_MULTITHREADING */

