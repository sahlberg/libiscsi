# test-tool needs to access private symbols, so build a noinst convenience
# library.
noinst_LTLIBRARIES = libiscsipriv.la

libiscsipriv_la_SOURCES = \
	connect.c crc32c.c discovery.c init.c \
	login.c nop.c pdu.c iscsi-command.c \
	scsi-lowlevel.c socket.c sync.c task_mgmt.c \
	logging.c

if !HAVE_LIBGCRYPT
libiscsipriv_la_SOURCES += md5.c
endif

if HAVE_LINUX_ISER
libiscsipriv_la_SOURCES += iser.c
endif

if HAVE_LINUX_ISER
libiscsipriv_la_LIBADD = -libverbs -lrdmacm -lpthread
endif

libiscsipriv_la_LDFLAGS = -no-undefined

libiscsipriv_la_CPPFLAGS = -I${srcdir}/../include -I$(srcdir)/include

AM_CFLAGS=$(WARN_CFLAGS)

# The proper library just pulls in the convenience library and explicitly
# specifies exported symbols.
lib_LTLIBRARIES = libiscsi.la

libiscsi_la_SOURCES =
libiscsi_la_LIBADD = libiscsipriv.la

SOCURRENT=9
SOREVISON=0
SOAGE=0
libiscsi_la_LDFLAGS = \
	-version-info $(SOCURRENT):$(SOREVISON):$(SOAGE) -bindir $(bindir) \
	-no-undefined -export-symbols ${srcdir}/libiscsi.syms

dist_noinst_DATA = libiscsi.syms libiscsi.def
