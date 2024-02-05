/* 
   Copyright (C) 2022 by zhenwei pi<pizhenwei@bytedance.com>

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

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <inttypes.h>
#include "iscsi.h"
#include "scsi-lowlevel.h"

static void pr_in_read_keys(struct scsi_persistent_reserve_in_read_keys *rk)
{
        int i;

        printf("  PR generation=0x%x, %d registered reservation keys follow:\n",
                        rk->prgeneration, rk->num_keys);
        for (i = 0; i < rk->num_keys; i++) {
                printf("    0x%" PRIx64 "\n", rk->keys[i]);
        }
}

static void pr_in_read_reservation(struct scsi_persistent_reserve_in_read_reservation *rr)
{
        printf("  PR generation=0x%x, Reservation follows:\n", rr->prgeneration);
        if (!rr->reserved) {
                return;
        }

        printf("    Key=0x%" PRIx64 "\n", rr->reservation_key);
        if (!rr->pr_scope) {
                printf("    scope: LU_SCOPE,  type: %s\n", scsi_pr_type_str(rr->pr_type));
        } else {
                printf("    scope: %d,  type: %s\n",
                                rr->pr_scope, scsi_pr_type_str(rr->pr_type));
        }
}

static void pr_in_report_capabilities(struct scsi_persistent_reserve_in_report_capabilities *rc)
{
        uint16_t mask = (rc->persistent_reservation_type_mask & SCSI_PR_TYPE_MASK_ALL);

        printf("Report capabilities response:\n");
        printf("  Compatible Reservation Handling(CRH): 0x%x\n", rc->crh);
        printf("  Specify Initiator Ports Capable(SIP_C): 0x%x\n", rc->sip_c);
        printf("  All Target Ports Capable(ATP_C): 0x%x\n", rc->atp_c);
        printf("  Persist Through Power Loss Capable(PTPL_C): 0x%x\n", rc->ptpl_c);
        printf("  Type Mask Valid(TMV): 0x%x\n", rc->tmv);
        printf("  Allow Commands: 0x%x\n", rc->allow_commands);
        printf("  Persist Through Power Loss Active(PTPL_A): 0x%x\n", rc->ptpl_a);
        printf("    Support indicated in Type mask:\n");
        if (mask & SCSI_PR_TYPE_MASK_EX_AC_AR) {
                printf("      Exclusive Access, All Registrants\n");
        }
        if (mask & SCSI_PR_TYPE_MASK_WR_EX) {
                printf("      Write Exclusive\n");
        }
        if (mask & SCSI_PR_TYPE_MASK_EX_AC) {
                printf("      Write Exclusive, Registrants Only\n");
        }
        if (mask & SCSI_PR_TYPE_MASK_WR_EX_RO) {
                printf("      Exclusive Access Registrants Only\n");
        }
        if (mask & SCSI_PR_TYPE_MASK_EX_AC_RO) {
                printf("      Write Exclusive, All Registrants\n");
        }
        if (mask & SCSI_PR_TYPE_MASK_WR_EX_AR) {
                printf("      Exclusive Access, All Registrants\n");
        }
}

static void do_pr_in(struct iscsi_context *iscsi, int lun, int sa)
{
        struct scsi_task *task;
        int full_size;
        void *blob;

        /* See how big this inquiry data is */
        task = iscsi_persistent_reserve_in_sync(iscsi, lun, sa, 1024);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
                fprintf(stderr, "PR IN command failed : %s\n", iscsi_get_error(iscsi));
                exit(-1);
        }

        full_size = scsi_datain_getfullsize(task);
        if (full_size > task->datain.size) {
                scsi_free_scsi_task(task);

                if ((task = iscsi_persistent_reserve_in_sync(iscsi, lun, sa, full_size)) == NULL) {
                        fprintf(stderr, "PR IN command failed : %s\n", iscsi_get_error(iscsi));
                        exit(-1);
                }
        }

        blob = scsi_datain_unmarshall(task);
        if (blob == NULL) {
                fprintf(stderr, "failed to unmarshall PR IN blob\n");
                exit(-1);
        }

        switch (sa) {
                case SCSI_PERSISTENT_RESERVE_READ_KEYS:
                        pr_in_read_keys(blob);
                        break;

                case SCSI_PERSISTENT_RESERVE_READ_RESERVATION:
                        pr_in_read_reservation(blob);
                        break;

                case SCSI_PERSISTENT_RESERVE_REPORT_CAPABILITIES:
                        pr_in_report_capabilities(blob);
                        break;

                default:
                        fprintf(stderr, "Usupported PR IN sa: 0x%02x\n", sa);
        }

        scsi_free_scsi_task(task);
}

static const char *pr_out_sa_str(int sa)
{
        switch (sa) {
                case SCSI_PERSISTENT_RESERVE_REGISTER:
                        return "register";
                case SCSI_PERSISTENT_RESERVE_RESERVE:
                        return "reserve";
                case SCSI_PERSISTENT_RESERVE_RELEASE:
                        return "release";
                case SCSI_PERSISTENT_RESERVE_CLEAR:
                        return "clear";
                case SCSI_PERSISTENT_RESERVE_PREEMPT:
                        return "preempt";
                case SCSI_PERSISTENT_RESERVE_PREEMPT_AND_ABORT:
                        return "preempt-and-abort";
                case SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY:
                        return "register-and-ignore-existing-key";
                case SCSI_PERSISTENT_RESERVE_REGISTER_AND_MOVE:
                        return "register-and-move";
        }

        return "Unknown SA";
}

static int do_pr_out_command(struct iscsi_context *iscsi, int lun, int sa, int type, uint64_t rk, uint64_t sark)
{
        struct scsi_task *task;
        struct scsi_persistent_reserve_out_basic param = {0};

        /* TODO other field in param */
        param.reservation_key = rk;
        param.service_action_reservation_key = sark;

        task = iscsi_persistent_reserve_out_sync(iscsi, lun, sa, 0, type, &param);
        if (task == NULL || task->status != SCSI_STATUS_GOOD) {
                fprintf(stderr, "PR OUT command [%s] failed : %s\n",
                                pr_out_sa_str(sa), iscsi_get_error(iscsi));
                return -1;
        }

        scsi_free_scsi_task(task);

        return 0;
}

static void do_pr_out(struct iscsi_context *iscsi, int lun, int sa, int type, uint64_t rk, uint64_t sark, unsigned char remove)
{
        /* always do register */
        if ((do_pr_out_command(iscsi, lun, SCSI_PERSISTENT_RESERVE_REGISTER, type, 0, sark) < 0)
                        || sa == SCSI_PERSISTENT_RESERVE_REGISTER) {
                return;
        }

        /* clear all keys */
        if (sa == SCSI_PERSISTENT_RESERVE_CLEAR) {
                rk = sark;
                do_pr_out_command(iscsi, lun, sa, type, rk, sark);
                return;
        }

        if (sa == SCSI_PERSISTENT_RESERVE_RESERVE) {
                rk = sark;
                if (do_pr_out_command(iscsi, lun, sa, type, rk, sark)) {
                        goto remove_self_rk;
                } else {
                        return;
                }
        }

        if (sa == SCSI_PERSISTENT_RESERVE_PREEMPT) {
                if (do_pr_out_command(iscsi, lun, sa, type, sark, rk)) {
                        goto remove_self_rk;
                }

                if (!remove) {
                        return;
                } /* otherwise try to release */
        }

        if (sa == SCSI_PERSISTENT_RESERVE_RELEASE) {
                rk = sark;
                do_pr_out_command(iscsi, lun, sa, type, rk, sark);
                goto remove_self_rk;
        }

remove_self_rk:
        do_pr_out_command(iscsi, lun, SCSI_PERSISTENT_RESERVE_REGISTER, type, sark, 0);
}

static void print_help(const char *bin)
{
        fprintf(stderr, "\n");
        fprintf(stderr, "Usage: %s [OPTION...] <iscsi-url>\n", bin);
        fprintf(stderr, "  -i, --initiator-name=iqn-name  Initiator name to use\n");
        fprintf(stderr, "  -d, --debug                    Enable debug\n");
        fprintf(stderr, "  -?, --help                     Show this help message\n");

        fprintf(stderr, "  -K, --param-rk=RK              PR Out: parameter reservation key (RK is in hex)\n");
        fprintf(stderr, "  -S, --param-sark=SARK          PR Out: parameter service action reservation key (SARK is in hex)\n");
        fprintf(stderr, "  -T, --prout-type=TYPE          PR Out: type field\n");
        fprintf(stderr, "  -G, --register                 PR Out: Register\n");
        fprintf(stderr, "  -R, --reserve                  PR Out: Reserve, SARK only(register sark implicitly)\n");
        fprintf(stderr, "  -L, --release                  PR Out: Release, SARK only. This program releases TYPE 7 and TYPE 8 only\n");
        fprintf(stderr, "  -C, --clear                    PR Out: Clear, SARK only\n");
        fprintf(stderr, "  -P, --preempt                  PR Out: Preempt, use SARK to preempt reservation from RK\n");
        fprintf(stderr, "  -X, --preempt-remove           PR Out: Preempt, use SARK to preempt reservation from RK, then release SARK and remove SARK\n");
#if 0
        fprintf(stderr, "  -A, --preempt-abort            PR Out: Preempt and Abort\n");
        fprintf(stderr, "  -I, --register-ignore          PR Out: Register and Ignore\n");
        fprintf(stderr, "  -M, --register-move            PR Out: Register and Move\n");
#endif
        fprintf(stderr, "  -k, --read-keys                PR In:  Read Keys (default)\n");
        fprintf(stderr, "  -r, --read-reservation         PR In:  Read Reservation\n");
        fprintf(stderr, "  -c, --report-capabilities      PR In:  Report Capabilities\n");
        fprintf(stderr, "  -s, --read-full-status         PR In:  Read Full Status\n");
        fprintf(stderr, "\n");
        fprintf(stderr, "iSCSI URL format : %s\n", ISCSI_URL_SYNTAX);
        fprintf(stderr, "<host> is either of:\n"
                        "  \"hostname\"       iscsi.example\n"
                        "  \"ipv4-address\"   10.1.1.27\n"
                        "  \"ipv6-address\"   [fce0::1]\n\n");

        fprintf(stderr, "PR Out TYPE field value meanings:\n"
                        "  0:    obsolete (was 'read shared' in SPC)\n"
                        "  1:    write exclusive\n"
                        "  2:    obsolete (was 'read exclusive')\n"
                        "  3:    exclusive access\n"
                        "  4:    obsolete (was 'shared access')\n"
                        "  5:    write exclusive, registrants only\n"
                        "  6:    exclusive access, registrants only\n"
                        "  7:    write exclusive, all registrants\n"
                        "  8:    exclusive access, all registrants\n");
}

int main(int argc, char *argv[])
{
        struct iscsi_context *iscsi;
        struct iscsi_url *iscsi_url;
        char *initiator = NULL;
        char *url = NULL;
        int debug = 0;
        int c, opt;
        int ret = -1;
        int pr_operations = 0;
        unsigned char pr_out = 1, remove = 0;
        int sa = -1;
        int type = 0;
        uint64_t rk = 0, sark = 0;

        /* always keep aligned with sg_persist command */
        static struct option long_options[] = {
                {"help",                no_argument,       NULL, 'h'},
                {"debug",               no_argument,       NULL, 'd'},
                {"initiator-name",      required_argument, NULL, 'i'},
                {"param-rk",            required_argument, NULL, 'K'},
                {"param-sark",          required_argument, NULL, 'S'},
                {"prout-type",          required_argument, NULL, 'T'},
                /* PR out, order by sa enum from spec */
                {"register",            no_argument,       NULL, 'G'},
                {"reserve",             no_argument,       NULL, 'R'},
                {"release",             no_argument,       NULL, 'L'},
                {"clear",               no_argument,       NULL, 'C'},
                {"preempt",             no_argument,       NULL, 'P'},
                {"preempt-remove",      no_argument,       NULL, 'X'},
#if 0
                {"preempt-abort",       no_argument,       NULL, 'A'},
                {"register-ignore",     no_argument,       NULL, 'I'},
                {"register-move",       no_argument,       NULL, 'M'},
#endif
                /* PR in, order by sa enum from spec */
                {"read-keys",           no_argument,       NULL, 'k'},
                {"read-reservation",    no_argument,       NULL, 'r'},
                {"report-capabilities", no_argument,       NULL, 'c'},
                {"read-full-status",    no_argument,       NULL, 's'},
                {0, 0, 0, 0}
        };

        while ((c = getopt_long(argc, argv, "hdHi:K:S:T:GRLCPXAIMkrcs", long_options, &opt)) != -1) {
                switch (c) {
                        case 'd':
                                debug = 1;
                                break;

                        case 'i':
                                initiator = optarg;
                                break;

                                /* PR out */
                        case 'K':
                                if (1 != sscanf(optarg, "%" SCNx64 "", &rk)) {
                                        fprintf(stderr, "bad argument to '--param-rk'\n");
                                        return -1;
                                }
                                break;

                        case 'S':
                                if (1 != sscanf(optarg, "%" SCNx64 "", &sark)) {
                                        fprintf(stderr, "bad argument to '--param-sark'\n");
                                        return -1;
                                }
                                break;

                        case 'T':
                                type = strtol(optarg, NULL, 0);
                                break;

                        case 'G':
                                sa = SCSI_PERSISTENT_RESERVE_REGISTER;
                                pr_operations++;
                                break;

                        case 'R':
                                sa = SCSI_PERSISTENT_RESERVE_RESERVE;
                                pr_operations++;
                                break;

                        case 'L':
                                sa = SCSI_PERSISTENT_RESERVE_RELEASE;
                                pr_operations++;
                                break;

                        case 'C':
                                sa = SCSI_PERSISTENT_RESERVE_CLEAR;
                                pr_operations++;
                                break;

                        case 'P':
                                sa = SCSI_PERSISTENT_RESERVE_PREEMPT;
                                pr_operations++;
                                break;

                        case 'X':
                                remove = 1;
                                sa = SCSI_PERSISTENT_RESERVE_PREEMPT;
                                pr_operations++;
                                break;

#if 0
                        case 'A':
                                sa = SCSI_PERSISTENT_RESERVE_PREEMPT_AND_ABORT;
                                pr_operations++;
                                break;

                        case 'I':
                                sa = SCSI_PERSISTENT_RESERVE_REGISTER_AND_IGNORE_EXISTING_KEY;
                                pr_operations++;
                                break;

                        case 'M':
                                sa = SCSI_PERSISTENT_RESERVE_REGISTER_AND_MOVE;
                                pr_operations++;
                                break;
#endif

                                /* PR in */
                        case 'k':
                                pr_out = 0;
                                sa = SCSI_PERSISTENT_RESERVE_READ_KEYS;
                                pr_operations++;
                                break;

                        case 'r':
                                pr_out = 0;
                                sa = SCSI_PERSISTENT_RESERVE_READ_RESERVATION;
                                pr_operations++;
                                break;

                        case 'c':
                                pr_out = 0;
                                sa = SCSI_PERSISTENT_RESERVE_REPORT_CAPABILITIES;
                                pr_operations++;
                                break;

                        case 's':
                                pr_out = 0;
                                sa = SCSI_PERSISTENT_RESERVE_READ_FULL_STATUS;
                                pr_operations++;
                                break;

                        case 'h':
                        case '?':
                        default:
                                print_help(argv[0]);
                                exit(0);
                }
        }

        if ((pr_operations != 1) || (sa == -1)) {
                fprintf(stderr, "You must specify one Persistent Reservation operation\n");
                print_help(argv[0]);
                exit(-1);
        }

        if (initiator == NULL) {
                fprintf(stderr, "You must specify initiator by -i/--initiator-name\n");
                exit(-1);
        }

        iscsi = iscsi_create_context(initiator);
        if (iscsi == NULL) {
                fprintf(stderr, "Failed to create context\n");
                exit(-1);
        }

        if (debug > 0) {
                iscsi_set_log_level(iscsi, debug);
                iscsi_set_log_fn(iscsi, iscsi_log_to_stderr);
        }

        if (argv[optind] != NULL) {
                url = argv[optind];
        } else {
                fprintf(stderr, "You must specify the URL\n");
                print_help(argv[0]);
                return -1;
        }

        iscsi_url = iscsi_parse_full_url(iscsi, url);
        if (iscsi_url == NULL) {
                fprintf(stderr, "Failed to parse URL: %s\n", iscsi_get_error(iscsi));
                goto destroy_context;
        }

        iscsi_set_session_type(iscsi, ISCSI_SESSION_NORMAL);
        iscsi_set_header_digest(iscsi, ISCSI_HEADER_DIGEST_NONE_CRC32C);
        if (iscsi_full_connect_sync(iscsi, iscsi_url->portal, iscsi_url->lun) != 0) {
                fprintf(stderr, "Login Failed. %s\n", iscsi_get_error(iscsi));
                goto destroy_url;
        }

        if (pr_out) {
                do_pr_out(iscsi, iscsi_url->lun, sa, type, rk, sark, remove);
        } else {
                do_pr_in(iscsi, iscsi_url->lun, sa);
        }

        /* finish successfully */
        ret = 0;
        iscsi_logout_sync(iscsi);

destroy_url:
        iscsi_destroy_url(iscsi_url);

destroy_context:
        iscsi_destroy_context(iscsi);

        return ret;
}
