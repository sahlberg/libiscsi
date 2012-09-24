# Generic definitions

ACLOCAL_AMFLAGS =-I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS=-I. -I$(srcdir)/include "-D_U_=__attribute__((unused))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = lib/libiscsi.la -lpopt

EXTRA_DIST = autogen.sh COPYING LICENCE-GPL-2.txt LICENCE-LGPL-2.1.txt \
	     packaging/RPM/libiscsi.spec.in packaging/RPM/makerpms.sh

# Simplify conditions below by declaring variables as empty

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
	lib/login.c lib/md5.c lib/nop.c lib/pdu.c lib/scsi-command.c \
	lib/scsi-lowlevel.c lib/socket.c lib/sync.c lib/task_mgmt.c

SONAME=$(firstword $(subst ., ,$(VERSION)))
SOREL=$(shell printf "%d%02d%02d" $(subst ., ,$(VERSION)))
lib_libiscsi_la_LDFLAGS = \
	-version-info $(SONAME):$(SOREL):0 -bindir $(bindir) -no-undefined \
	-export-symbols $(srcdir)/lib/libiscsi.syms

# libiscsi utilities

if PROGRAMS
bin_PROGRAMS += bin/iscsi-inq bin/iscsi-ls
bin_iscsi_inq_SOURCES = src/iscsi-inq.c
bin_iscsi_ls_SOURCES = src/iscsi-ls.c

# Other examples

noinst_PROGRAMS += bin/iscsiclient
bin_iscsiclient_SOURCES = examples/iscsiclient.c

EXTRA_PROGRAMS += bin/iscsi-dd
bin_iscsi_dd_SOURCES = examples/iscsi-dd.c

# libiscsi test tool

noinst_PROGRAMS += bin/iscsi-test
dist_noinst_HEADERS += test-tool/iscsi-test.h
bin_iscsi_test_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/test-tool
bin_iscsi_test_LDFLAGS = -ldl
bin_iscsi_test_SOURCES = test-tool/iscsi-test.c \
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
	\
	test-tool/1000_cmdsn_invalid.c \
	test-tool/1010_datasn_invalid.c \
	test-tool/1020_bufferoffset_invalid.c

endif

# LD_PRELOAD library.

if LD_ISCSI
EXTRA_PROGRAMS += bin/ld_iscsi
CLEANFILES = bin/ld_iscsi.o bin/ld_iscsi.so

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
