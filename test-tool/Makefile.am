AM_CPPFLAGS=-I. -I../include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = ../lib/libiscsi.la

EXTRA_DIST = README

dist_noinst_HEADERS = iscsi-support.h \
		      iscsi-test.h \
		      iscsi-test-cu.h

# libiscsi test tool using cunit
if ISCSITEST
bin_PROGRAMS = iscsi-test-cu
iscsi_test_cu_LDFLAGS = -ldl -lcunit
iscsi_test_cu_SOURCES = iscsi-test-cu.c \
	iscsi-support.c \
	test_compareandwrite_simple.c \
	test_compareandwrite_miscompare.c \
	test_get_lba_status_simple.c \
	test_get_lba_status_beyond_eol.c \
	test_inquiry_alloc_length.c \
	test_inquiry_block_limits.c \
	test_inquiry_evpd.c \
	test_inquiry_mandatory_vpd_sbc.c \
	test_inquiry_standard.c \
	test_inquiry_supported_vpd.c \
	test_inquiry_version_descriptors.c \
	test_iscsi_cmdsn_toohigh.c \
	test_iscsi_cmdsn_toolow.c \
	test_iscsi_datasn_invalid.c \
	test_mandatory_sbc.c \
	test_modesense6_all_pages.c \
	test_modesense6_residuals.c \
	test_nomedia_sbc.c \
	test_orwrite_simple.c \
	test_orwrite_beyond_eol.c \
	test_orwrite_0blocks.c \
	test_orwrite_wrprotect.c \
	test_orwrite_flags.c \
	test_orwrite_verify.c \
	test_prefetch10_simple.c \
	test_prefetch10_beyond_eol.c \
	test_prefetch10_0blocks.c \
	test_prefetch10_flags.c \
	test_prefetch16_simple.c \
	test_prefetch16_beyond_eol.c \
	test_prefetch16_0blocks.c \
	test_prefetch16_flags.c \
	test_preventallow_simple.c \
	test_preventallow_eject.c \
	test_preventallow_itnexus_loss.c \
	test_preventallow_logout.c \
	test_preventallow_warm_reset.c \
	test_preventallow_cold_reset.c \
	test_preventallow_lun_reset.c \
	test_preventallow_2_itnexuses.c \
	test_prin_read_keys_simple.c \
	test_prin_serviceaction_range.c \
	test_prout_register_simple.c \
	test_prout_reserve_simple.c \
	test_prout_reserve_access.c \
	test_prout_reserve_ownership.c \
	test_read6_simple.c \
	test_read6_beyond_eol.c \
	test_read10_simple.c \
	test_read10_beyond_eol.c \
	test_read10_0blocks.c \
	test_read10_rdprotect.c \
	test_read10_flags.c \
	test_read10_residuals.c \
	test_read10_invalid.c \
	test_read12_simple.c \
	test_read12_beyond_eol.c \
	test_read12_0blocks.c \
	test_read12_rdprotect.c \
	test_read12_flags.c \
	test_read12_residuals.c \
	test_read16_simple.c \
	test_read16_beyond_eol.c \
	test_read16_0blocks.c \
	test_read16_rdprotect.c \
	test_read16_flags.c \
	test_read16_residuals.c \
	test_readcapacity10_simple.c \
	test_readcapacity16_alloclen.c \
	test_readcapacity16_protection.c \
	test_readcapacity16_simple.c \
	test_readonly_sbc.c \
	test_report_supported_opcodes_one_command.c \
	test_report_supported_opcodes_rctd.c \
	test_report_supported_opcodes_servactv.c \
	test_report_supported_opcodes_simple.c \
	test_reserve6_simple.c \
	test_reserve6_2initiators.c \
	test_reserve6_logout.c \
	test_reserve6_itnexus_loss.c \
	test_reserve6_target_warm_reset.c \
	test_reserve6_target_cold_reset.c \
	test_reserve6_lun_reset.c \
	test_sanitize_block_erase.c \
	test_sanitize_block_erase_reserved.c \
	test_sanitize_crypto_erase.c \
	test_sanitize_crypto_erase_reserved.c \
	test_sanitize_exit_failure_mode.c \
	test_sanitize_invalid_serviceaction.c \
	test_sanitize_overwrite.c \
	test_sanitize_overwrite_reserved.c \
	test_sanitize_readonly.c \
	test_sanitize_reservations.c \
	test_sanitize_reset.c \
	test_startstopunit_simple.c \
	test_startstopunit_pwrcnd.c \
	test_startstopunit_noloej.c \
	test_testunitready_simple.c \
	test_unmap_simple.c \
	test_unmap_0blocks.c \
	test_unmap_vpd.c \
	test_verify10_simple.c \
	test_verify10_beyond_eol.c \
	test_verify10_0blocks.c \
	test_verify10_vrprotect.c \
	test_verify10_flags.c \
	test_verify10_mismatch.c \
	test_verify10_mismatch_no_cmp.c \
	test_verify12_simple.c \
	test_verify12_beyond_eol.c \
	test_verify12_0blocks.c \
	test_verify12_vrprotect.c \
	test_verify12_flags.c \
	test_verify12_mismatch.c \
	test_verify12_mismatch_no_cmp.c \
	test_verify16_simple.c \
	test_verify16_beyond_eol.c \
	test_verify16_0blocks.c \
	test_verify16_vrprotect.c \
	test_verify16_flags.c \
	test_verify16_mismatch.c \
	test_verify16_mismatch_no_cmp.c \
	test_write10_simple.c \
	test_write10_beyond_eol.c \
	test_write10_0blocks.c \
	test_write10_wrprotect.c \
	test_write10_flags.c \
	test_write10_residuals.c \
	test_write12_simple.c \
	test_write12_beyond_eol.c \
	test_write12_0blocks.c \
	test_write12_wrprotect.c \
	test_write12_flags.c \
	test_write12_residuals.c \
	test_write16_simple.c \
	test_write16_beyond_eol.c \
	test_write16_0blocks.c \
	test_write16_wrprotect.c \
	test_write16_flags.c \
	test_write16_residuals.c \
	test_writesame10_simple.c \
	test_writesame10_beyond_eol.c \
	test_writesame10_0blocks.c \
	test_writesame10_wrprotect.c \
	test_writesame10_unmap.c \
	test_writesame10_unmap_unaligned.c \
	test_writesame10_unmap_until_end.c \
	test_writesame10_unmap_vpd.c \
	test_writesame16_simple.c \
	test_writesame16_beyond_eol.c \
	test_writesame16_0blocks.c \
	test_writesame16_wrprotect.c \
	test_writesame16_unmap.c \
	test_writesame16_unmap_unaligned.c \
	test_writesame16_unmap_until_end.c \
	test_writesame16_unmap_vpd.c \
	test_writeverify10_simple.c \
	test_writeverify10_beyond_eol.c \
	test_writeverify10_0blocks.c \
	test_writeverify10_wrprotect.c \
	test_writeverify10_flags.c \
	test_writeverify10_residuals.c \
	test_writeverify12_simple.c \
	test_writeverify12_beyond_eol.c \
	test_writeverify12_0blocks.c \
	test_writeverify12_wrprotect.c \
	test_writeverify12_flags.c \
	test_writeverify12_residuals.c \
	test_writeverify16_simple.c \
	test_writeverify16_beyond_eol.c \
	test_writeverify16_0blocks.c \
	test_writeverify16_wrprotect.c \
	test_writeverify16_flags.c \
	test_writeverify16_residuals.c

endif



# old deprecated libiscsi test tool
noinst_PROGRAMS = iscsi-test
iscsi_test_LDFLAGS = -ldl
iscsi_test_SOURCES = iscsi-test.c \
	iscsi-support.c \
	0000_testunitready_simple.c \
	0100_read10_simple.c 0101_read10_beyond_eol.c \
	0102_read10_0blocks.c 0103_read10_rdprotect.c \
	0104_read10_flags.c 0105_read10_invalid.c \
	0110_readcapacity10_simple.c \
	0120_read6_simple.c \
	0121_read6_beyond_eol.c 0122_read6_invalid.c \
	0130_verify10_simple.c 0131_verify10_mismatch.c \
	0132_verify10_mismatch_no_cmp.c 0133_verify10_beyondeol.c \
	0160_readcapacity16_simple.c \
	0161_readcapacity16_alloclen.c \
	0170_unmap_simple.c 0171_unmap_zero.c \
	0180_writesame10_unmap.c 0181_writesame10_unmap_unaligned.c \
	0182_writesame10_beyondeol.c \
	0183_writesame10_wrprotect.c \
	0184_writesame10_0blocks.c \
	0190_writesame16_unmap.c 0191_writesame16_unmap_unaligned.c \
	0192_writesame16_beyondeol.c \
	0193_writesame16_wrprotect.c \
	0194_writesame16_0blocks.c \
	0200_read16_simple.c 0201_read16_rdprotect.c \
	0202_read16_flags.c  0203_read16_0blocks.c   \
	0204_read16_beyondeol.c  \
	0210_read12_simple.c 0211_read12_rdprotect.c \
	0212_read12_flags.c  0213_read12_0blocks.c   \
	0214_read12_beyondeol.c \
	0220_write16_simple.c 0221_write16_wrprotect.c \
	0222_write16_flags.c  0223_write16_0blocks.c   \
	0224_write16_beyondeol.c \
	0230_write12_simple.c 0231_write12_wrprotect.c \
	0232_write12_flags.c  0233_write12_0blocks.c   \
	0234_write12_beyondeol.c \
	0240_prefetch10_simple.c \
	0241_prefetch10_flags.c \
	0242_prefetch10_beyondeol.c \
	0243_prefetch10_0blocks.c \
	0250_prefetch16_simple.c \
	0251_prefetch16_flags.c \
	0252_prefetch16_beyondeol.c \
	0253_prefetch16_0blocks.c \
	0260_get_lba_status_simple.c 0264_get_lba_status_beyondeol.c \
	0270_verify16_simple.c 0271_verify16_mismatch.c \
	0272_verify16_mismatch_no_cmp.c 0273_verify16_beyondeol.c \
	0280_verify12_simple.c 0281_verify12_mismatch.c \
	0282_verify12_mismatch_no_cmp.c 0283_verify12_beyondeol.c \
	0290_write10_simple.c 0291_write10_wrprotect.c \
	0292_write10_flags.c  0293_write10_0blocks.c   \
	0294_write10_beyondeol.c \
	0300_readonly.c \
	0310_writeverify10_simple.c 0311_writeverify10_wrprotect.c \
	0314_writeverify10_beyondeol.c \
	0320_writeverify12_simple.c 0321_writeverify12_wrprotect.c \
	0324_writeverify12_beyondeol.c \
	0330_writeverify16_simple.c 0331_writeverify16_wrprotect.c \
	0334_writeverify16_beyondeol.c \
	0340_compareandwrite_simple.c 0341_compareandwrite_mismatch.c \
	0343_compareandwrite_beyondeol.c \
	0350_orwrite_simple.c \
	0351_orwrite_wrprotect.c \
	0354_orwrite_beyondeol.c \
	0360_startstopunit_simple.c \
	0361_startstopunit_pwrcnd.c \
	0362_startstopunit_noloej.c \
	0370_nomedia.c \
	0380_preventallow_simple.c \
	0381_preventallow_eject.c \
	0382_preventallow_itnexus_loss.c \
	0383_preventallow_target_warm_reset.c \
	0384_preventallow_target_cold_reset.c \
	0385_preventallow_lun_reset.c \
	0386_preventallow_2_it_nexuses.c \
	0390_mandatory_opcodes_sbc.c \
	0400_inquiry_basic.c \
	0401_inquiry_alloclen.c \
	0402_inquiry_evpd.c \
	0403_inquiry_supported_vpd.c \
	0404_inquiry_all_reported_vpd.c \
	0410_readtoc_basic.c \
	0420_reserve6_simple.c \
	0421_reserve6_lun_reset.c \
	0422_reserve6_logout.c \
	0423_reserve6_sessionloss.c \
	0424_reserve6_target_reset.c \
	0430_report_all_supported_ops.c \
	\
	1000_cmdsn_invalid.c \
	1010_datasn_invalid.c \
	1020_bufferoffset_invalid.c \
	1030_unsolicited_data_overflow.c \
	1031_unsolicited_data_out.c \
	1040_saturate_maxcmdsn.c \
	1041_unsolicited_immediate_data.c \
	1042_unsolicited_nonimmediate_data.c \
	1100_persistent_reserve_in_read_keys_simple.c \
	1110_persistent_reserve_in_serviceaction_range.c \
	1120_persistent_register_simple.c \
	1130_persistent_reserve_simple.c \
	1140_persistent_reserve_access_check_ea.c \
	1141_persistent_reserve_access_check_we.c \
	1142_persistent_reserve_access_check_earo.c \
	1143_persistent_reserve_access_check_wero.c \
	1144_persistent_reserve_access_check_eaar.c \
	1145_persistent_reserve_access_check_wear.c

