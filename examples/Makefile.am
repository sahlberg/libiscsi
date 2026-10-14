AM_CPPFLAGS=-I. -I${srcdir}/../include -I${srcdir}/../win32
AM_CFLAGS=$(WARN_CFLAGS)
AM_LDFLAGS=-no-undefined
LIBS=../lib/libiscsi.la

noinst_PROGRAMS = iscsiclient iscsi-dd

# The pthreads examples use POSIX signals and alarm().
if !TARGET_OS_IS_WIN32
noinst_PROGRAMS += iscsi-pthreads-inq iscsi-pthreads-readloop iscsi-pthreads-readloop-async
endif
