AC_PREREQ(2.50)
AC_INIT(libiscsi, m4_esyscmd([grep 'Version:' ./packaging/RPM/libiscsi.spec.in 2>/dev/null | head -1 | sed -e 's/[ \t]*Version:[ \t]*\([^ \t]*\)[ \t]*.*/\1/' | tr -d '\n']))
AC_CONFIG_SRCDIR([lib/init.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
AC_PROG_LIBTOOL
LT_INIT

AC_CANONICAL_HOST
AM_PROG_CC_C_O

enable_write_strings="yes"

case $host in
  *solaris*)
    enable_write_strings="no"
    AC_CHECK_LIB([socket], [main], , [AC_MSG_ERROR([Can not find required library])])
    AC_CHECK_LIB([nsl],    [main], , [AC_MSG_ERROR([Can not find required library])])
    ;;
  *)
    ;;
esac

AC_ARG_ENABLE([werror], [AS_HELP_STRING([--disable-werror],
              [Disables building with -Werror by default])])

if test "$ac_cv_prog_gcc" = yes; then
   WARN_CFLAGS="-Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wno-strict-aliasing"
   WARN_CFLAGS="$WARN_CFLAGS -Wno-unknown-warning-option -Wno-stringop-truncation"
   if test "x$enable_werror" != "xno"; then
       WARN_CFLAGS="$WARN_CFLAGS -Werror"
   fi
   if test "x$enable_write_strings" != "xno"; then
       WARN_CFLAGS="$WARN_CFLAGS -Wwrite-strings"
   fi
fi
AC_SUBST(WARN_CFLAGS)

AC_ARG_ENABLE([manpages],
              [AC_HELP_STRING([--enable-manpages],
                              [Enable building the manpages])],
	      [ENABLE_MANPAGES=$enableval],
	      [ENABLE_MANPAGES="no"])
AM_CONDITIONAL(BUILD_MANPAGES,
               [expr "$ENABLE_MANPAGES" : yes > /dev/null 2>&1])

AC_ARG_ENABLE([test_tool],
              [AC_HELP_STRING([--enable-test-tool],
                              [Enable building test-tool (to test a remote server)])],
              [ENABLE_TEST_TOOL=$enableval],
              [ENABLE_TEST_TOOL=yes])
AM_CONDITIONAL([BUILD_TEST_TOOL],
               [expr "$ENABLE_TEST_TOOL" : yes > /dev/null 2>&1])

AC_ARG_ENABLE([tests],
              [AC_HELP_STRING([--enable-tests],
                              [Enable building tests])],
              [ENABLE_TESTS=$enableval],
              [ENABLE_TESTS=yes])
AM_CONDITIONAL([BUILD_TESTS],
               [expr "$ENABLE_TESTS" : yes > /dev/null 2>&1])

AC_ARG_ENABLE([examples],
              [AC_HELP_STRING([--enable-examples],
                              [Enable building examples])],
              [ENABLE_EXAMPLES=$enableval],
              [ENABLE_EXAMPLES=yes])
AM_CONDITIONAL([BUILD_EXAMPLES],
               [expr "$ENABLE_EXAMPLES" : yes > /dev/null 2>&1])

AC_CONFIG_HEADER(config.h)

AC_CHECK_LIB([gcrypt], [gcry_control])
AM_CONDITIONAL([HAVE_LIBGCRYPT], [test $ac_cv_lib_gcrypt_gcry_control = yes])

AC_CACHE_CHECK([for sin_len in sock],libiscsi_cv_HAVE_SOCK_SIN_LEN,[
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>],
[struct sockaddr_in sock; sock.sin_len = sizeof(sock);],
libiscsi_cv_HAVE_SOCK_SIN_LEN=yes,libiscsi_cv_HAVE_SOCK_SIN_LEN=no)])
if test x"$libiscsi_cv_HAVE_SOCK_SIN_LEN" = x"yes"; then
    AC_DEFINE(HAVE_SOCK_SIN_LEN,1,[Whether the sockaddr_in struct has a sin_len property])
fi

AC_CACHE_CHECK([for sys filio.h],libiscsi_cv_NEED_SYS_FILIO_H,[
AC_TRY_COMPILE([#include <stdint.h>
#include <sys/filio.h>],
[int foo = FIONREAD],
libiscsi_cv_NEED_SYS_FILIO_H=yes,libiscsi_cv_NEED_SYS_FILIO_H=no)])
if test x"$libiscsi_cv_NEED_SYS_FILIO_H" = x"yes"; then
    AC_DEFINE(NEED_SYS_FILIO_H,1,[Whether we need sys/filio.h])
fi

# check for sys/socket.h
dnl Check for sys/socket.h
AC_CHECK_HEADERS([sys/socket.h])

# check for netinet/tcp.h
dnl Check for netinet/tcp.h
AC_CHECK_HEADERS([netinet/tcp.h])

# check for netinet/in.h
dnl Check for netinet/in.h
AC_CHECK_HEADERS([netinet/in.h])

# check for arpa/inet.h
dnl Check for arpa/inet.h
AC_CHECK_HEADERS([arpa/inet.h])

# check for sys/uio.h
dnl Check for sys/uio.h
AC_CHECK_HEADERS([sys/uio.h])

# check for poll.h
dnl Check for poll.h
AC_CHECK_HEADERS([poll.h])


AC_CACHE_CHECK([for sockaddr_in6 support],libiscsi_cv_HAVE_SOCKADDR_IN6,[
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>],
[struct sockaddr_in6 sock; int len = sizeof(sock);],
libiscsi_cv_HAVE_SOCKADDR_IN6=yes,libiscsi_cv_HAVE_SOCKADDR_IN6=no)])
if test x"$libiscsi_cv_HAVE_SOCKADDR_IN6" = x"yes"; then
    AC_DEFINE(HAVE_SOCKADDR_IN6,1,[Whether we have IPv6 support])
fi

AC_CACHE_CHECK([for SG_IO support],libiscsi_cv_HAVE_SG_IO,[
AC_TRY_COMPILE([
#include <unistd.h>
#include <scsi/sg.h>],
[int sgio = SG_IO;],
libiscsi_cv_HAVE_SG_IO=yes,libiscsi_cv_HAVE_SG_IO=no)])
if test x"$libiscsi_cv_HAVE_SG_IO" = x"yes"; then
    AC_DEFINE(HAVE_SG_IO,1,[Whether we have SG_IO support])
fi

AC_CACHE_CHECK([for iSER support],libiscsi_cv_HAVE_LINUX_ISER,[
AC_TRY_COMPILE([
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>],
[int ibv = IBV_WC_SEND;],
libiscsi_cv_HAVE_LINUX_ISER=yes,libiscsi_cv_HAVE_LINUX_ISER=no)])
if test x"$libiscsi_cv_HAVE_LINUX_ISER" = x"yes"; then
    AC_DEFINE(HAVE_LINUX_ISER,1,[Whether we have iSER support])
fi
AM_CONDITIONAL([HAVE_LINUX_ISER], [test $libiscsi_cv_HAVE_LINUX_ISER = yes])

AC_CACHE_CHECK([whether libcunit is available],
               [ac_cv_have_cunit],
               [ac_save_CFLAGS="$CFLAGS"
                ac_save_LIBS="$LIBS"
                CFLAGS="$CFLAGS $GLIB_CFLAGS"
                LIBS="$GLIB_LIBS $LIBS -lcunit"
                AC_TRY_LINK([
                    #include <CUnit/CUnit.h>
                    ], [], [ac_cv_have_cunit=yes], [ac_cv_have_cunit=no])
                CFLAGS="$ac_save_CFLAGS"
                LIBS="$ac_save_LIBS"])
if ! test "$ac_cv_have_cunit" = yes ; then
  AC_MSG_NOTICE(You need libcunit to build the test suite.)
  AC_MSG_NOTICE(The scsi/iscsi test suite will not be built.)
fi
AM_CONDITIONAL(ISCSITEST, [test "$ac_cv_have_cunit" = yes -a "$enable_shared" = "yes"])

AM_CONDITIONAL(LD_ISCSI, [expr "(" "$host_os" : "linux" ")" "&" "$enable_shared" "=" "yes"])

AC_CHECK_MEMBER([struct CU_SuiteInfo.pSetUpFunc],
		[AC_DEFINE([HAVE_CU_SUITEINFO_PSETUPFUNC], 1,
		 [Define to 1 if struct CU_SuiteInfo has a member called pSetUpFunc])],
		 [], [[
#include <CUnit/CUnit.h>
#include <CUnit/TestDB.h>
]])

AC_SEARCH_LIBS(clock_gettime, rt, [
	       AC_DEFINE([HAVE_CLOCK_GETTIME],1,[Define if clock_gettime is available])])


AC_CONFIG_FILES([Makefile]
		[doc/Makefile]
		[examples/Makefile]
		[lib/Makefile]
		[test-tool/Makefile]
		[tests/Makefile]
		[utils/Makefile]
		[libiscsi.pc])
AC_OUTPUT
