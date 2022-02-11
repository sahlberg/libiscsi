AM_CPPFLAGS = -I${srcdir}/../include -I${srcdir}/../win32
AM_CFLAGS = $(WARN_CFLAGS)
AM_LDFLAGS = -no-undefined
LIBS = ../lib/libiscsi.la

bin_PROGRAMS = iscsi-inq iscsi-ls iscsi-swp iscsi-pr
if !TARGET_OS_IS_WIN32
bin_PROGRAMS += iscsi-perf iscsi-readcapacity16
endif
