lib_LTLIBRARIES = libiscsi.la

libiscsi_la_SOURCES = \
	connect.c crc32c.c discovery.c init.c \
	login.c nop.c pdu.c iscsi-command.c \
	scsi-lowlevel.c socket.c sync.c task_mgmt.c \
	logging.c

if !HAVE_LIBGCRYPT
libiscsi_la_SOURCES += md5.c
endif

SOCURRENT=7
SOREVISON=1
SOAGE=0
libiscsi_la_LDFLAGS = \
	-version-info $(SOCURRENT):$(SOREVISON):$(SOAGE) -bindir $(bindir) \
	-no-undefined -export-symbols ${srcdir}/libiscsi.syms

libiscsi_la_CPPFLAGS = -I${srcdir}/../include -I$(srcdir)/include \
	"-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"

AM_CFLAGS=$(WARN_CFLAGS)

dist_noinst_DATA = libiscsi.syms libiscsi.def

