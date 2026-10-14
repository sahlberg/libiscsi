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
 * Reconnect while the multithreading service thread is running: read
 * from the LUN with the synchronous API, make the target drop the
 * session half way through, then force a reconnect from the main thread,
 * and check that all the reads complete and that the service thread can
 * be stopped.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

#define NUM_READS 20

const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:prog-mt-reconnect";

static void on_alarm(int sig)
{
        (void)sig;
        fprintf(stderr, "Timed out, the reconnect did not complete\n");
        _exit(10);
}

static void print_usage(void)
{
        fprintf(stderr, "Usage: prog_mt_reconnect [-i <initiator-name>] "
                "<iscsi-url>\n");
}

int main(int argc, char *argv[])
{
        struct iscsi_context *iscsi;
        struct iscsi_url *iscsi_url;
        struct scsi_task *task;
        struct scsi_readcapacity16 *rc16;
        uint32_t block_size;
        int c, i;

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
        alarm(60);

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
        iscsi_set_targetname(iscsi, iscsi_url->target);
        iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
        iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
        if (iscsi_full_connect_sync(iscsi, iscsi_url->portal,
                                    iscsi_url->lun) != 0) {
                fprintf(stderr, "Login failed: %s\n", iscsi_get_error(iscsi));
                exit(10);
        }

        if (iscsi_mt_service_thread_start(iscsi)) {
                fprintf(stderr, "Failed to start service thread\n");
                exit(10);
        }

        task = iscsi_readcapacity16_sync(iscsi, iscsi_url->lun);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
                fprintf(stderr, "READCAPACITY16 failed: %s\n",
                        iscsi_get_error(iscsi));
                exit(10);
        }
        rc16 = scsi_datain_unmarshall(task);
        if (rc16 == NULL) {
                fprintf(stderr, "Failed to unmarshall READCAPACITY16\n");
                exit(10);
        }
        block_size = rc16->block_length;
        scsi_free_scsi_task(task);

        for (i = 0; i < NUM_READS; i++) {
                if (i == NUM_READS / 2) {
                        char buf[256];

                        printf("write garbage to the socket to trigger a "
                               "server disconnect\n");
                        memset(buf, 0, sizeof(buf));
                        if (write(iscsi_get_fd(iscsi), buf, sizeof(buf)) !=
                            (ssize_t)sizeof(buf)) {
                                fprintf(stderr, "write failed\n");
                                exit(10);
                        }
                }
                if (i == 3 * NUM_READS / 4) {
                        printf("force a reconnect from the main thread\n");
                        if (iscsi_force_reconnect_sync(iscsi) != 0) {
                                fprintf(stderr, "Reconnect failed: %s\n",
                                        iscsi_get_error(iscsi));
                                exit(10);
                        }
                }

                task = iscsi_read16_sync(iscsi, iscsi_url->lun, i,
                                         block_size, block_size,
                                         0, 0, 0, 0, 0);
                if (task == NULL) {
                        fprintf(stderr, "READ16 of LBA %d failed: %s\n", i,
                                iscsi_get_error(iscsi));
                        exit(10);
                }
                if (task->status == SCSI_STATUS_CHECK_CONDITION &&
                    task->sense.key == SCSI_SENSE_UNIT_ATTENTION) {
                        printf("Received UNIT_ATTENTION. Ignoring.\n");
                } else if (task->status != SCSI_STATUS_GOOD) {
                        fprintf(stderr, "READ16 of LBA %d failed: %s\n", i,
                                iscsi_get_error(iscsi));
                        exit(10);
                }
                printf("READ returned for LBA %d\n", i);
                scsi_free_scsi_task(task);
        }

        iscsi_mt_service_thread_stop(iscsi);
        printf("service thread stopped\n");

        iscsi_logout_sync(iscsi);
        iscsi_destroy_url(iscsi_url);
        iscsi_destroy_context(iscsi);
        return 0;
}
