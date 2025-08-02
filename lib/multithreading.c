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
#include "win32/win32_compat.h"
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

#include <pthread.h>
#include <string.h>
#include "iscsi.h"
#include "iscsi-private.h"

#ifdef HAVE_MULTITHREADING

#ifdef WIN32
iscsi_tid_t iscsi_mt_get_tid(void)
{
    return GetCurrentThreadId();
}
static void* iscsi_mt_service_thread(void* arg)
{
    struct iscsi_context* iscsi = (struct iscsi_context*)arg;
    struct pollfd pfd;
    int revents;
    int ret;

    iscsi->multithreading_enabled = 1;

    while (iscsi->multithreading_enabled) {
        pfd.fd = iscsi_get_fd(iscsi);
        pfd.events = iscsi_which_events(iscsi);
        pfd.revents = 0;

        ret = poll(&pfd, 1, 0);
        if (ret < 0) {
            iscsi_set_error(iscsi, "Poll failed");
            revents = -1;
        }
        else {
            revents = pfd.revents;
        }
        if (iscsi_service(iscsi, revents) < 0) {
            if (revents != -1)
                iscsi_set_error(iscsi, "iscsi_service failed");
        }
    }
    return NULL;
}

static DWORD WINAPI service_thread_init(LPVOID lpParam)
{
    HANDLE hStdout;
    struct iscsi_context* iscsi;

    hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdout == INVALID_HANDLE_VALUE) {
        return 1;
    }
    iscsi = (struct iscsi_context *)lpParam;
    iscsi_mt_service_thread(iscsi);
    return 0;
}

int iscsi_mt_service_thread_start(struct iscsi_context* iscsi)
{
    iscsi->iscsii->service_thread = CreateThread(NULL, 1024*1024, service_thread_init, iscsi, 0, NULL);
    if (iscsi->iscsii->service_thread == NULL) {
        iscsi_set_error(iscsi, "Failed to start service thread");
        return -1;
    }
    while (iscsi->multithreading_enabled == 0) {
        Sleep(100);
    }
    return 0;
}

void iscsi_mt_service_thread_stop(struct iscsi_context* iscsi)
{
    iscsi->multithreading_enabled = 0;
    while (WaitForSingleObject(iscsi->iscsii->service_thread, INFINITE) != WAIT_OBJECT_0);
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

#elif defined(HAVE_PTHREAD) /* WIN32 */

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/syscall.h>

iscsi_tid_t iscsi_mt_get_tid(void)
{
#ifdef HAVE_PTHREAD_THREADID_NP || defined(__APPLE__) && defined(__MACH__)
        iscsi_tid_t tid;
        pthread_threadid_np(NULL, &tid);
        return tid;
#elif defined(SYS_gettid)
        pid_t tid = syscall(SYS_gettid);
#endif
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
#endif

#endif /* HAVE_PTHREAD */

#endif /* HAVE_MULTITHREADING */

