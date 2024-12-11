/*
   Copyright (C) 2010 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

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
#ifdef HAVE_CONFIG_H
#include "config.h"

#endif

#if defined(_WIN32)
#include <winsock2.h>

#include "win32_compat.h"

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
WSADATA wsaData;
#endif

#ifdef HAVE_POLL_H
#include <poll.h>

#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>

#endif

#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>

#include "iscsi.h"
#include "scsi-lowlevel.h"

int showluns;
int useurls;
const char *target_iqn = NULL;
const char *initiator = "iqn.2007-10.com.github:sahlberg:libiscsi:iscsi-ls";

struct client_state {
  int finished;
  int status;
  int lun;
  int type;
  const char *username;
  const char *password;
};

void format_wwn_to_kernel_format(const uint8_t *designator, int length,
                                 char *formatted_wwn, int size) {
  if (length <= 0 || length > 64 || !designator || !formatted_wwn) {
    snprintf(formatted_wwn, size, "Invalid WWID");
    return;
  }

  if ((designator[0] >> 4) != 0x6) {
    snprintf(formatted_wwn, size, "Not NAA");
    return;
  }

  snprintf(formatted_wwn, size, "naa.");
  for (int i = 0; i < length && (i * 2 + 4) < size; i++) {
    sprintf(&formatted_wwn[4 + i * 2], "%02x", designator[i]);
  }
}

void event_loop(struct iscsi_context *iscsi, struct client_state *state) {
  struct pollfd pfd;

  while (state->finished == 0) {
    pfd.fd = iscsi_get_fd(iscsi);
    pfd.events = iscsi_which_events(iscsi);

    if (!pfd.events) {
      sleep(1);
      continue;
    }

    if (poll(&pfd, 1, -1) < 0) {
      fprintf(stderr, "Poll failed");
      exit(10);
    }
    if (iscsi_service(iscsi, pfd.revents) < 0) {
      fprintf(stderr, "iscsi_service failed with : %s\n",
              iscsi_get_error(iscsi));
      exit(10);
    }
  }
}

void print_lun_info(int indent_level, const char *format, ...) {
    va_list args;
    va_start(args, format);

    for (int i = 0; i < indent_level; i++) {
        printf(" ");
    }

    vprintf(format, args);
    va_end(args);
}

void show_lun(struct iscsi_context *iscsi, int lun) {
    struct scsi_task *task;
    struct scsi_inquiry_standard *inq;
    int type;
    long long size = 0;
    char error_message[256] = "";

    /* check we can talk to the lun */
    tur_try_again:
    if ((task = iscsi_testunitready_sync(iscsi, lun)) == NULL) {
        snprintf(error_message, sizeof(error_message), "testunitready failed: %s", iscsi_get_error(iscsi));
        goto output_error;
    }
    if (task->status == SCSI_STATUS_CHECK_CONDITION) {
        if (task->sense.key == SCSI_SENSE_UNIT_ATTENTION &&
            task->sense.ascq == SCSI_SENSE_ASCQ_BUS_RESET) {
            scsi_free_scsi_task(task);
            goto tur_try_again;
        }
    }

    if (task->status == SCSI_STATUS_CHECK_CONDITION &&
        task->sense.key == SCSI_SENSE_NOT_READY &&
        task->sense.ascq == SCSI_SENSE_ASCQ_MEDIUM_NOT_PRESENT) {
    } else if (task->status != SCSI_STATUS_GOOD) {
        snprintf(error_message, sizeof(error_message), "TESTUNITREADY failed: %s", iscsi_get_error(iscsi));
        scsi_free_scsi_task(task);
        goto output_error;
    }
    scsi_free_scsi_task(task);

    task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION, 64);
    if (task == NULL || task->status != SCSI_STATUS_GOOD) {
        snprintf(error_message, sizeof(error_message), "Failed to get device ID for LUN %d: %s", lun, iscsi_get_error(iscsi));
        scsi_free_scsi_task(task);
        goto output_error;
    }

    int full_size = scsi_datain_getfullsize(task);
    if (full_size > task->datain.size) {
        task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_DEVICE_IDENTIFICATION, full_size);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
            snprintf(error_message, sizeof(error_message), "Failed to get full device ID for LUN %d: %s", lun, iscsi_get_error(iscsi));
            scsi_free_scsi_task(task);
            goto output_error;
        }
    }

    struct scsi_inquiry_device_identification *inq_serial = scsi_datain_unmarshall(task);
    if (inq_serial == NULL) {
        snprintf(error_message, sizeof(error_message), "failed to unmarshall inquiry datain blob");
        scsi_free_scsi_task(task);
        goto output_error;
    }
    if (inq_serial->qualifier != SCSI_INQUIRY_PERIPHERAL_QUALIFIER_CONNECTED) {
        snprintf(error_message, sizeof(error_message), "error: multipath device not connected");
        scsi_free_scsi_task(task);
        goto output_error;
    }

    if (inq_serial->device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
        snprintf(error_message, sizeof(error_message), "error: multipath devices must be SBC");
        scsi_free_scsi_task(task);
        goto output_error;
    }

    char designator_text[129];
    memset(designator_text, 0, sizeof(designator_text));
    struct scsi_inquiry_device_designator *des;

    for (des = inq_serial->designators; des != NULL; des = des->next) {
        if (des->association != SCSI_ASSOCIATION_LOGICAL_UNIT) {
            continue;
        }

        if (des->designator_type != SCSI_DESIGNATOR_TYPE_NAA) {
            continue;
        }
        if (des->designator_length > 0 && des->designator_length <= 64) {
            format_wwn_to_kernel_format((const uint8_t *)des->designator,
                                        des->designator_length, designator_text,
                                        sizeof(designator_text));
        } else {
            snprintf(error_message, sizeof(error_message), "Designator length is invalid: %d", des->designator_length);
            scsi_free_scsi_task(task);
            goto output_error;
        }
    }
    scsi_free_scsi_task(task);

    /* check what type of lun we have */
    task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
    if (task == NULL || task->status != SCSI_STATUS_GOOD) {
        snprintf(error_message, sizeof(error_message), "failed to send inquiry command: %s", iscsi_get_error(iscsi));
        scsi_free_scsi_task(task);
        goto output_error;
    }
    inq = scsi_datain_unmarshall(task);
    if (inq == NULL) {
        snprintf(error_message, sizeof(error_message), "failed to unmarshall inquiry datain blob");
        scsi_free_scsi_task(task);
        goto output_error;
    }
    type = inq->device_type;
    scsi_free_scsi_task(task);

    if (type == SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
        struct scsi_readcapacity10 *rc10;

        task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
            snprintf(error_message, sizeof(error_message), "failed to send readcapacity command");
            scsi_free_scsi_task(task);
            goto output_error;
        }

        rc10 = scsi_datain_unmarshall(task);
        if (rc10 == NULL) {
            snprintf(error_message, sizeof(error_message), "failed to unmarshall readcapacity10 data");
            scsi_free_scsi_task(task);
            goto output_error;
        }

        size = rc10->block_size;
        size *= rc10->lba;

        scsi_free_scsi_task(task);
    }

    if (type == SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {
        print_lun_info(8, "{\n");
        print_lun_info(10, "\"LUN\": %d,\n", lun);
        print_lun_info(10, "\"WWID\": \"%s\",\n", designator_text);
        print_lun_info(10, "\"Size\": %lld\n", size);
        print_lun_info(8, "}");
    }

    return;

output_error:
    print_lun_info(8, "{\n");
    print_lun_info(10, "\"LUN\": %d,\n", lun);
    print_lun_info(10, "\"Errors\": \"%s\"\n", error_message);
    print_lun_info(8, "}");
}

void list_luns(struct client_state *clnt, const char *target,
               const char *portal) {
  struct iscsi_context *iscsi;
  struct scsi_task *task;
  struct scsi_reportluns_list *list;
  int full_report_size;
  int i;

  iscsi = iscsi_create_context(initiator);
  if (iscsi == NULL) {
    fprintf(stderr, "Failed to create context\n");
    exit(10);
  }

  iscsi_set_initiator_username_pwd(iscsi, clnt->username, clnt->password);
  if (iscsi_set_targetname(iscsi, target)) {
    fprintf(stderr, "Failed to set target name\n");
    exit(10);
  }
  iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
  iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);

  if (iscsi_full_connect_sync(iscsi, portal, -1) != 0) {
    fprintf(stderr, "list_luns: iscsi_connect failed. %s\n", iscsi_get_error(iscsi));
    exit(10);
  }

  task = iscsi_reportluns_sync(iscsi, 0, 16);
  if (task == NULL) {
    fprintf(stderr, "reportluns failed : %s\n", iscsi_get_error(iscsi));
    exit(10);
  }
  full_report_size = scsi_datain_getfullsize(task);
  if (full_report_size > task->datain.size) {
    scsi_free_scsi_task(task);
    task = iscsi_reportluns_sync(iscsi, 0, full_report_size);
    if (task == NULL) {
      fprintf(stderr, "reportluns failed : %s\n", iscsi_get_error(iscsi));
      exit(10);
    }
  }

  list = scsi_datain_unmarshall(task);
  if (list == NULL) {
    fprintf(stderr, "failed to unmarshall reportluns datain blob\n");
    exit(10);
  }

  for (i = 0; i < (int)list->num; i++) {
    if (i > 0) {
      printf(",\n");
    }
    show_lun(iscsi, list->luns[i]);
  }

  scsi_free_scsi_task(task);
  iscsi_destroy_context(iscsi);
}

void discoverylogout_cb(struct iscsi_context *iscsi, int status,
                        void *command_data, void *private_data) {
  struct client_state *state = (struct client_state *)private_data;

  if (status != 0) {
    fprintf(stderr, "Failed to logout from target. : %s\n",
            iscsi_get_error(iscsi));
    exit(10);
  }

  if (iscsi_disconnect(iscsi) != 0) {
    fprintf(stderr, "Failed to disconnect old socket\n");
    exit(10);
  }

  state->finished = 1;
}

void discovery_cb(struct iscsi_context *iscsi, int status, void *command_data, void *private_data) {
    struct iscsi_discovery_address *addr;
    int first_target = 1;

    if (status != 0) {
        fprintf(stderr, "{\"error\": \"Failed to do discovery on target: %s\"}\n", iscsi_get_error(iscsi));
        return;
    }

    printf("[\n");
    for (addr = command_data; addr; addr = addr->next) {
        // If target_iqn is not NULL, filter by target_iqn
        if (target_iqn != NULL && strcmp(addr->target_name, target_iqn) != 0) {
            continue;
        }

        struct iscsi_target_portal *portal = addr->portals;

        if (!first_target) {
            printf(",\n");
        }
        first_target = 0;

        printf("  {\n");
        printf("    \"Target\": \"%s\",\n", addr->target_name);
        printf("    \"Portals\":  [\n");

        int first_portal = 1;
        while (portal != NULL) {
            if (!first_portal) {
                printf(",\n");
            }
            first_portal = 0;

            char *comma_pos = strchr(portal->portal, ',');
            if (comma_pos != NULL) {
                *comma_pos = '\0';
            }

            printf("        \"%s\"", portal->portal);
            portal = portal->next;
        }
        printf("\n      ],\n");

        if (showluns) {
            printf("    \"LUNs\": [\n");
            list_luns(private_data, addr->target_name, addr->portals->portal);
            printf("\n    ]\n");
        } else {
            printf("    \"LUNs\": []\n");
        }

        printf("  }");
    }
    printf("\n]\n");

    if (iscsi_logout_async(iscsi, discoverylogout_cb, private_data) != 0) {
        fprintf(stderr, "Failed to logout from target: %s\n", iscsi_get_error(iscsi));
        return;
    }
}

void discoverylogin_cb(struct iscsi_context *iscsi, int status,
                       void *command_data, void *private_data) {
  if (status != 0) {
    fprintf(stderr, "Login failed. %s\n", iscsi_get_error(iscsi));
    exit(10);
  }

  if (iscsi_discovery_async(iscsi, discovery_cb, private_data) != 0) {
    fprintf(stderr, "failed to send discovery command : %s\n",
            iscsi_get_error(iscsi));
    exit(10);
  }
}

void discoveryconnect_cb(struct iscsi_context *iscsi, int status,
                         void *command_data, void *private_data) {
  if (status != 0) {
    fprintf(stderr, "discoveryconnect_cb: connection failed : %s\n",
            iscsi_get_error(iscsi));
    exit(10);
  }

  if (iscsi_login_async(iscsi, discoverylogin_cb, private_data) != 0) {
    fprintf(stderr, "iscsi_login_async failed : %s\n", iscsi_get_error(iscsi));
    exit(10);
  }
}

void print_usage(void) {
   fprintf(stderr,
           "Usage: iscsi-ls [-?|--help] [-d|--debug] "
           "[--usage] [-i|--initiator-name=iqn-name] "
           "[--target-iqn=iqn-name]\n"
           "\t\t[-s|--show-luns] <iscsi-portal-url>\n");
 }

 void print_help(void) {
   fprintf(stderr, "Usage: iscsi-ls [OPTION...] <iscsi-url>\n");
   fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
   fprintf(stderr, "  -d, --debug                       Print debug information\n");
   fprintf(stderr, "  -s, --show-luns                   Show the luns for each target\n");
   fprintf(stderr, "  -U, --url                         Output targets in URL format\n");
   fprintf(stderr, "                                    (does not work with -s)\n");
   fprintf(stderr, "  -T, --target-iqn=iqn-name         Specify a single target IQN\n");
   fprintf(stderr, "\n");
   fprintf(stderr, "Help options:\n");
   fprintf(stderr, "  -?, --help                        Show this help message\n");
   fprintf(stderr, "      --usage                       Display brief usage message\n");
 }

int main(int argc, char *argv[]) {
  struct iscsi_context *iscsi;
  struct iscsi_url *iscsi_url = NULL;
  struct client_state state;
  char *url = NULL;
  int c;
  int option_index;
  static int show_help = 0, show_usage = 0, debug = 0;

#ifdef _WIN32
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    printf("Failed to start Winsock2\n");
    exit(10);
  }
#endif

  static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"usage", no_argument, NULL, 'u'},
      {"debug", no_argument, NULL, 'd'},
      {"initiator-name", required_argument, NULL, 'i'},
      {"show-luns", no_argument, NULL, 's'},
      {"url", no_argument, NULL, 'U'},
      {"target-iqn", required_argument, NULL, 'T'}, // Исправлено здесь
      {0, 0, 0, 0}
  };

  while ((c = getopt_long(argc, argv, "h?udi:sUT:", long_options,
                          &option_index)) != -1) {
    switch (c) {
      case 'h':
      case '?':
        show_help = 1;
        break;
      case 'u':
        show_usage = 1;
        break;
      case 'd':
        debug = 1;
        break;
      case 'i':
        initiator = optarg;
        break;
      case 's':
        showluns = 1;
        break;
      case 'U':
        useurls = 1;
        break;
      case 'T':
        target_iqn = optarg;
        break;
      default:
        fprintf(stderr, "Unrecognized option '%c'\n\n", c);
        print_help();
        exit(0);
    }
  }

  if (show_help != 0) {
    print_help();
    exit(0);
  }

  if (show_usage != 0) {
    print_usage();
    exit(0);
  }

  memset(&state, 0, sizeof(state));

  if (argv[optind] != NULL) {
    url = strdup(argv[optind]);
  }
  if (url == NULL) {
    fprintf(stderr, "You must specify iscsi target portal.\n");
    print_usage();
    exit(10);
  }

  iscsi = iscsi_create_context(initiator);
  if (iscsi == NULL) {
    printf("Failed to create context\n");
    exit(10);
  }

  iscsi_set_session_type(iscsi, ISCSI_SESSION_DISCOVERY);
  iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE);

  if (debug > 0) {
    iscsi_set_log_level(iscsi, debug);
    iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
  }

  iscsi_url = iscsi_parse_portal_url(iscsi, url);

  free(url);

  if (iscsi_url == NULL) {
    fprintf(stderr, "Failed to parse URL: %s\n", iscsi_get_error(iscsi));
    exit(10);
  }

  state.username = iscsi_url->user;
  state.password = iscsi_url->passwd;

  if (iscsi_connect_async(iscsi, iscsi_url->portal, discoveryconnect_cb,
                          &state) != 0) {
    fprintf(stderr, "connect_async: iscsi_connect failed. %s\n",
            iscsi_get_error(iscsi));
    exit(10);
  }

  event_loop(iscsi, &state);

  iscsi_destroy_url(iscsi_url);
  iscsi_destroy_context(iscsi);
  return 0;
}
