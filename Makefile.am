# Generic definitions

ACLOCAL_AMFLAGS =-I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS=-I. -I$(srcdir)/include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = lib/libiscsi.la
XSLTPROC = /usr/bin/xsltproc

# Manpages
man1_MANS = doc/iscsi-inq.1 doc/iscsi-ls.1 doc/iscsi-swp.1
CLEANFILES = $(man1_MANS)

EXTRA_DIST = autogen.sh COPYING LICENCE-GPL-2.txt LICENCE-LGPL-2.1.txt \
	     packaging/RPM/libiscsi.spec.in packaging/RPM/makerpms.sh \
	     doc/iscsi-inq.1.xml doc/iscsi-ls.1.xml doc/iscsi-swp.1.xml \
	     aros/aros_compat.c aros/aros_compat.h aros/iscsi-ls.c \
	     aros/Makefile.AROS \
	     win32/vsbuild.bat win32/win32_compat.c win32/win32_compat.h \
	     lib/libiscsi.def \
	     test-tool/README \
	     patches/mtx-iscsi.diff patches/README patches/sg3_utils-1.32.patch

# Simplify conditions below by declaring variables as empty

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libiscsi.pc

bin_PROGRAMS =
noinst_PROGRAMS =
EXTRA_PROGRAMS =

# libiscsi shared library

iscsi_includedir = $(includedir)/iscsi
dist_iscsi_include_HEADERS = include/iscsi.h include/scsi-lowlevel.h
dist_noinst_HEADERS = include/iscsi-private.h include/md5.h include/slist.h
dist_noinst_DATA = lib/libiscsi.syms

lib_LTLIBRARIES = lib/libiscsi.la
lib_libiscsi_la_SOURCES = \
	lib/connect.c lib/crc32c.c lib/discovery.c lib/init.c \
	lib/login.c lib/nop.c lib/pdu.c lib/iscsi-command.c \
	lib/scsi-lowlevel.c lib/socket.c lib/sync.c lib/task_mgmt.c \
	lib/logging.c

if !HAVE_LIBGCRYPT
lib_libiscsi_la_SOURCES += lib/md5.c
endif

SOCURRENT=3
SOREVISON=10
SOAGE=0
lib_libiscsi_la_LDFLAGS = \
	-version-info $(SOCURRENT):$(SOREVISON):$(SOAGE) -bindir $(bindir) -no-undefined \
	-export-symbols $(srcdir)/lib/libiscsi.syms

# libiscsi utilities

bin_PROGRAMS += bin/iscsi-inq bin/iscsi-ls bin/iscsi-readcapacity16 \
	bin/iscsi-swp
bin_iscsi_inq_SOURCES = src/iscsi-inq.c
bin_iscsi_ls_SOURCES = src/iscsi-ls.c
bin_iscsi_swp_SOURCES = src/iscsi-swp.c
bin_iscsi_readcapacity16_SOURCES = src/iscsi-readcapacity16.c

# Other examples

noinst_PROGRAMS += bin/iscsiclient
bin_iscsiclient_SOURCES = examples/iscsiclient.c

EXTRA_PROGRAMS += bin/iscsi-dd
bin_iscsi_dd_SOURCES = examples/iscsi-dd.c

# libiscsi test tool

noinst_PROGRAMS += bin/iscsi-test
dist_noinst_HEADERS += test-tool/iscsi-support.h \
		       test-tool/iscsi-test.h \
		       test-tool/iscsi-test-cu.h
bin_iscsi_test_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/test-tool
bin_iscsi_test_LDFLAGS = -ldl
bin_iscsi_test_SOURCES = test-tool/iscsi-test.c \
	test-tool/iscsi-support.c \
	test-tool/0000_testunitready_simple.c \
	test-tool/0100_read10_simple.c test-tool/0101_read10_beyond_eol.c \
	test-tool/0102_read10_0blocks.c test-tool/0103_read10_rdprotect.c \
	test-tool/0104_read10_flags.c test-tool/0105_read10_invalid.c \
	test-tool/0110_readcapacity10_simple.c \
	test-tool/0120_read6_simple.c \
	test-tool/0121_read6_beyond_eol.c test-tool/0122_read6_invalid.c \
	test-tool/0130_verify10_simple.c test-tool/0131_verify10_mismatch.c \
	test-tool/0132_verify10_mismatch_no_cmp.c test-tool/0133_verify10_beyondeol.c \
	test-tool/0160_readcapacity16_simple.c \
	test-tool/0161_readcapacity16_alloclen.c \
	test-tool/0170_unmap_simple.c test-tool/0171_unmap_zero.c \
	test-tool/0180_writesame10_unmap.c test-tool/0181_writesame10_unmap_unaligned.c \
	test-tool/0182_writesame10_beyondeol.c \
	test-tool/0183_writesame10_wrprotect.c \
	test-tool/0184_writesame10_0blocks.c \
	test-tool/0190_writesame16_unmap.c test-tool/0191_writesame16_unmap_unaligned.c \
	test-tool/0192_writesame16_beyondeol.c \
	test-tool/0193_writesame16_wrprotect.c \
	test-tool/0194_writesame16_0blocks.c \
	test-tool/0200_read16_simple.c test-tool/0201_read16_rdprotect.c \
	test-tool/0202_read16_flags.c  test-tool/0203_read16_0blocks.c   \
	test-tool/0204_read16_beyondeol.c  \
	test-tool/0210_read12_simple.c test-tool/0211_read12_rdprotect.c \
	test-tool/0212_read12_flags.c  test-tool/0213_read12_0blocks.c   \
	test-tool/0214_read12_beyondeol.c \
	test-tool/0220_write16_simple.c test-tool/0221_write16_wrprotect.c \
	test-tool/0222_write16_flags.c  test-tool/0223_write16_0blocks.c   \
	test-tool/0224_write16_beyondeol.c \
	test-tool/0230_write12_simple.c test-tool/0231_write12_wrprotect.c \
	test-tool/0232_write12_flags.c  test-tool/0233_write12_0blocks.c   \
	test-tool/0234_write12_beyondeol.c \
	test-tool/0240_prefetch10_simple.c \
	test-tool/0241_prefetch10_flags.c \
	test-tool/0242_prefetch10_beyondeol.c \
	test-tool/0243_prefetch10_0blocks.c \
	test-tool/0250_prefetch16_simple.c \
	test-tool/0251_prefetch16_flags.c \
	test-tool/0252_prefetch16_beyondeol.c \
	test-tool/0253_prefetch16_0blocks.c \
	test-tool/0260_get_lba_status_simple.c test-tool/0264_get_lba_status_beyondeol.c \
	test-tool/0270_verify16_simple.c test-tool/0271_verify16_mismatch.c \
	test-tool/0272_verify16_mismatch_no_cmp.c test-tool/0273_verify16_beyondeol.c \
	test-tool/0280_verify12_simple.c test-tool/0281_verify12_mismatch.c \
	test-tool/0282_verify12_mismatch_no_cmp.c test-tool/0283_verify12_beyondeol.c \
	test-tool/0290_write10_simple.c test-tool/0291_write10_wrprotect.c \
	test-tool/0292_write10_flags.c  test-tool/0293_write10_0blocks.c   \
	test-tool/0294_write10_beyondeol.c \
	test-tool/0300_readonly.c \
	test-tool/0310_writeverify10_simple.c test-tool/0311_writeverify10_wrprotect.c \
	test-tool/0314_writeverify10_beyondeol.c \
	test-tool/0320_writeverify12_simple.c test-tool/0321_writeverify12_wrprotect.c \
	test-tool/0324_writeverify12_beyondeol.c \
	test-tool/0330_writeverify16_simple.c test-tool/0331_writeverify16_wrprotect.c \
	test-tool/0334_writeverify16_beyondeol.c \
	test-tool/0340_compareandwrite_simple.c test-tool/0341_compareandwrite_mismatch.c \
	test-tool/0343_compareandwrite_beyondeol.c \
	test-tool/0350_orwrite_simple.c \
	test-tool/0351_orwrite_wrprotect.c \
	test-tool/0354_orwrite_beyondeol.c \
	test-tool/0360_startstopunit_simple.c \
	test-tool/0361_startstopunit_pwrcnd.c \
	test-tool/0362_startstopunit_noloej.c \
	test-tool/0370_nomedia.c \
	test-tool/0380_preventallow_simple.c \
	test-tool/0381_preventallow_eject.c \
	test-tool/0382_preventallow_itnexus_loss.c \
	test-tool/0383_preventallow_target_warm_reset.c \
	test-tool/0384_preventallow_target_cold_reset.c \
	test-tool/0385_preventallow_lun_reset.c \
	test-tool/0386_preventallow_2_it_nexuses.c \
	test-tool/0390_mandatory_opcodes_sbc.c \
	test-tool/0400_inquiry_basic.c \
	test-tool/0401_inquiry_alloclen.c \
	test-tool/0402_inquiry_evpd.c \
	test-tool/0403_inquiry_supported_vpd.c \
	test-tool/0404_inquiry_all_reported_vpd.c \
	test-tool/0410_readtoc_basic.c \
	test-tool/0420_reserve6_simple.c \
	test-tool/0421_reserve6_lun_reset.c \
	test-tool/0422_reserve6_logout.c \
	test-tool/0423_reserve6_sessionloss.c \
	test-tool/0424_reserve6_target_reset.c \
	test-tool/0430_report_all_supported_ops.c \
	\
	test-tool/1000_cmdsn_invalid.c \
	test-tool/1010_datasn_invalid.c \
	test-tool/1020_bufferoffset_invalid.c \
	test-tool/1030_unsolicited_data_overflow.c \
	test-tool/1031_unsolicited_data_out.c \
	test-tool/1040_saturate_maxcmdsn.c \
	test-tool/1041_unsolicited_immediate_data.c \
	test-tool/1042_unsolicited_nonimmediate_data.c \
	test-tool/1100_persistent_reserve_in_read_keys_simple.c \
	test-tool/1110_persistent_reserve_in_serviceaction_range.c \
	test-tool/1120_persistent_register_simple.c \
	test-tool/1130_persistent_reserve_simple.c \
	test-tool/1140_persistent_reserve_access_check_ea.c \
	test-tool/1141_persistent_reserve_access_check_we.c \
	test-tool/1142_persistent_reserve_access_check_earo.c \
	test-tool/1143_persistent_reserve_access_check_wero.c \
	test-tool/1144_persistent_reserve_access_check_eaar.c \
	test-tool/1145_persistent_reserve_access_check_wear.c


if ISCSITEST
# libiscsi test tool using cunit

bin_PROGRAMS += bin/iscsi-test-cu
# dist_noinst_HEADERS += test-tool/iscsi-test-cu.h
bin_iscsi_test_cu_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/test-tool
bin_iscsi_test_cu_LDFLAGS = -ldl -lcunit
bin_iscsi_test_cu_SOURCES = test-tool/iscsi-test-cu.c \
	test-tool/iscsi-support.c \
	test-tool/test_compareandwrite_simple.c \
	test-tool/test_compareandwrite_miscompare.c \
	test-tool/test_get_lba_status_simple.c \
	test-tool/test_get_lba_status_beyond_eol.c \
	test-tool/test_inquiry_alloc_length.c \
	test-tool/test_inquiry_block_limits.c \
	test-tool/test_inquiry_evpd.c \
	test-tool/test_inquiry_mandatory_vpd_sbc.c \
	test-tool/test_inquiry_standard.c \
	test-tool/test_inquiry_supported_vpd.c \
	test-tool/test_inquiry_version_descriptors.c \
	test-tool/test_iscsi_cmdsn_toohigh.c \
	test-tool/test_iscsi_cmdsn_toolow.c \
	test-tool/test_iscsi_datasn_invalid.c \
	test-tool/test_mandatory_sbc.c \
	test-tool/test_modesense6_all_pages.c \
	test-tool/test_modesense6_residuals.c \
	test-tool/test_nomedia_sbc.c \
	test-tool/test_orwrite_simple.c \
	test-tool/test_orwrite_beyond_eol.c \
	test-tool/test_orwrite_0blocks.c \
	test-tool/test_orwrite_wrprotect.c \
	test-tool/test_orwrite_flags.c \
	test-tool/test_orwrite_verify.c \
	test-tool/test_prefetch10_simple.c \
	test-tool/test_prefetch10_beyond_eol.c \
	test-tool/test_prefetch10_0blocks.c \
	test-tool/test_prefetch10_flags.c \
	test-tool/test_prefetch16_simple.c \
	test-tool/test_prefetch16_beyond_eol.c \
	test-tool/test_prefetch16_0blocks.c \
	test-tool/test_prefetch16_flags.c \
	test-tool/test_preventallow_simple.c \
	test-tool/test_preventallow_eject.c \
	test-tool/test_preventallow_itnexus_loss.c \
	test-tool/test_preventallow_logout.c \
	test-tool/test_preventallow_warm_reset.c \
	test-tool/test_preventallow_cold_reset.c \
	test-tool/test_preventallow_lun_reset.c \
	test-tool/test_preventallow_2_itnexuses.c \
	test-tool/test_prin_read_keys_simple.c \
	test-tool/test_prin_serviceaction_range.c \
	test-tool/test_prout_register_simple.c \
	test-tool/test_prout_reserve_simple.c \
	test-tool/test_prout_reserve_access.c \
	test-tool/test_prout_reserve_ownership.c \
	test-tool/test_read6_simple.c \
	test-tool/test_read6_beyond_eol.c \
	test-tool/test_read10_simple.c \
	test-tool/test_read10_beyond_eol.c \
	test-tool/test_read10_0blocks.c \
	test-tool/test_read10_rdprotect.c \
	test-tool/test_read10_flags.c \
	test-tool/test_read10_residuals.c \
	test-tool/test_read10_invalid.c \
	test-tool/test_read12_simple.c \
	test-tool/test_read12_beyond_eol.c \
	test-tool/test_read12_0blocks.c \
	test-tool/test_read12_rdprotect.c \
	test-tool/test_read12_flags.c \
	test-tool/test_read12_residuals.c \
	test-tool/test_read16_simple.c \
	test-tool/test_read16_beyond_eol.c \
	test-tool/test_read16_0blocks.c \
	test-tool/test_read16_rdprotect.c \
	test-tool/test_read16_flags.c \
	test-tool/test_read16_residuals.c \
	test-tool/test_readcapacity10_simple.c \
	test-tool/test_readcapacity16_alloclen.c \
	test-tool/test_readcapacity16_protection.c \
	test-tool/test_readcapacity16_simple.c \
	test-tool/test_readonly_sbc.c \
	test-tool/test_report_supported_opcodes_one_command.c \
	test-tool/test_report_supported_opcodes_rctd.c \
	test-tool/test_report_supported_opcodes_servactv.c \
	test-tool/test_report_supported_opcodes_simple.c \
	test-tool/test_reserve6_simple.c \
	test-tool/test_reserve6_2initiators.c \
	test-tool/test_reserve6_logout.c \
	test-tool/test_reserve6_itnexus_loss.c \
	test-tool/test_reserve6_target_warm_reset.c \
	test-tool/test_reserve6_target_cold_reset.c \
	test-tool/test_reserve6_lun_reset.c \
	test-tool/test_sanitize_block_erase.c \
	test-tool/test_sanitize_block_erase_reserved.c \
	test-tool/test_sanitize_crypto_erase.c \
	test-tool/test_sanitize_crypto_erase_reserved.c \
	test-tool/test_sanitize_exit_failure_mode.c \
	test-tool/test_sanitize_invalid_serviceaction.c \
	test-tool/test_sanitize_overwrite.c \
	test-tool/test_sanitize_overwrite_reserved.c \
	test-tool/test_sanitize_readonly.c \
	test-tool/test_sanitize_reservations.c \
	test-tool/test_sanitize_reset.c \
	test-tool/test_startstopunit_simple.c \
	test-tool/test_startstopunit_pwrcnd.c \
	test-tool/test_startstopunit_noloej.c \
	test-tool/test_testunitready_simple.c \
	test-tool/test_unmap_simple.c \
	test-tool/test_unmap_0blocks.c \
	test-tool/test_unmap_vpd.c \
	test-tool/test_verify10_simple.c \
	test-tool/test_verify10_beyond_eol.c \
	test-tool/test_verify10_0blocks.c \
	test-tool/test_verify10_vrprotect.c \
	test-tool/test_verify10_flags.c \
	test-tool/test_verify10_mismatch.c \
	test-tool/test_verify10_mismatch_no_cmp.c \
	test-tool/test_verify12_simple.c \
	test-tool/test_verify12_beyond_eol.c \
	test-tool/test_verify12_0blocks.c \
	test-tool/test_verify12_vrprotect.c \
	test-tool/test_verify12_flags.c \
	test-tool/test_verify12_mismatch.c \
	test-tool/test_verify12_mismatch_no_cmp.c \
	test-tool/test_verify16_simple.c \
	test-tool/test_verify16_beyond_eol.c \
	test-tool/test_verify16_0blocks.c \
	test-tool/test_verify16_vrprotect.c \
	test-tool/test_verify16_flags.c \
	test-tool/test_verify16_mismatch.c \
	test-tool/test_verify16_mismatch_no_cmp.c \
	test-tool/test_write10_simple.c \
	test-tool/test_write10_beyond_eol.c \
	test-tool/test_write10_0blocks.c \
	test-tool/test_write10_wrprotect.c \
	test-tool/test_write10_flags.c \
	test-tool/test_write10_residuals.c \
	test-tool/test_write12_simple.c \
	test-tool/test_write12_beyond_eol.c \
	test-tool/test_write12_0blocks.c \
	test-tool/test_write12_wrprotect.c \
	test-tool/test_write12_flags.c \
	test-tool/test_write12_residuals.c \
	test-tool/test_write16_simple.c \
	test-tool/test_write16_beyond_eol.c \
	test-tool/test_write16_0blocks.c \
	test-tool/test_write16_wrprotect.c \
	test-tool/test_write16_flags.c \
	test-tool/test_write16_residuals.c \
	test-tool/test_writesame10_simple.c \
	test-tool/test_writesame10_beyond_eol.c \
	test-tool/test_writesame10_0blocks.c \
	test-tool/test_writesame10_wrprotect.c \
	test-tool/test_writesame10_unmap.c \
	test-tool/test_writesame10_unmap_unaligned.c \
	test-tool/test_writesame10_unmap_until_end.c \
	test-tool/test_writesame10_unmap_vpd.c \
	test-tool/test_writesame16_simple.c \
	test-tool/test_writesame16_beyond_eol.c \
	test-tool/test_writesame16_0blocks.c \
	test-tool/test_writesame16_wrprotect.c \
	test-tool/test_writesame16_unmap.c \
	test-tool/test_writesame16_unmap_unaligned.c \
	test-tool/test_writesame16_unmap_until_end.c \
	test-tool/test_writesame16_unmap_vpd.c \
	test-tool/test_writeverify10_simple.c \
	test-tool/test_writeverify10_beyond_eol.c \
	test-tool/test_writeverify10_0blocks.c \
	test-tool/test_writeverify10_wrprotect.c \
	test-tool/test_writeverify10_flags.c \
	test-tool/test_writeverify10_residuals.c \
	test-tool/test_writeverify12_simple.c \
	test-tool/test_writeverify12_beyond_eol.c \
	test-tool/test_writeverify12_0blocks.c \
	test-tool/test_writeverify12_wrprotect.c \
	test-tool/test_writeverify12_flags.c \
	test-tool/test_writeverify12_residuals.c \
	test-tool/test_writeverify16_simple.c \
	test-tool/test_writeverify16_beyond_eol.c \
	test-tool/test_writeverify16_0blocks.c \
	test-tool/test_writeverify16_wrprotect.c \
	test-tool/test_writeverify16_flags.c \
	test-tool/test_writeverify16_residuals.c

endif

# LD_PRELOAD library.

if LD_ISCSI
EXTRA_PROGRAMS += bin/ld_iscsi
CLEANFILES += bin/ld_iscsi.o bin/ld_iscsi.so

# This gets a bit messy:
#
# 1) let automake compile the sources
bin_ld_iscsi_SOURCES = src/ld_iscsi.c
bin_ld_iscsi_CFLAGS = $(AM_CFLAGS) -fPIC

# 2) let libtool link in the static version of the library
noinst_LTLIBRARIES = lib/libiscsi_convenience.la
lib_libiscsi_convenience_la_SOURCES = $(lib_libiscsi_la_SOURCES)
bin/ld_iscsi.o: src/bin_ld_iscsi-ld_iscsi.o lib/libiscsi_convenience.la
	$(LIBTOOL) --mode=link $(CC) -o $@ $^

# 3) Manually create the .so file.
bin_SCRIPTS = bin/ld_iscsi.so
bin/ld_iscsi.so: bin/ld_iscsi.o
	$(CC) -shared -o bin/ld_iscsi.so bin/ld_iscsi.o -ldl
endif

doc/iscsi-ls.1: doc/iscsi-ls.1.xml
	-test -z "$(XSLTPROC)" || $(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

doc/iscsi-inq.1: doc/iscsi-inq.1.xml
	-test -z "$(XSLTPROC)" || $(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

doc/iscsi-swp.1: doc/iscsi-swp.1.xml
	-test -z "$(XSLTPROC)" || $(XSLTPROC) -o $@ http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

