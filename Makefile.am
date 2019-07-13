# Generic definitions
SUBDIRS = lib doc utils
if BUILD_TEST_TOOL
SUBDIRS += test-tool
endif
if BUILD_TESTS
SUBDIRS += tests
endif
if BUILD_EXAMPLES
SUBDIRS += examples
endif

ACLOCAL_AMFLAGS =-I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS=-I. -I$(srcdir)/include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = lib/libiscsi.la

EXTRA_DIST = autogen.sh COPYING LICENCE-GPL-2.txt LICENCE-LGPL-2.1.txt \
	     packaging/RPM/libiscsi.spec.in packaging/RPM/makerpms.sh \
	     aros/aros_compat.c aros/aros_compat.h aros/iscsi-ls.c \
	     aros/Makefile.AROS test-tool/iscsi-multipath.h \
	     win32/win32_compat.c win32/win32_compat.h \
	     patches/mtx-iscsi.diff patches/README patches/sg3_utils-1.32.patch

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libiscsi.pc

iscsi_includedir = $(includedir)/iscsi
dist_iscsi_include_HEADERS = include/iscsi.h include/scsi-lowlevel.h
dist_noinst_HEADERS = include/iscsi-private.h include/md5.h include/slist.h

