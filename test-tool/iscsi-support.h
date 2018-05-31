/*
   iscsi-test tool support

   Copyright (C) 2012 by Lee Duncan <leeman.duncan@gmail.com>
   Copyright (C) 2014 by Ronnie sahlberg <ronniesahlberg@gmail.com>

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

#ifndef        _ISCSI_SUPPORT_H_
#define        _ISCSI_SUPPORT_H_

#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

extern const char *initiatorname1;
extern const char *initiatorname2;

#define EXPECT_STATUS_GOOD SCSI_STATUS_GOOD, SCSI_SENSE_NO_SENSE, NULL, 0
#define EXPECT_STATUS_GENERIC_BAD SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, NULL, 0
#define EXPECT_STATUS_TIMEOUT SCSI_STATUS_TIMEOUT, SCSI_SENSE_NO_SENSE, NULL, 0
#define EXPECT_NO_MEDIUM SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_NOT_READY, no_medium_ascqs, 3
#define EXPECT_LBA_OOB SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, lba_oob_ascqs, 1
#define EXPECT_INVALID_FIELD_IN_CDB SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, invalid_cdb_ascqs,2 
#define EXPECT_PARAM_LIST_LEN_ERR SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, param_list_len_err_ascqs, 1
#define EXPECT_TOO_MANY_DESCR SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, too_many_desc_ascqs, 2
#define EXPECT_UNSUPP_DESCR_CODE SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, unsupp_desc_code_ascqs, 2
#define EXPECT_MISCOMPARE SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_MISCOMPARE, miscompare_ascqs, 1
#define EXPECT_WRITE_PROTECTED SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_DATA_PROTECTION, write_protect_ascqs, 3
#define EXPECT_SANITIZE SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_NOT_READY, sanitize_ascqs, 1
#define EXPECT_REMOVAL_PREVENTED SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_ILLEGAL_REQUEST, removal_ascqs, 1
#define EXPECT_RESERVATION_CONFLICT SCSI_STATUS_RESERVATION_CONFLICT, 0, NULL, 0
#define EXPECT_COPY_ABORTED SCSI_STATUS_CHECK_CONDITION, SCSI_SENSE_COPY_ABORTED, copy_aborted_ascqs, 3

int no_medium_ascqs[3];
int lba_oob_ascqs[1];
int invalid_cdb_ascqs[2];
int param_list_len_err_ascqs[1];
int too_many_desc_ascqs[2];
int unsupp_desc_code_ascqs[2];
int write_protect_ascqs[3];
int sanitize_ascqs[1];
int removal_ascqs[1];
int miscompare_ascqs[1];
int copy_aborted_ascqs[3];

extern int loglevel;
#define LOG_SILENT  0
#define LOG_NORMAL  1
#define LOG_VERBOSE 2
void logging(int level, const char *format, ...) _R_(2,3);

/*
 * define special flags for logging a blank line, so compiler
 * does not commplain when logging a ""
 */
#define        LOG_BLANK_LINE " "
#define LOG_BLANK_LINE_CMP_LEN 2

#define CHECK_FOR_DATALOSS                                                \
do {                                                                        \
        if (!data_loss) {                                                \
                logging(LOG_NORMAL, "[SKIPPED] --dataloss flag is not " \
                                "set. Skipping test.");                                     \
                CU_PASS("[SKIPPED] --dataloss flag is not set."                \
                        " Skipping test");                                \
                return;                                                        \
        }                                                                \
} while (0);

#define CHECK_FOR_SANITIZE                                                \
do {                                                                        \
        if (!allow_sanitize) {                                                \
                logging(LOG_NORMAL, "[SKIPPED] --allow-sanitize flag " \
                        "is not set. Skipping test.");                                     \
                CU_PASS("[SKIPPED] --allow-sanitize flag is not set."        \
                        " Skipping test");                                \
                return;                                                        \
        }                                                                \
} while (0);

#define CHECK_FOR_READONLY                                                \
do {                                                                        \
        if (!readonly) {                                                \
                logging(LOG_NORMAL, "[SKIPPED] Logical unit is not "        \
                                "write-protected. Skipping test.");        \
                CU_PASS("[SKIPPED] Logical unit is not write-"                \
                                   "protected. Skipping test");                \
                return;                                                        \
        }                                                                \
} while (0);

#define CHECK_FOR_REMOVABLE                                                \
do {                                                                        \
        if (!inq->rmb) {                                                \
                logging(LOG_NORMAL, "[SKIPPED] Logical unit is not "        \
                        "removable. Skipping test.");                        \
                CU_PASS("[SKIPPED] Logical unit is not removable"        \
                        " Skipping test");                                \
                return;                                                        \
        }                                                                \
} while (0);

#define CHECK_FOR_THIN_PROVISIONING                                        \
do {                                                                        \
        if (rc16 == NULL || rc16->lbpme == 0) {                                              \
                logging(LOG_NORMAL, "[SKIPPED] Logical unit is fully"        \
                        " provisioned. Skipping test");                        \
                CU_PASS("[SKIPPED] Logical unit is fully provisioned."        \
                        " Skipping test");                                \
                return;                                                             \
        }                                                                \
} while (0);

#define CHECK_FOR_LBPWS10                                                \
do {                                                                        \
        if (inq_lbp->lbpws10 == 0) {                                               \
                logging(LOG_NORMAL, "[SKIPPED] Logical unit does not"        \
                        " have LBPWS10. Skipping test");                \
                CU_PASS("[SKIPPED] Logical unit does not have LBPWS10."        \
                        " Skipping test");                                \
                return;                                                             \
        }                                                                \
} while (0);

#define CHECK_FOR_LBPWS                                                        \
do {                                                                        \
        if (inq_lbp->lbpws == 0) {                                               \
                logging(LOG_NORMAL, "[SKIPPED] Logical unit does not"        \
                        " have LBPWS. Skipping test");                               \
                CU_PASS("[SKIPPED] Logical unit does not have LBPWS."        \
                        " Skipping test");                                \
                return;                                                             \
        }                                                                \
} while (0);

#define CHECK_FOR_LBPU                                                        \
do {                                                                        \
        if (inq_lbp->lbpu == 0) {                                               \
                logging(LOG_NORMAL, "[SKIPPED] Logical unit does not"        \
                        " have LBPU. Skipping test");                               \
                CU_PASS("[SKIPPED] Logical unit does not have LBPU."        \
                        " Skipping test");                                \
                return;                                                             \
        }                                                                \
} while (0);

#define CHECK_FOR_LBPPB_GT_1                                                \
do {                                                                        \
        if (lbppb < 2) {                                                \
          logging(LOG_NORMAL, "[SKIPPED] LBPPB < 2. Skipping test");        \
                CU_PASS("[SKIPPED] LBPPB < 2. Skipping test");                \
                return;                                                             \
        }                                                                \
} while (0);

#define CHECK_FOR_SBC                                                        \
do {                                                                        \
        if (inq->device_type != SCSI_INQUIRY_PERIPHERAL_DEVICE_TYPE_DIRECT_ACCESS) {\
                logging(LOG_NORMAL, "[SKIPPED] Not SBC device."        \
                        " Skipping test");                                \
                CU_PASS("[SKIPPED] Not SBC device."                        \
                        " Skipping test");                                \
                return;                                                             \
        }                                                                \
} while (0);

#define CHECK_SIZE(_min_blocks, _c)                                     \
        do {                                                            \
                if (num_blocks < _min_blocks) {                         \
                        logging(LOG_VERBOSE, "[SKIPPED] "               \
                                "LUN too small. Skipping assert");      \
                } else {                                                \
                        _c                                              \
                }                                                       \
        } while (0);

#define COMPAREANDWRITE(...)                                            \
        do {                                                            \
                int _r;                                                 \
                _r = compareandwrite(__VA_ARGS__);                      \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] COMPAREANDWRITE " \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "COMPAREANDWRITE. Skipping test");      \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define EXTENDEDCOPY(...)                                               \
        do {                                                            \
                int _r;                                                 \
                _r = extendedcopy(__VA_ARGS__);                         \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] EXTENDEDCOPY "   \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "EXTENDEDCOPY. Skipping test");         \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define GETLBASTATUS(...)                                               \
        do {                                                            \
                int _r;                                                 \
                _r = get_lba_status(__VA_ARGS__);                       \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] GETLBASTATUS "   \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "GETLBASTATUS. Skipping test");         \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define MODESENSE6(...)                                                 \
        do {                                                            \
                int _r;                                                 \
                _r = modesense6(__VA_ARGS__);                           \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] MODESENSE6 "     \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "MODESENSE6. Skipping test");           \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define ORWRITE(...)                                                    \
        do {                                                            \
                int _r;                                                 \
                _r = orwrite(__VA_ARGS__);                              \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] ORWRITE "        \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "ORWRITE. Skipping test");              \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define PREFETCH10(...)                                                 \
        do {                                                            \
                int _r;                                                 \
                _r = prefetch10(__VA_ARGS__);                           \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] PREFETCH10 "     \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "PREFETCH10. Skipping test");           \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define PREFETCH16(...)                                                 \
        do {                                                            \
                int _r;                                                 \
                _r = prefetch16(__VA_ARGS__);                           \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] PREFETCH16 "     \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "PREFETCH16. Skipping test");           \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define PREVENTALLOW(...)                                               \
        do {                                                            \
                int _r;                                                 \
                _r = preventallow(__VA_ARGS__);                         \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] PREVENTALLOW "   \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "PREVENTALLOW. Skipping test");         \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READ6(...)                                                      \
        do {                                                            \
                int _r;                                                 \
                _r = read6(__VA_ARGS__);                                \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READ6 "          \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READ6. Skipping test");                \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READ10(...)                                                     \
        ({								\
                int _r;                                                 \
                _r = read10(__VA_ARGS__);                               \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READ10 "         \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READ10. Skipping test");               \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
		_r;							\
        })

#define READ12(...)                                                     \
        do {                                                            \
                int _r;                                                 \
                _r = read12(__VA_ARGS__);                               \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READ12 "         \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READ12. Skipping test");               \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READ16(...)                                                     \
        do {                                                            \
                int _r;                                                 \
                _r = read16(__VA_ARGS__);                               \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READ16 "         \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READ16. Skipping test");               \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READCAPACITY10(...)                                             \
        do {                                                            \
                int _r;                                                 \
                _r = readcapacity10(__VA_ARGS__);                       \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READCAPACITY10 " \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READCAPACITY10. Skipping test");       \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READCAPACITY16(...)                                             \
        do {                                                            \
                int _r;                                                 \
                _r = readcapacity16(__VA_ARGS__);                       \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READCAPACITY16 " \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READCAPACITY16. Skipping test");       \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READDEFECTDATA10(...)                                           \
        do {                                                            \
                int _r;                                                 \
                _r = readdefectdata10(__VA_ARGS__);                     \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READDEFECTDATA10 " \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READDEFECTDATA10. Skipping test");     \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define READDEFECTDATA12(...)                                           \
        do {                                                            \
                int _r;                                                 \
                _r = readdefectdata12(__VA_ARGS__);                     \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] READDEFECTDATA12 " \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "READDEFECTDATA12. Skipping test");     \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define RECEIVE_COPY_RESULTS(...)                                       \
        ({								\
                int _r;                                                 \
                _r = receive_copy_results(__VA_ARGS__);                 \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] RECEIVE_COPY"    \
                                "_RESULTS is not implemented.");        \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "RECEIVE_COPY_RESULTS. Skipping test"); \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
		_r;							\
        })

#define RELEASE6(...)                                                   \
        do {                                                            \
                int _r;                                                 \
                _r = release6(__VA_ARGS__);                             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] RELEASE6 "       \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "RELEASE6. Skipping test");             \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define REPORT_SUPPORTED_OPCODES(...)                                   \
        do {                                                            \
                int _r;                                                 \
                _r = report_supported_opcodes(__VA_ARGS__);             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] REPORT_SUPPORTED" \
                                "_OPCODES is not implemented.");        \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "REPORT_SUPPORTED_OPCODES. Skipping "   \
                                "test");                                \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define RESERVE6(...)                                                   \
        do {                                                            \
                int _r;                                                 \
                _r = reserve6(__VA_ARGS__);                             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] RESERVE6 "       \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "RESERVE6. Skipping test");             \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define SANITIZE(...)                                                   \
        do {                                                            \
                int _r;                                                 \
                _r = sanitize(__VA_ARGS__);                             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] SANITIZE "       \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "SANITIZE. Skipping test");             \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define STARTSTOPUNIT(...)                                              \
        do {                                                            \
                int _r;                                                 \
                _r = startstopunit(__VA_ARGS__);                        \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] STARTSTOPUNIT "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "STARTSTOPUNIT. Skipping test");        \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define SYNCHRONIZECACHE10(...)                                         \
        do {                                                            \
                int _r;                                                 \
                _r = synchronizecache10(__VA_ARGS__);                   \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] SYNCHRONIZECACHE10 "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "SYNCHRONIZECACHE10. Skipping test");   \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define SYNCHRONIZECACHE16(...)                                         \
        do {                                                            \
                int _r;                                                 \
                _r = synchronizecache16(__VA_ARGS__);                   \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] SYNCHRONIZECACHE16 "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "SYNCHRONIZECACHE16. Skipping test");   \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define TESTUNITREADY(...)                                              \
        do {                                                            \
                int _r;                                                 \
                _r = testunitready(__VA_ARGS__);                        \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] TESTUNITREADY "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "TESTUNITREADY. Skipping test");        \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define UNMAP(...)                                                      \
        do {                                                            \
                int _r;                                                 \
                _r = unmap(__VA_ARGS__);                                \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] UNMAP "          \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "UNMAP. Skipping test");                \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define VERIFY10(...)                                                   \
        do {                                                            \
                int _r;                                                 \
                _r = verify10(__VA_ARGS__);                             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] VERIFY10 "       \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "VERIFY10. Skipping test");             \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define VERIFY12(...)                                                   \
        do {                                                            \
                int _r;                                                 \
                _r = verify12(__VA_ARGS__);                             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] VERIFY12 "       \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "VERIFY12. Skipping test");             \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define VERIFY16(...)                                                   \
        do {                                                            \
                int _r;                                                 \
                _r = verify16(__VA_ARGS__);                             \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] VERIFY16 "       \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "VERIFY16. Skipping test");             \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITE10(...)                                                    \
        ({								\
                int _r;                                                 \
                _r = write10(__VA_ARGS__);                              \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITE10 "        \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITE10. Skipping test");              \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
		_r;							\
        })

#define WRITE12(...)                                                    \
        do {                                                            \
                int _r;                                                 \
                _r = write12(__VA_ARGS__);                              \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITE12 "        \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITE12. Skipping test");              \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITE16(...)                                                    \
        do {                                                            \
                int _r;                                                 \
                _r = write16(__VA_ARGS__);                              \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITE16 "        \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITE16. Skipping test");              \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITEATOMIC16(...)                                              \
        do {                                                            \
                int _r;                                                 \
                _r = writeatomic16(__VA_ARGS__);                        \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITEATOMIC16 "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITEATOMIC16. Skipping test");        \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITESAME10(...)                                                \
        do {                                                            \
                int _r;                                                 \
                _r = writesame10(__VA_ARGS__);                          \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITESAME10 "    \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITESAME10. Skipping test");          \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITESAME16(...)                                                \
        do {                                                            \
                int _r;                                                 \
                _r = writesame16(__VA_ARGS__);                          \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITESAME16 "    \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITESAME16. Skipping test");          \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITEVERIFY10(...)                                              \
        do {                                                            \
                int _r;                                                 \
                _r = writeverify10(__VA_ARGS__);                        \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY10 "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITEVERIFY10. Skipping test");        \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITEVERIFY12(...)                                              \
        do {                                                            \
                int _r;                                                 \
                _r = writeverify12(__VA_ARGS__);                        \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY12 "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITEVERIFY12. Skipping test");        \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define WRITEVERIFY16(...)                                              \
        do {                                                            \
                int _r;                                                 \
                _r = writeverify16(__VA_ARGS__);                        \
                if (_r == -2) {                                         \
                        logging(LOG_NORMAL, "[SKIPPED] WRITEVERIFY16 "  \
                                "is not implemented.");                 \
                        CU_PASS("[SKIPPED] Target does not support "    \
                                "WRITEVERIFY16. Skipping test");        \
                        return;                                         \
                }                                                       \
                CU_ASSERT_EQUAL(_r, 0);                                 \
        } while (0);

#define ALL_ZERO(...)                                                   \
        do {                                                            \
                if (all_zero(__VA_ARGS__) == 0) {                       \
                        logging(LOG_NORMAL, "[FAILED] Blocks did not "  \
                                "read back as zero");                   \
                        CU_FAIL("[FAILED] Blocks did not read back "    \
                                "as zero");                             \
                        return;                                         \
                }                                                       \
        } while (0);

extern struct scsi_inquiry_standard *inq;
extern struct scsi_inquiry_logical_block_provisioning *inq_lbp;
extern struct scsi_inquiry_block_device_characteristics *inq_bdc;
extern struct scsi_inquiry_block_limits *inq_bl;
extern struct scsi_readcapacity16 *rc16;
extern struct scsi_report_supported_op_codes *rsop;

extern unsigned char *scratch;
extern size_t block_size;
extern uint64_t num_blocks;
extern int lbppb;
extern int data_loss;
extern int allow_sanitize;
extern int readonly;
extern int sbc3_support;
extern int maximum_transfer_length;

struct scsi_device {
        char *error_str;

        struct iscsi_context *iscsi_ctx;
        int iscsi_lun;
        char *iscsi_url;

        char *sgio_dev;
        int sgio_fd;
};
extern struct scsi_device *sd;

struct iscsi_context *iscsi_context_login(const char *initiatorname, const char *url, int *lun);

struct iscsi_async_state {
        struct scsi_task *task;
        int status;
        int finished;
};
void wait_until_test_finished(struct iscsi_context *iscsi, struct iscsi_async_state *test_state);

struct iscsi_pdu;
int (*local_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);

struct scsi_command_descriptor *get_command_descriptor(int opcode, int sa);

/*
 * PGR support
 */

static inline long rand_key(void)
{
        static int seed = 0;

        if (!seed) {
                struct timeval tv;
                pid_t p;
                unsigned int s;

                gettimeofday(&tv, NULL);
                p = getpid();
                s = p ^ tv.tv_sec ^ tv.tv_usec;
                srandom(s);
        }
        seed = 1;
        return random();
}

static inline int pr_type_is_all_registrants(
        enum scsi_persistent_out_type pr_type)
{
        switch (pr_type) {
        case SCSI_PERSISTENT_RESERVE_TYPE_WRITE_EXCLUSIVE_ALL_REGISTRANTS:
        case SCSI_PERSISTENT_RESERVE_TYPE_EXCLUSIVE_ACCESS_ALL_REGISTRANTS:
                return 1;
        default:
                return 0;
        }
}

int all_zero(const unsigned char *buf, unsigned size);

int prin_task(struct scsi_device *sdev, int service_action,
    int success_expected);
int prin_read_keys(struct scsi_device *sdev, struct scsi_task **tp,
                   struct scsi_persistent_reserve_in_read_keys **rkp,
                   uint16_t allocation_len);
int prout_register_and_ignore(struct scsi_device *sdev,
    unsigned long long key);
int prout_register_key(struct scsi_device *sdev,
    unsigned long long sark, unsigned long long rk);
int prin_verify_key_presence(struct scsi_device *sdev,
    unsigned long long key, int present);
int prout_reregister_key_fails(struct scsi_device *sdev,
    unsigned long long sark);
int prout_reserve(struct scsi_device *sdev,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int prout_release(struct scsi_device *sdev,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int prout_clear(struct scsi_device *sdev, unsigned long long key);
int prout_preempt(struct scsi_device *sdev,
                  unsigned long long sark, unsigned long long rk,
                  enum scsi_persistent_out_type pr_type);
int prin_verify_not_reserved(struct scsi_device *sdev);
int prin_verify_reserved_as(struct scsi_device *sdev,
    unsigned long long key, enum scsi_persistent_out_type pr_type);
int prin_report_caps(struct scsi_device *sdev, struct scsi_task **tp,
        struct scsi_persistent_reserve_in_report_capabilities **_rcaps);
int verify_read_works(struct scsi_device *sdev, unsigned char *buf);
int verify_write_works(struct scsi_device *sdev, unsigned char *buf);
int verify_read_fails(struct scsi_device *sdev, unsigned char *buf);
int verify_write_fails(struct scsi_device *sdev, unsigned char *buf);

int compareandwrite(struct scsi_device *sdev, uint64_t lba, unsigned char *data, uint32_t len, int blocksize, int wrprotect, int dpo, int fua, int group_number, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int get_lba_status(struct scsi_device *sdev, struct scsi_task **task, uint64_t lba, uint32_t len, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int inquiry(struct scsi_device *sdev, struct scsi_task **task, int evpd, int page_code, int maxsize, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int modesense6(struct scsi_device *sdev, struct scsi_task **task, int dbd, enum scsi_modesense_page_control pc, enum scsi_modesense_page_code page_code, int sub_page_code, unsigned char alloc_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int modeselect6(struct scsi_device *sdev, int pf, int sp, struct scsi_mode_page *mp, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int orwrite(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int prefetch10(struct scsi_device *sdev, uint32_t lba, int num_blocks, int immed, int group, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int prefetch16(struct scsi_device *sdev, uint64_t lba, int num_blocks, int immed, int group, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int preventallow(struct scsi_device *sdev, int prevent);
int read6(struct scsi_device *sdev, struct scsi_task **task, uint32_t lba, uint32_t datalen, int blocksize, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int read10(struct scsi_device *sdev, struct scsi_task **task, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int read12(struct scsi_device *sdev, struct scsi_task **task, uint32_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int read16(struct scsi_device *sdev, struct scsi_task **task, uint64_t lba, uint32_t datalen, int blocksize, int rdprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int readcapacity10(struct scsi_device *sdev, struct scsi_task **task, uint32_t lba, int pmi, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int readcapacity16(struct scsi_device *sdev, struct scsi_task **task, int alloc_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int readdefectdata10(struct scsi_device *sdev, struct scsi_task **task,
                     int req_plist, int req_glist,
                     int defect_list_format, uint16_t alloc_len,
                     int status, enum scsi_sense_key key,
                     int *ascq, int num_ascq);
int readdefectdata12(struct scsi_device *sdev, struct scsi_task **task,
                     int req_plist, int req_glist,
                     int defect_list_format,
                     uint32_t address_descriptor_index,
                     uint32_t alloc_len,
                     int status, enum scsi_sense_key key,
                     int *ascq, int num_ascq);
int report_supported_opcodes(struct scsi_device *sdev, struct scsi_task **save_task, int rctd, int options, int opcode, int sa, int alloc_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int release6(struct scsi_device *sdev);
int reserve6(struct scsi_device *sdev);
int reserve6_conflict(struct scsi_device *sdev);
int sanitize(struct scsi_device *sdev, int immed, int ause, int sa, int param_len, struct iscsi_data *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int startstopunit(struct scsi_device *sdev, int immed, int pcm, int pc, int no_flush, int loej, int start, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int synchronizecache10(struct scsi_device *sdev, uint32_t lba, int num_blocks, int sync_nv, int immed, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int synchronizecache16(struct scsi_device *sdev, uint64_t lba, int num_blocks, int sync_nv, int immed, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int testunitready_clear_ua(struct scsi_device *sdev);
int testunitready(struct scsi_device *sdev, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int unmap(struct scsi_device *sdev, int anchor, struct unmap_list *list, int list_len, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int verify10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int verify12(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int verify16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int vprotect, int dpo, int bytchk, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int write10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int write12(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int write16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int fua_nv, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int writeatomic16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int fua, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int writesame10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int writesame16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int num_blocks, int anchor, int unmap, int wrprotect, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int writeverify10(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int writeverify12(struct scsi_device *sdev, uint32_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int writeverify16(struct scsi_device *sdev, uint64_t lba, uint32_t datalen, int blocksize, int wrprotect, int dpo, int bytchk, int group, unsigned char *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);

int set_swp(struct scsi_device *sdev);
int clear_swp(struct scsi_device *sdev);

int extendedcopy(struct scsi_device *sdev, struct iscsi_data *data, int status, enum scsi_sense_key key, int *ascq, int num_ascq);
int get_desc_len(enum ec_descr_type_code desc_type);
int populate_tgt_desc(unsigned char *desc, enum ec_descr_type_code desc_type, int luid_type, int nul, int peripheral_type, int rel_init_port_id, int pad, struct scsi_device *dev);
int populate_seg_desc_hdr(unsigned char *hdr, enum ec_descr_type_code desc_type, int dc, int cat, int src_index, int dst_index);
int populate_seg_desc_b2b(unsigned char *desc, int dc, int cat, int src_index, int dst_index, int num_blks, uint64_t src_lba, uint64_t dst_lba);
void populate_param_header(unsigned char *buf, int list_id, int str, int list_id_usage, int prio, int tgt_desc_len, int seg_desc_len, int inline_data_len);
int receive_copy_results(struct scsi_task **task, struct scsi_device *sdev,
                         enum scsi_copy_results_sa sa, int list_id,
                         void **datap, int status, enum scsi_sense_key key,
                         int *ascq, int num_ascq);
int test_iscsi_tur_until_good(struct scsi_device *iscsi_sd, int *num_uas);

uint64_t test_get_clock_sec(void);

#endif        /* _ISCSI_SUPPORT_H_ */
