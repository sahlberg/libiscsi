/*
   iscsi-test tool

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

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <poll.h>
#include <getopt.h>
#include <fnmatch.h>
#include "slist.h"
#include "iscsi.h"
#include "scsi-lowlevel.h"
#include "iscsi-private.h"
#include "iscsi-test.h"
#include "iscsi-support.h"

#ifndef discard_const
#define discard_const(ptr) ((void *)((intptr_t)(ptr)))
#endif

int loglevel = LOG_VERBOSE;

int (*real_iscsi_queue_pdu)(struct iscsi_context *iscsi, struct iscsi_pdu *pdu);


int show_info;

struct scsi_test {
       const char *name;
       int (*test)(const char *initiator, const char *url);
};

static struct scsi_test tests[] = {
/* SCSI protocol tests */

/* testunitready*/
{ "T0000_testunitready_simple",		T0000_testunitready_simple },

/* read10*/
{ "T0100_read10_simple",		T0100_read10_simple },
{ "T0101_read10_beyond_eol",		T0101_read10_beyond_eol },
{ "T0102_read10_0blocks",		T0102_read10_0blocks },
{ "T0103_read10_rdprotect",		T0103_read10_rdprotect },
{ "T0104_read10_flags",			T0104_read10_flags },
{ "T0105_read10_invalid",      		T0105_read10_invalid },

/* readcapacity10*/
{ "T0110_readcapacity10_simple",	T0110_readcapacity10_simple },

/* read6*/
{ "T0120_read6_simple",			T0120_read6_simple },
{ "T0121_read6_beyond_eol",		T0121_read6_beyond_eol },
{ "T0122_read6_invalid",      		T0122_read6_invalid },

/* verify10*/
{ "T0130_verify10_simple",		T0130_verify10_simple },
{ "T0131_verify10_mismatch",		T0131_verify10_mismatch },
{ "T0132_verify10_mismatch_no_cmp",	T0132_verify10_mismatch_no_cmp },
{ "T0133_verify10_beyondeol",		T0133_verify10_beyondeol },

/* readcapacity16*/
{ "T0160_readcapacity16_simple",	T0160_readcapacity16_simple },
{ "T0161_readcapacity16_alloclen",	T0161_readcapacity16_alloclen },

/* unmap*/
{ "T0170_unmap_simple",			T0170_unmap_simple },
{ "T0171_unmap_zero",			T0171_unmap_zero },

/* writesame10*/
{ "T0180_writesame10_unmap",		T0180_writesame10_unmap },
{ "T0181_writesame10_unmap_unaligned",	T0181_writesame10_unmap_unaligned },
{ "T0182_writesame10_beyondeol",	T0182_writesame10_beyondeol },
{ "T0183_writesame10_wrprotect",	T0183_writesame10_wrprotect },
{ "T0184_writesame10_0blocks",		T0184_writesame10_0blocks },

/* writesame16*/
{ "T0190_writesame16_unmap",		T0190_writesame16_unmap },
{ "T0191_writesame16_unmap_unaligned",	T0191_writesame16_unmap_unaligned },
{ "T0192_writesame16_beyondeol",	T0192_writesame16_beyondeol },
{ "T0193_writesame16_wrprotect",	T0193_writesame16_wrprotect },
{ "T0194_writesame16_0blocks",		T0194_writesame16_0blocks },

/* read16*/
{ "T0200_read16_simple",		T0200_read16_simple },
{ "T0201_read16_rdprotect",		T0201_read16_rdprotect },
{ "T0202_read16_flags",			T0202_read16_flags },
{ "T0203_read16_0blocks",		T0203_read16_0blocks },
{ "T0204_read16_beyondeol",		T0204_read16_beyondeol },

/* read12*/
{ "T0210_read12_simple",		T0210_read12_simple },
{ "T0211_read12_rdprotect",		T0211_read12_rdprotect },
{ "T0212_read12_flags",			T0212_read12_flags },
{ "T0213_read12_0blocks",		T0213_read12_0blocks },
{ "T0214_read12_beyondeol",		T0214_read12_beyondeol },

/* write16*/
{ "T0220_write16_simple",		T0220_write16_simple },
{ "T0221_write16_wrprotect",		T0221_write16_wrprotect },
{ "T0222_write16_flags",	       	T0222_write16_flags },
{ "T0223_write16_0blocks",		T0223_write16_0blocks },
{ "T0224_write16_beyondeol",		T0224_write16_beyondeol },

/* write12*/
{ "T0230_write12_simple",		T0230_write12_simple },
{ "T0231_write12_wrprotect",		T0231_write12_wrprotect },
{ "T0232_write12_flags",	       	T0232_write12_flags },
{ "T0233_write12_0blocks",		T0233_write12_0blocks },
{ "T0234_write12_beyondeol",		T0234_write12_beyondeol },

/* prefetch10*/
{ "T0240_prefetch10_simple",		T0240_prefetch10_simple },
{ "T0241_prefetch10_flags",		T0241_prefetch10_flags },
{ "T0242_prefetch10_beyondeol",		T0242_prefetch10_beyondeol },
{ "T0243_prefetch10_0blocks",		T0243_prefetch10_0blocks },

/* prefetch16*/
{ "T0250_prefetch16_simple",		T0250_prefetch16_simple },
{ "T0251_prefetch16_flags",		T0251_prefetch16_flags },
{ "T0252_prefetch16_beyondeol",		T0252_prefetch16_beyondeol },
{ "T0253_prefetch16_0blocks",		T0253_prefetch16_0blocks },

/* get_lba_status */
{ "T0260_get_lba_status_simple",	T0260_get_lba_status_simple },
{ "T0264_get_lba_status_beyondeol",	T0264_get_lba_status_beyondeol },

/* verify16*/
{ "T0270_verify16_simple",		T0270_verify16_simple },
{ "T0271_verify16_mismatch",		T0271_verify16_mismatch },
{ "T0272_verify16_mismatch_no_cmp",	T0272_verify16_mismatch_no_cmp },
{ "T0273_verify16_beyondeol",		T0273_verify16_beyondeol },

/* verify12*/
{ "T0280_verify12_simple",		T0280_verify12_simple },
{ "T0281_verify12_mismatch",		T0281_verify12_mismatch },
{ "T0282_verify12_mismatch_no_cmp",	T0282_verify12_mismatch_no_cmp },
{ "T0283_verify12_beyondeol",		T0283_verify12_beyondeol },

/* write10*/
{ "T0290_write10_simple",		T0290_write10_simple },
{ "T0291_write10_wrprotect",		T0291_write10_wrprotect },
{ "T0292_write10_flags",	       	T0292_write10_flags },
{ "T0293_write10_0blocks",		T0293_write10_0blocks },
{ "T0294_write10_beyondeol",		T0294_write10_beyondeol },

/* Readonly */
{ "T0300_readonly",			T0300_readonly },

/* writeverify10*/
{ "T0310_writeverify10_simple",		T0310_writeverify10_simple },
{ "T0311_writeverify10_wrprotect",	T0311_writeverify10_wrprotect },
{ "T0314_writeverify10_beyondeol",	T0314_writeverify10_beyondeol },

/* writeverify12*/
{ "T0320_writeverify12_simple",		T0320_writeverify12_simple },
{ "T0321_writeverify12_wrprotect",	T0321_writeverify12_wrprotect },
{ "T0324_writeverify12_beyondeol",	T0324_writeverify12_beyondeol },

/* writeverify16*/
{ "T0330_writeverify16_simple",		T0330_writeverify16_simple },
{ "T0331_writeverify16_wrprotect",	T0331_writeverify16_wrprotect },
{ "T0334_writeverify16_beyondeol",	T0334_writeverify16_beyondeol },

/* compareandwrite*/
{ "T0340_compareandwrite_simple",	T0340_compareandwrite_simple },
{ "T0341_compareandwrite_mismatch",	T0341_compareandwrite_mismatch },
{ "T0343_compareandwrite_beyondeol",	T0343_compareandwrite_beyondeol },

/* orwrite*/
{ "T0350_orwrite_simple",		T0350_orwrite_simple },
{ "T0351_orwrite_wrprotect",		T0351_orwrite_wrprotect },
{ "T0354_orwrite_beyindeol",		T0354_orwrite_beyondeol },

/* startstopunit*/
{ "T0360_startstopunit_simple",		T0360_startstopunit_simple },
{ "T0361_startstopunit_pwrcnd",		T0361_startstopunit_pwrcnd },
{ "T0362_startstopunit_noloej",		T0362_startstopunit_noloej },

/* nomedia*/
{ "T0370_nomedia",			T0370_nomedia },

/* preventallowmediumremoval*/
{ "T0380_preventallow_simple",		T0380_preventallow_simple },
{ "T0381_preventallow_eject",		T0381_preventallow_eject },
{ "T0382_preventallow_itnexus_loss",	T0382_preventallow_itnexus_loss },
{ "T0383_preventallow_target_warm_reset",	T0383_preventallow_target_warm_reset },
{ "T0384_preventallow_target_cold_reset",	T0384_preventallow_target_cold_reset },
{ "T0385_preventallow_lun_reset",	T0385_preventallow_lun_reset },
{ "T0386_preventallow_2_itl_nexuses",	T0386_preventallow_2_itl_nexuses },

/* support for mandatory opcodes*/
{ "T0390_mandatory_opcodes_sbc",	T0390_mandatory_opcodes_sbc },

/* inquiry*/
{ "T0400_inquiry_basic",		T0400_inquiry_basic },
{ "T0401_inquiry_alloclen",		T0401_inquiry_alloclen },
{ "T0402_inquiry_evpd",			T0402_inquiry_evpd },
{ "T0403_inquiry_supported_vpd",	T0403_inquiry_supported_vpd },
{ "T0404_inquiry_all_reported_vpd",	T0404_inquiry_all_reported_vpd },

/* read TOC/PMA/ATIP */
{ "T0410_readtoc_basic",                T0410_readtoc_basic },

/* reserve6/release6 */
{ "T0420_reserve6_simple",              T0420_reserve6_simple },
{ "T0421_reserve6_lun_reset",           T0421_reserve6_lun_reset },
{ "T0422_reserve6_logout",              T0422_reserve6_logout },
{ "T0423_reserve6_sessionloss",         T0423_reserve6_sessionloss },
{ "T0424_reserve6_target_reset",           T0424_reserve6_target_reset },

/* Maintenance In - Report Supported Operations */
{ "T0430_report_all_supported_ops",     T0430_report_all_supported_ops },

/* iSCSI protocol tests */

/* invalid cmdsn from initiator */
{ "T1000_cmdsn_invalid",		T1000_cmdsn_invalid },

/* invalid datasn from initiator */
{ "T1010_datasn_invalid",		T1010_datasn_invalid },

/* invalid bufferoffset from initiator */
{ "T1020_bufferoffset_invalid",		T1020_bufferoffset_invalid },

/* sending too much unsolicited data */
{ "T1030_unsolicited_data_overflow",	T1030_unsolicited_data_overflow },
{ "T1031_unsolicited_data_out",		T1031_unsolicited_data_out },

/* Test that if we start blocking new I/O due to saturating maxcmdsn
   that we eventualld do recover and finish
*/
{ "T1040_saturate_maxcmdsn",		T1040_saturate_maxcmdsn },

{ "T1041_unsolicited_immediate_data",	T1041_unsolicited_immediate_data },
{ "T1042_unsolicited_nonimmediate_data",T1042_unsolicited_nonimmediate_data },

/* PERSISTENT_RESERVE_IN/PERSISTENT_RESERVE_OUT */
{ "T1100_persistent_reserve_in_read_keys_simple", T1100_persistent_reserve_in_read_keys_simple },
{ "T1110_persistent_reserve_in_serviceaction_range", T1110_persistent_reserve_in_serviceaction_range },
{ "T1120_persistent_register_simple", T1120_persistent_register_simple },
{ "T1130_persistent_reserve_simple", T1130_persistent_reserve_simple },
{ "T1140_persistent_reserve_access_check_ea", T1140_persistent_reserve_access_check_ea },
{ "T1141_persistent_reserve_access_check_we", T1141_persistent_reserve_access_check_we },
{ "T1142_persistent_reserve_access_check_earo", T1142_persistent_reserve_access_check_earo },
{ "T1143_persistent_reserve_access_check_wero", T1143_persistent_reserve_access_check_wero },
{ "T1144_persistent_reserve_access_check_eaar", T1144_persistent_reserve_access_check_eaar },
{ "T1145_persistent_reserve_access_check_wear", T1145_persistent_reserve_access_check_wear },

{ NULL, NULL }
};

	
static void print_usage(void)
{
	fprintf(stderr, "Usage: iscsi-test [-?] [-?|--help] [--usage] [-t|--test=<test>] [-s|--skip=<test>]\n"
			"\t\t[-l|--list] [--info] [-i|--initiator-name=<iqn-name>]\n"
			"\t\t<iscsi-url>\n");
}

static void print_help(void)
{
	fprintf(stderr, "Usage: iscsi-test [OPTION...] <iscsi-url>\n");
	fprintf(stderr, "  -i, --initiator-name=iqn-name     Initiatorname to use\n");
	fprintf(stderr, "  -I, --initiator-name-2=iqn-name   Second initiatorname to use\n");
	fprintf(stderr, "  -t, --test=test-name              Which test to run. Default is to run all tests.\n");
	fprintf(stderr, "  -s, --skip=test-name              Which test to skip. Default is to run all tests.\n");
	fprintf(stderr, "  -l, --list                        List all tests.\n");
	fprintf(stderr, "  --info,                           Print extra info about a test.\n");
	fprintf(stderr, "  --dataloss                        Allow destructive tests.\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Help options:\n");
	fprintf(stderr, "  -?, --help                        Show this help message\n");
	fprintf(stderr, "      --usage                       Display brief usage message\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "iSCSI URL format : %s\n", ISCSI_URL_SYNTAX);
	fprintf(stderr, "\n");
	fprintf(stderr, "<host> is either of:\n");
	fprintf(stderr, "  \"hostname\"       iscsi.example\n");
	fprintf(stderr, "  \"ipv4-address\"   10.1.1.27\n");
	fprintf(stderr, "  \"ipv6-address\"   [fce0::1]\n");
}


int main(int argc, char *argv[])
{
	const char *url = NULL;
	int show_help = 0, show_usage = 0, list_tests = 0;
	int res, num_failed, num_skipped;
	struct scsi_test *test;
	char *testname = NULL;
	char *skipname = NULL;
	int lun;
	struct iscsi_context *iscsi;
	struct scsi_task *task;
	struct scsi_task *inq_task;
	struct scsi_task *inq_bl_task;
	struct scsi_task *rc16_task;
	struct scsi_readcapacity10 *rc10;
	int full_size;
	int c;

	static struct option long_options[] = {
		{"help",             no_argument,          NULL,        'h'},
		{"usage",            no_argument,          NULL,        'u'},
		{"dataloss",         no_argument,          NULL,        'D'},
		{"info",             no_argument,          NULL,        'X'},
		{"list",             no_argument,          NULL,        'l'},
		{"test",             required_argument,    NULL,        't'},
		{"skip",             required_argument,    NULL,        's'},
		{"initiator_name",   required_argument,    NULL,        'i'},
		{"initiator_name-2", required_argument,    NULL,        'I'},
		{0, 0, 0, 0}
	};
	int option_index;

	while ((c = getopt_long(argc, argv, "h?ui:I:ls:t:", long_options,
			&option_index)) != -1) {
		switch (c) {
		case 'h':
		case '?':
			show_help = 1;
			break;
		case 'u':
			show_usage = 1;
			break;
		case 'l':
			list_tests = 1;
			break;
		case 'D':
			data_loss = 1;
			break;
		case 'X':
			show_info = 1;
			break;
		case 's':
			skipname = optarg;
			break;
		case 't':
			testname = optarg;
			break;
		case 'i':
			initiatorname1 = optarg;
			break;
		case 'I':
			initiatorname2 = optarg;
			break;
		default:
			fprintf(stderr, "Unrecognized option '%c'\n\n", c);
			print_help();
			exit(0);
		}
	}


	real_iscsi_queue_pdu = dlsym(RTLD_NEXT, "iscsi_queue_pdu");


	if (show_help != 0) {
		print_help();
		exit(0);
	}

	if (show_usage != 0) {
		print_usage();
		exit(0);
	}

	if (list_tests != 0) {
		for (test = &tests[0]; test->name; test++) {
			printf("%s\n", test->name);
			if (show_info) {
				test->test(initiatorname1, url);
			}
		}
		exit(0);
	}

	if (argv[optind] != NULL) {
		url = strdup(argv[optind]);
	}
	if (url == NULL) {
		fprintf(stderr, "You must specify the URL\n");
		print_usage();
		exit(10);
	}


	iscsi = iscsi_context_login(initiatorname1, url, &lun);
	if (iscsi == NULL) {
		printf("Failed to login to target\n");
		return -1;
	}

	/* find the size of the LUN
	   All devices support readcapacity10 but only some support
	   readcapacity16
	*/
	task = iscsi_readcapacity10_sync(iscsi, lun, 0, 0);
	if (task == NULL) {
		printf("Failed to send READCAPACITY10 command: %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return -1;
	}
	if (task->status != SCSI_STATUS_GOOD) {
		printf("READCAPACITY10 command: failed with sense. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		iscsi_destroy_context(iscsi);
		return -1;
	}
	rc10 = scsi_datain_unmarshall(task);
	if (rc10 == NULL) {
		printf("failed to unmarshall READCAPACITY10 data. %s\n", iscsi_get_error(iscsi));
		scsi_free_scsi_task(task);
		iscsi_destroy_context(iscsi);
		return -1;
	}
	block_size = rc10->block_size;
	num_blocks = rc10->lba;
	scsi_free_scsi_task(task);

	rc16_task = iscsi_readcapacity16_sync(iscsi, lun);
	if (rc16_task == NULL) {
		printf("Failed to send READCAPACITY16 command: %s\n", iscsi_get_error(iscsi));
		iscsi_destroy_context(iscsi);
		return -1;
	}
	if (rc16_task->status == SCSI_STATUS_GOOD) {
		rc16 = scsi_datain_unmarshall(rc16_task);
		if (rc16 == NULL) {
			printf("failed to unmarshall READCAPACITY16 data. %s\n", iscsi_get_error(iscsi));
			scsi_free_scsi_task(rc16_task);
			iscsi_destroy_context(iscsi);
			return -1;
		}
		block_size = rc16->block_length;
		num_blocks = rc16->returned_lba;
		lbppb      = 1 << rc16->lbppbe;
	}


	inq_task = iscsi_inquiry_sync(iscsi, lun, 0, 0, 64);
	if (inq_task == NULL || inq_task->status != SCSI_STATUS_GOOD) {
		printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
		return -1;
	}
	full_size = scsi_datain_getfullsize(inq_task);
	if (full_size > inq_task->datain.size) {
		scsi_free_scsi_task(inq_task);

		/* we need more data for the full list */
		if ((inq_task = iscsi_inquiry_sync(iscsi, lun, 0, 0, full_size)) == NULL) {
			printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
			return -1;
		}
	}
	inq = scsi_datain_unmarshall(inq_task);
	if (inq == NULL) {
		printf("failed to unmarshall inquiry datain blob\n");
		scsi_free_scsi_task(inq_task);
		return -1;
	}

	/* try reading block limits vpd */
	inq_bl_task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, 64);
	if (inq_bl_task && inq_bl_task->status != SCSI_STATUS_GOOD) {
		scsi_free_scsi_task(inq_bl_task);
		inq_bl_task = NULL;
	}
	if (inq_bl_task) {
		full_size = scsi_datain_getfullsize(inq_bl_task);
		if (full_size > inq_bl_task->datain.size) {
			scsi_free_scsi_task(inq_bl_task);

			if ((inq_bl_task = iscsi_inquiry_sync(iscsi, lun, 1, SCSI_INQUIRY_PAGECODE_BLOCK_LIMITS, full_size)) == NULL) {
				printf("Inquiry command failed : %s\n", iscsi_get_error(iscsi));
				return -1;
			}
		}

		inq_bl = scsi_datain_unmarshall(inq_bl_task);
		if (inq_bl == NULL) {
			printf("failed to unmarshall inquiry datain blob\n");
			return -1;
		}
	}


	num_failed = num_skipped = 0;
	for (test = &tests[0]; test->name; test++) {
		if (testname != NULL && fnmatch(testname, test->name, 0)) {
			continue;
		}

		if (skipname != NULL) {
			char * pchr = skipname;
			char * pchr2 = NULL;
			int skip = 0;
			do {
				pchr2 = strchr(pchr,',');
				if (pchr2) pchr2[0]=0x00;
				if (!fnmatch(pchr, test->name, 0)) {
					skip = 1;
				}
				if (pchr2) {pchr2[0]=',';pchr=pchr2+1;}
			} while (pchr2);
			if (skip) continue;
		}

		res = test->test(initiatorname1, url);
		if (res == 0) {
			printf("TEST %s [OK]\n", test->name);
		} else if (res == -2) {
			printf("TEST %s [SKIPPED]\n", test->name);
			num_skipped++;
		} else {
			printf("TEST %s [FAILED]\n", test->name);
			num_failed++;
		}
		printf("\n");
	}

	scsi_free_scsi_task(inq_task);
	scsi_free_scsi_task(rc16_task);
	scsi_free_scsi_task(inq_bl_task);
	iscsi_logout_sync(iscsi);
	iscsi_destroy_context(iscsi);

	free(discard_const(url));

	return num_failed ? num_failed : num_skipped ? 77 : 0;
}
