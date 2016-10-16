AM_CPPFLAGS=-I. -I${srcdir}/../include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = ../lib/libiscsi.la

noinst_PROGRAMS = iscsiclient iscsi-dd


#
# LD_PRELOAD library.
#
if LD_ISCSI
EXTRA_PROGRAMS = ld_iscsi
CLEANFILES = ld_iscsi.o ld_iscsi.so lib/*

#
# This gets a bit messy:
#
# 1) let automake compile the sources
ld_iscsi_SOURCES = ld_iscsi.c
ld_iscsi_CFLAGS = $(AM_CFLAGS) -fPIC

# 2) let libtool link in the static version of the library
noinst_LTLIBRARIES = lib/libiscsi_convenience.la
lib_libiscsi_convenience_la_SOURCES = \
	../lib/connect.c ../lib/iscsi-command.c ../lib/nop.c \
	../lib/sync.c ../lib/crc32c.c ../lib/logging.c ../lib/pdu.c \
	../lib/task_mgmt.c ../lib/discovery.c ../lib/login.c \
	../lib/scsi-lowlevel.c ../lib/init.c ../lib/md5.c \
	../lib/socket.c

ld_iscsi.o: ld_iscsi-ld_iscsi.o lib/libiscsi_convenience.la
	$(LIBTOOL) --mode=link $(CC) -o $@ $^

# 3) Manually create the .so file.
bin_SCRIPTS = ld_iscsi.so
ld_iscsi.so: ld_iscsi.o
	$(CC) -shared -o ld_iscsi.so ld_iscsi.o -ldl
endif

