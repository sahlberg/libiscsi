AM_CPPFLAGS=-I. -I${srcdir}/../include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS=$(WARN_CFLAGS)
LDADD = ../lib/libiscsi.la

noinst_PROGRAMS = iscsiclient iscsi-dd
