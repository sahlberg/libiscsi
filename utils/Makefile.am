AM_CPPFLAGS = -I${srcdir}/../include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS = $(WARN_CFLAGS)
LDADD = ../lib/libiscsi.la

bin_PROGRAMS = iscsi-inq iscsi-ls iscsi-perf iscsi-readcapacity16 \
	iscsi-swp

