/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
/*
   Copyright (C) 2026 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

/*
 * A synchronous command that times out must not leave anything behind
 * that still refers to it, since its state is on the stack of the
 * returned function and its buffers belong to the caller again.
 *
 * The program talks to the target through a small proxy that can hold
 * back or drop the connection when it sees a given SCSI command:
 *
 * 1. Drop the connection on a READ16, then hold the READ16 that the
 *    reconnect requeues past the timeout.
 * 2. Hold the DATA-OUT of a WRITE16 with small socket buffers, so that
 *    a DATA-OUT pdu is only partly written when the timeout expires.
 * 3. Like 1, with the multithreading service thread running.
 *
 * After each timeout the context is used for a while longer, which lets
 * the proxy release what it held and the target reply to it. Run under
 * AddressSanitizer with detect_stack_use_after_return=1 to catch
 * accesses to the stack or the buffers of the timed out call.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#define TIMEOUT 3

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:prog-sync-timeout";

/*
 * Proxy
 */
struct rule {
        int conn;               /* which connection, counting from 1 */
        uint8_t pdu_opcode;     /* the PDU to act on */
        uint8_t cdb_opcode;     /* and for a SCSI command, which one */
        double hold;            /* hold the PDU back for this long */
        int drop;               /* then drop the connection */
};

static struct sockaddr_in target_addr;
static int listen_fd;
static int small_buffers;
static struct rule rules[2];
static int num_rules;
static int num_conns;
static pthread_mutex_t proxy_mutex = PTHREAD_MUTEX_INITIALIZER;

static double now(void)
{
        struct timespec ts;

        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void set_rules(int small, int n, const struct rule *r)
{
        pthread_mutex_lock(&proxy_mutex);
        small_buffers = small;
        num_rules = n;
        memcpy(rules, r, n * sizeof(*r));
        num_conns = 0;
        pthread_mutex_unlock(&proxy_mutex);
}

static int send_all(int fd, const unsigned char *buf, size_t len)
{
        while (len > 0) {
                ssize_t n = send(fd, buf, len, MSG_NOSIGNAL);

                if (n < 0 && errno == EINTR) {
                        continue;
                }
                if (n <= 0) {
                        return -1;
                }
                buf += n;
                len -= n;
        }
        return 0;
}

/* Length of the iSCSI PDU that starts at @bhs, without digests. */
static size_t pdu_length(const unsigned char *bhs)
{
        size_t ahs = bhs[4] * 4;
        size_t data = (bhs[5] << 16) | (bhs[6] << 8) | bhs[7];

        return 48 + ahs + ((data + 3) & ~3);
}

static void *connection(void *arg)
{
        int cfd = (int)(intptr_t)arg;
        int sfd, conn, i;
        struct rule rule = { 0, 0, 0, 0, 0 };
        int have_rule = 0;
        unsigned char *buf;
        size_t len = 0, parsed = 0, size = 1024 * 1024;
        double release = 0;
        int holding = 0;

        pthread_mutex_lock(&proxy_mutex);
        conn = ++num_conns;
        for (i = 0; i < num_rules; i++) {
                if (rules[i].conn == conn) {
                        rule = rules[i];
                        have_rule = 1;
                }
        }
        pthread_mutex_unlock(&proxy_mutex);

        sfd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sfd, (struct sockaddr *)&target_addr,
                    sizeof(target_addr)) != 0) {
                close(cfd);
                close(sfd);
                return NULL;
        }
        buf = malloc(size);

        for (;;) {
                struct pollfd pfd[2];
                unsigned char tmp[4096];
                ssize_t n;
                int timeout = 100;

                if (holding && now() >= release) {
                        if (rule.drop) {
                                break;
                        }
                        holding = 0;
                        have_rule = 0;
                }

                /* forward the parsed client data that is not held back */
                if (!holding && parsed > 0) {
                        if (send_all(sfd, buf, parsed) != 0) {
                                break;
                        }
                        memmove(buf, buf + parsed, len - parsed);
                        len -= parsed;
                        parsed = 0;
                }

                /* parse whole PDUs from the client */
                while (!holding && len - parsed >= 48) {
                        unsigned char *bhs = buf + parsed;

                        if (have_rule &&
                            (bhs[0] & 0x3f) == rule.pdu_opcode &&
                            (rule.pdu_opcode != 0x01 ||
                             bhs[32] == rule.cdb_opcode)) {
                                holding = 1;
                                release = now() + rule.hold;
                                break;
                        }
                        if (len - parsed < pdu_length(bhs)) {
                                break;
                        }
                        parsed += pdu_length(bhs);
                }
                if (!holding && parsed > 0) {
                        continue;
                }

                /* while holding, stop reading from the client */
                pfd[0].fd = holding ? -1 : cfd;
                pfd[0].events = POLLIN;
                pfd[1].fd = sfd;
                pfd[1].events = POLLIN;
                if (poll(pfd, 2, timeout) < 0 && errno != EINTR) {
                        break;
                }
                if (pfd[0].revents & (POLLIN | POLLHUP | POLLERR)) {
                        n = recv(cfd, tmp, sizeof(tmp), 0);
                        if (n <= 0) {
                                break;
                        }
                        if (len + n > size) {
                                size = (len + n) * 2;
                                buf = realloc(buf, size);
                        }
                        memcpy(buf + len, tmp, n);
                        len += n;
                }
                if (pfd[1].revents & (POLLIN | POLLHUP | POLLERR)) {
                        n = recv(sfd, tmp, sizeof(tmp), 0);
                        if (n <= 0 || send_all(cfd, tmp, n) != 0) {
                                break;
                        }
                }
        }

        free(buf);
        close(cfd);
        close(sfd);
        return NULL;
}

static void *proxy(void *arg)
{
        (void)arg;

        for (;;) {
                pthread_t thread;
                int fd, small;

                fd = accept(listen_fd, NULL, NULL);
                if (fd < 0) {
                        continue;
                }
                pthread_mutex_lock(&proxy_mutex);
                small = small_buffers;
                pthread_mutex_unlock(&proxy_mutex);
                if (small) {
                        int size = 4096;

                        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size,
                                   sizeof(size));
                }
                pthread_create(&thread, NULL, connection,
                               (void *)(intptr_t)fd);
                pthread_detach(thread);
        }
        return NULL;
}

static int start_proxy(const char *portal, char *proxy_portal, size_t size)
{
        struct sockaddr_in sin;
        socklen_t sin_len = sizeof(sin);
        char host[64];
        const char *colon;
        pthread_t thread;
        int one = 1, small = 4096;

        colon = strrchr(portal, ':');
        memset(&target_addr, 0, sizeof(target_addr));
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(colon ? atoi(colon + 1) : 3260);
        snprintf(host, sizeof(host), "%.*s",
                 colon ? (int)(colon - portal) : (int)strlen(portal), portal);
        if (inet_pton(AF_INET, host, &target_addr.sin_addr) != 1) {
                fprintf(stderr, "The portal must be an IPv4 address\n");
                return -1;
        }

        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        /* accepted sockets inherit this, see small_buffers */
        setsockopt(listen_fd, SOL_SOCKET, SO_RCVBUF, &small, sizeof(small));
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listen_fd, (struct sockaddr *)&sin, sizeof(sin)) != 0 ||
            listen(listen_fd, 4) != 0 ||
            getsockname(listen_fd, (struct sockaddr *)&sin, &sin_len) != 0) {
                fprintf(stderr, "Failed to set up the proxy\n");
                return -1;
        }
        snprintf(proxy_portal, size, "127.0.0.1:%d", ntohs(sin.sin_port));

        pthread_create(&thread, NULL, proxy, NULL);
        pthread_detach(thread);
        return 0;
}

/*
 * Tests
 */
static struct iscsi_context *login(const char *portal, const char *target,
                                   int lun)
{
        struct iscsi_context *iscsi;

        iscsi = iscsi_create_context(initiator);
        if (iscsi == NULL) {
                fprintf(stderr, "Failed to create context\n");
                exit(10);
        }
        iscsi_set_targetname(iscsi, target);
        iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
        iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE);
        if (iscsi_full_connect_sync(iscsi, portal, lun) != 0) {
                fprintf(stderr, "Login failed: %s\n", iscsi_get_error(iscsi));
                exit(10);
        }
        return iscsi;
}

/* Keep using the context while the proxy lets go of what it held. */
static void keep_using(struct iscsi_context *iscsi, int lun, int seconds)
{
        double end = now() + seconds;

        while (now() < end) {
                struct scsi_task *task;

                task = iscsi_testunitready_sync(iscsi, lun);
                if (task != NULL) {
                        scsi_free_scsi_task(task);
                } else {
                        usleep(100000);
                }
        }
}

static int check_timeout(const char *name, double start,
                         struct scsi_task *task)
{
        double elapsed = now() - start;
        int status = task ? task->status : -1;

        printf("%s: returned after %.1fs with status %d\n", name, elapsed,
               status);
        if (task != NULL && task->status == SCSI_STATUS_GOOD) {
                fprintf(stderr, "%s: the command did not time out\n", name);
                return -1;
        }
        if (elapsed > TIMEOUT + 3) {
                fprintf(stderr, "%s: the timeout took %.1fs\n", name,
                        elapsed);
                return -1;
        }
        return 0;
}

static int test_requeue(const char *portal, const char *target, int lun,
                        int mt)
{
        const char *name = mt ? "requeue with service thread" : "requeue";
        struct rule r[2] = {
                /* make sure the requeue gets a later timeout */
                { 1, 0x01, 0x88, 1.5, 1 },
                { 2, 0x01, 0x88, 8, 0 },
        };
        struct iscsi_context *iscsi;
        struct scsi_task *task;
        double start;
        int ret;

        set_rules(0, 2, r);
        iscsi = login(portal, target, lun);
        iscsi_set_timeout(iscsi, TIMEOUT);
        if (mt && iscsi_mt_service_thread_start(iscsi)) {
                fprintf(stderr, "Failed to start service thread\n");
                exit(10);
        }

        start = now();
        task = iscsi_read16_sync(iscsi, lun, 0, 4096, 4096,
                                 0, 0, 0, 0, 0);
        ret = check_timeout(name, start, task);
        if (task != NULL) {
                scsi_free_scsi_task(task);
        }

        keep_using(iscsi, lun, 10);
        if (mt) {
                iscsi_mt_service_thread_stop(iscsi);
        }
        iscsi_destroy_context(iscsi);
        return ret;
}

static int test_partial_write(const char *portal, const char *target, int lun)
{
        const char *name = "partial write";
        struct rule r[1] = {
                /* the first DATA-OUT */
                { 1, 0x05, 0, 8, 0 },
        };
        struct iscsi_context *iscsi;
        struct scsi_task *task;
        unsigned char *data;
        int size = 4096, len = 256 * 1024;
        double start;
        int ret;

        set_rules(1, 1, r);
        iscsi = login(portal, target, lun);
        iscsi_set_timeout(iscsi, TIMEOUT);
        setsockopt(iscsi_get_fd(iscsi), SOL_SOCKET, SO_SNDBUF, &size,
                   sizeof(size));

        data = malloc(len);
        memset(data, 0xa5, len);
        start = now();
        task = iscsi_write16_sync(iscsi, lun, 0, data, len, 4096,
                                  0, 0, 0, 0, 0);
        ret = check_timeout(name, start, task);
        if (task != NULL) {
                scsi_free_scsi_task(task);
        }
        /* the buffer is ours again */
        memset(data, 0, len);
        free(data);

        keep_using(iscsi, lun, 10);
        iscsi_destroy_context(iscsi);
        return ret;
}

static void print_usage(void)
{
        fprintf(stderr, "Usage: prog_sync_timeout [-i <initiator-name>] "
                "<iscsi-url>\n");
}

int main(int argc, char *argv[])
{
        struct iscsi_context *iscsi;
        struct iscsi_url *iscsi_url;
        char portal[64];
        int c, failed = 0;

        while ((c = getopt(argc, argv, "i:")) != -1) {
                switch (c) {
                case 'i':
                        initiator = optarg;
                        break;
                default:
                        print_usage();
                        exit(10);
                }
        }
        if (optind != argc - 1) {
                print_usage();
                exit(10);
        }

        /* keep our output in order with the libiscsi log on stderr */
        setvbuf(stdout, NULL, _IOLBF, 0);
        signal(SIGPIPE, SIG_IGN);
        alarm(180);

        iscsi = iscsi_create_context(initiator);
        iscsi_url = iscsi_parse_full_url(iscsi, argv[optind]);
        if (iscsi_url == NULL) {
                fprintf(stderr, "Failed to parse URL: %s\n",
                        iscsi_get_error(iscsi));
                exit(10);
        }
        if (start_proxy(iscsi_url->portal, portal, sizeof(portal)) != 0) {
                exit(10);
        }

        failed |= test_requeue(portal, iscsi_url->target, iscsi_url->lun, 0);
        failed |= test_partial_write(portal, iscsi_url->target,
                                     iscsi_url->lun);
        failed |= test_requeue(portal, iscsi_url->target, iscsi_url->lun, 1);

        iscsi_destroy_url(iscsi_url);
        iscsi_destroy_context(iscsi);
        return failed ? 10 : 0;
}
