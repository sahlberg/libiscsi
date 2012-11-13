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
	lib/login.c lib/md5.c lib/nop.c lib/pdu.c lib/iscsi-command.c \
	lib/scsi-lowlevel.c lib/socket.c lib/sync.c lib/task_mgmt.c \
	lib/logging.c

SONAME=$(firstword $(subst ., ,$(VERSION)))
SOREL=$(shell printf "%d%02d%02d" $(subst ., ,$(VERSION)))
lib_libiscsi_la_LDFLAGS = \
	-version-info $(SONAME):$(SOREL):0 -bindir $(bindir) -no-undefined \
	-export-symbols $(srcdir)/lib/libiscsi.syms

# libiscsi utilities

if PROGRAMS
bin_PROGRAMS += bin/iscsi-inq bin/iscsi-ls bin/iscsi-readcapacity16
bin_iscsi_inq_SOURCES = src/iscsi-inq.c
bin_iscsi_ls_SOURCES = src/iscsi-ls.c
bin_iscsi_readcapacity16_SOURCES = src/iscsi-readcapacity16.c
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
