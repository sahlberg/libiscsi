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
 * A command that makes the target drop the connection every time it is
 * sent, such as one larger than an iSER target can take, must not be
 * sent again forever by the reconnects. It is to fail once it has been
 * sent again ISCSI_MAX_COMMAND_RESENDS (3) times, and the session is to
 * stay usable.
 *
 * The program talks to the target through a small proxy that drops the
 * connection whenever it sees a READ16 of POISON_LBA, and counts how
 * many times it saw it. The test is run without and with the
 * multithreading service thread.
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

#define POISON_LBA 7
#define EXPECTED_SENDS 4        /* the first send, then 3 resends */

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:prog-resend-limit";

/*
 * Proxy
 */
static struct sockaddr_in target_addr;
static int listen_fd;
static int poison_seen;
static pthread_mutex_t proxy_mutex = PTHREAD_MUTEX_INITIALIZER;

static double now(void)
{
        struct timespec ts;

        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ts.tv_sec + ts.tv_nsec / 1e9;
}

static int get_poison_seen(void)
{
        int seen;

        pthread_mutex_lock(&proxy_mutex);
        seen = poison_seen;
        pthread_mutex_unlock(&proxy_mutex);
        return seen;
}

static void reset_poison_seen(void)
{
        pthread_mutex_lock(&proxy_mutex);
        poison_seen = 0;
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

/* Is @bhs a SCSI command PDU for a READ16 of POISON_LBA? */
static int is_poison(const unsigned char *bhs)
{
        const unsigned char *cdb = bhs + 32;
        uint64_t lba = 0;
        int i;

        if ((bhs[0] & 0x3f) != 0x01 || cdb[0] != 0x88) {
                return 0;
        }
        for (i = 2; i < 10; i++) {
                lba = (lba << 8) | cdb[i];
        }
        return lba == POISON_LBA;
}

static void *connection(void *arg)
{
        int cfd = (int)(intptr_t)arg;
        int sfd;
        unsigned char *buf;
        size_t len = 0, parsed = 0, size = 1024 * 1024;

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
                int drop = 0;

                /* parse whole PDUs from the client */
                while (len - parsed >= 48) {
                        unsigned char *bhs = buf + parsed;

                        if (is_poison(bhs)) {
                                pthread_mutex_lock(&proxy_mutex);
                                poison_seen++;
                                pthread_mutex_unlock(&proxy_mutex);
                                drop = 1;
                                break;
                        }
                        if (len - parsed < pdu_length(bhs)) {
                                break;
                        }
                        parsed += pdu_length(bhs);
                }
                /* forward what came before it */
                if (parsed > 0) {
                        if (send_all(sfd, buf, parsed) != 0) {
                                break;
                        }
                        memmove(buf, buf + parsed, len - parsed);
                        len -= parsed;
                        parsed = 0;
                }
                if (drop) {
                        break;
                }

                pfd[0].fd = cfd;
                pfd[0].events = POLLIN;
                pfd[1].fd = sfd;
                pfd[1].events = POLLIN;
                if (poll(pfd, 2, 100) < 0 && errno != EINTR) {
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
                int fd;

                fd = accept(listen_fd, NULL, NULL);
                if (fd < 0) {
                        continue;
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
        int one = 1;

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

/* A READ16 of @lba that is to succeed, allowing for a UNIT ATTENTION. */
static int good_read(struct iscsi_context *iscsi, int lun, uint64_t lba,
                     const char *name)
{
        int i;

        for (i = 0; i < 2; i++) {
                struct scsi_task *task;
                int status;

                task = iscsi_read16_sync(iscsi, lun, lba, 512, 512,
                                         0, 0, 0, 0, 0);
                if (task == NULL) {
                        fprintf(stderr, "%s: READ16 of LBA %d failed: %s\n",
                                name, (int)lba, iscsi_get_error(iscsi));
                        return -1;
                }
                status = task->status;
                if (status == SCSI_STATUS_CHECK_CONDITION &&
                    task->sense.key == SCSI_SENSE_UNIT_ATTENTION) {
                        scsi_free_scsi_task(task);
                        continue;
                }
                scsi_free_scsi_task(task);
                if (status != SCSI_STATUS_GOOD) {
                        fprintf(stderr, "%s: READ16 of LBA %d failed with "
                                "status %d: %s\n", name, (int)lba, status,
                                iscsi_get_error(iscsi));
                        return -1;
                }
                return 0;
        }
        fprintf(stderr, "%s: READ16 of LBA %d kept returning UNIT "
                "ATTENTION\n", name, (int)lba);
        return -1;
}

static int test_resend_limit(const char *portal, const char *target, int lun,
                             int mt)
{
        const char *name = mt ? "resend limit with service thread" :
                                "resend limit";
        struct iscsi_context *iscsi;
        struct scsi_task *task;
        double start, elapsed;
        int ret = 0, seen, status;

        reset_poison_seen();
        iscsi = login(portal, target, lun);
        if (mt && iscsi_mt_service_thread_start(iscsi)) {
                fprintf(stderr, "Failed to start service thread\n");
                exit(10);
        }

        if (good_read(iscsi, lun, 0, name)) {
                ret = -1;
        }

        start = now();
        task = iscsi_read16_sync(iscsi, lun, POISON_LBA, 512, 512,
                                 0, 0, 0, 0, 0);
        elapsed = now() - start;
        seen = get_poison_seen();
        status = task ? task->status : -1;
        printf("%s: the command was sent %d times and returned after "
               "%.1fs with status %d: %s\n", name, seen, elapsed, status,
               iscsi_get_error(iscsi));
        if (task != NULL) {
                scsi_free_scsi_task(task);
        }
        if (status == SCSI_STATUS_GOOD) {
                fprintf(stderr, "%s: the command did not fail\n", name);
                ret = -1;
        }
        if (seen != EXPECTED_SENDS) {
                fprintf(stderr, "%s: the command was sent %d times, "
                        "expected %d\n", name, seen, EXPECTED_SENDS);
                ret = -1;
        }

        /* the session must still work */
        if (good_read(iscsi, lun, 0, name)) {
                ret = -1;
        }
        if (get_poison_seen() != seen) {
                fprintf(stderr, "%s: the command was sent again after it "
                        "failed\n", name);
                ret = -1;
        }

        if (mt) {
                iscsi_mt_service_thread_stop(iscsi);
        }
        iscsi_logout_sync(iscsi);
        iscsi_destroy_context(iscsi);
        return ret;
}

static void on_alarm(int sig)
{
        (void)sig;
        fprintf(stderr, "Timed out, the command was sent again forever\n");
        _exit(10);
}

static void print_usage(void)
{
        fprintf(stderr, "Usage: prog_resend_limit [-i <initiator-name>] "
                "<iscsi-url>\n");
}

int main(int argc, char *argv[])
{
        struct iscsi_context *iscsi;
        struct iscsi_url *iscsi_url;
        char proxy_portal[64];
        int c, ret = 0;

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

        signal(SIGALRM, on_alarm);
        alarm(180);

        iscsi = iscsi_create_context(initiator);
        if (iscsi == NULL) {
                fprintf(stderr, "Failed to create context\n");
                exit(10);
        }
        iscsi_url = iscsi_parse_full_url(iscsi, argv[optind]);
        if (iscsi_url == NULL) {
                fprintf(stderr, "Failed to parse URL: %s\n",
                        iscsi_get_error(iscsi));
                exit(10);
        }
        if (start_proxy(iscsi_url->portal, proxy_portal,
                        sizeof(proxy_portal)) != 0) {
                exit(10);
        }

        if (test_resend_limit(proxy_portal, iscsi_url->target,
                              iscsi_url->lun, 0) != 0) {
                ret = 10;
        }
        if (test_resend_limit(proxy_portal, iscsi_url->target,
                              iscsi_url->lun, 1) != 0) {
                ret = 10;
        }

        iscsi_destroy_url(iscsi_url);
        iscsi_destroy_context(iscsi);
        return ret;
}
