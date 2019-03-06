AM_CPPFLAGS=-I. -I${srcdir}/../include \
	"-D_U_=__attribute__((unused)) " \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = ../lib/libiscsi.la

EXTRA_DIST = README

dist_noinst_HEADERS = iscsi-support.h \
		      iscsi-test-cu.h iscsi-multipath.h

# libiscsi test tool using cunit
bin_PROGRAMS = iscsi-test-cu
iscsi_test_cu_LDFLAGS = -ldl -lcunit
iscsi_test_cu_SOURCES = iscsi-test-cu.c \
	iscsi-support.c \
	iscsi-multipath.c \
	test_read6_simple.c \
	test_read6_beyond_eol.c \
	test_read10_simple.c \
	test_read10_beyond_eol.c \
	test_read10_0blocks.c \
	test_read10_rdprotect.c \
	test_read10_dpofua.c \
	test_read10_residuals.c \
	test_read10_invalid.c \
	test_read12_simple.c \
	test_read12_beyond_eol.c \
	test_read12_0blocks.c \
	test_read12_rdprotect.c \
	test_read12_dpofua.c \
	test_read12_residuals.c \
	test_read16_simple.c \
	test_read16_beyond_eol.c \
	test_read16_0blocks.c \
	test_read16_rdprotect.c \
	test_read16_dpofua.c \
	test_read16_residuals.c \
	test_unmap_simple.c \
	test_unmap_0blocks.c \
	test_unmap_vpd.c \
	test_write10_simple.c \
	test_write10_beyond_eol.c \
	test_write10_0blocks.c \
	test_write10_wrprotect.c \
	test_write10_dpofua.c \
	test_write10_residuals.c \
	test_write12_simple.c \
	test_write12_beyond_eol.c \
	test_write12_0blocks.c \
	test_write12_wrprotect.c \
	test_write12_dpofua.c \
	test_write12_residuals.c \
	test_write16_simple.c \
	test_write16_beyond_eol.c \
	test_write16_0blocks.c \
	test_write16_wrprotect.c \
	test_write16_dpofua.c \
	test_write16_residuals.c \
	test_async_read.c \
	test_async_write.c \
	test_async_abort_simple.c

