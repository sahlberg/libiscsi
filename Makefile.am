# Generic definitions

ACLOCAL_AMFLAGS =-I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS=-I. -I$(srcdir)/include "-D_U_=__attribute__((unused))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = lib/libiscsi.la -lpopt

EXTRA_DIST = autogen.sh COPYING.LESSER \
	     packaging/RPM/libiscsi.spec.in packaging/RPM/makerpms.sh

# Simplify conditions below by declaring variables as empty

bin_PROGRAMS =
noinst_PROGRAMS =
EXTRA_PROGRAMS =

# libiscsi shared library

iscsi_includedir = $(includedir)/iscsi
dist_iscsi_include_HEADERS = include/iscsi.h include/scsi-lowlevel.h
dist_noinst_HEADERS = include/iscsi-private.h include/md5.h include/slist.h

lib_LTLIBRARIES = lib/libiscsi.la
lib_libiscsi_la_SOURCES = \
	lib/connect.c lib/crc32c.c lib/discovery.c lib/init.c \
	lib/login.c lib/md5.c lib/nop.c lib/pdu.c lib/scsi-command.c \
	lib/scsi-lowlevel.c lib/socket.c lib/sync.c lib/task_mgmt.c

SONAME=$(firstword $(subst ., ,$(VERSION)))
SOREL=$(shell printf "%d%02d%02d" $(subst ., ,$(VERSION)))
lib_libiscsi_la_LDFLAGS = \
	-version-info $(SONAME):$(SOREL):0 -bindir $(bindir) -no-undefined

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
bin_iscsi_test_SOURCES = test-tool/iscsi-test.c \
	test-tool/0100_read10_simple.c test-tool/0101_read10_beyond_eol.c \
	test-tool/0102_read10_0blocks.c test-tool/0103_read10_rdprotect.c \
	test-tool/0104_read10_flags.c test-tool/0105_read10_invalid.c \
	test-tool/0110_readcapacity10_simple.c \
	test-tool/0111_readcapacity10_pmi.c test-tool/0120_read6_simple.c \
	test-tool/0121_read6_beyond_eol.c test-tool/0122_read6_invalid.c \
	test-tool/0130_verify10_simple.c test-tool/0131_verify10_mismatch.c \
	test-tool/0132_verify10_mismatch_no_cmp.c \
	test-tool/0143_read12_rdprotect.c
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
