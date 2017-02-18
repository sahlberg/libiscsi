AM_CPPFLAGS = -I${srcdir}/../include "-D_U_=__attribute__((unused))" \
	"-D_R_(A,B)=__attribute__((format(printf,A,B)))"
AM_CFLAGS = $(WARN_CFLAGS)
LDADD = ../lib/libiscsi.la

noinst_PROGRAMS = prog_reconnect prog_reconnect_timeout prog_noop_reply \
	prog_readwrite_iov prog_timeout prog_read_all_pdus \
	prog_header_digest

T = `ls test_*.sh`

test: $(noinst_PROGRAMS)
	for TEST in $(T); do \
		echo "Running $$TEST"; \
		echo "--------------"; \
		sh $$TEST || exit 1;   \
		echo "--------------"; \
		echo; \
	done
