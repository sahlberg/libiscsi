# Generic definitions
SUBDIRS = lib doc utils test-tool

CLEANFILES=
ACLOCAL_AMFLAGS =-I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS=-I. -I$(srcdir)/include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = lib/libiscsi.la

EXTRA_DIST = autogen.sh COPYING LICENCE-GPL-2.txt LICENCE-LGPL-2.1.txt \
	     packaging/RPM/libiscsi.spec.in packaging/RPM/makerpms.sh \
	     aros/aros_compat.c aros/aros_compat.h aros/iscsi-ls.c \
	     aros/Makefile.AROS \
	     win32/vsbuild.bat win32/win32_compat.c win32/win32_compat.h \
	     lib/libiscsi.def \
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

# Other examples
noinst_PROGRAMS += bin/iscsiclient
bin_iscsiclient_SOURCES = examples/iscsiclient.c

EXTRA_PROGRAMS += bin/iscsi-dd
bin_iscsi_dd_SOURCES = examples/iscsi-dd.c

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

