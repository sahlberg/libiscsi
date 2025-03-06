AC_PREREQ([2.69])
AC_INIT([libiscsi], m4_esyscmd([grep 'Version:' ./packaging/RPM/libiscsi.spec.in 2>/dev/null | head -1 | sed -e 's/[ \t]*Version:[ \t]*\([^ \t]*\)[ \t]*.*/\1/' | tr -d '\n']))
AC_CONFIG_SRCDIR([lib/init.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])
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
  *mingw*|*msys*)
    target_os_is_win32=true
    ;;
  *)
    ;;
esac

AM_CONDITIONAL([TARGET_OS_IS_WIN32], [test x$target_os_is_win32 = xtrue])

AC_ARG_ENABLE([werror], [AS_HELP_STRING([--disable-werror],
              [Disables building with -Werror by default])])

if test "$ac_cv_c_compiler_gnu" = yes; then
   WARN_CFLAGS="-Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wcast-qual -Wvla"
   WARN_CFLAGS="$WARN_CFLAGS -Wno-unknown-warning-option -Wno-stringop-truncation"
   WARN_CFLAGS="$WARN_CFLAGS -Wno-unused-parameter"
   if test "x$enable_werror" != "xno"; then
       WARN_CFLAGS="$WARN_CFLAGS -Werror"
   fi
   if test "x$enable_write_strings" != "xno"; then
       WARN_CFLAGS="$WARN_CFLAGS -Wwrite-strings"
   fi
fi
AC_SUBST(WARN_CFLAGS)

AC_ARG_ENABLE([manpages],
              [AS_HELP_STRING([--enable-manpages],
                              [Enable building the manpages])],
	      [ENABLE_MANPAGES=$enableval],
	      [ENABLE_MANPAGES="no"])
AM_CONDITIONAL(BUILD_MANPAGES,
               [expr "$ENABLE_MANPAGES" : yes > /dev/null 2>&1])

AC_ARG_ENABLE([test_tool],
              [AS_HELP_STRING([--enable-test-tool],
                              [Enable building test-tool (to test a remote server)])],
              [ENABLE_TEST_TOOL=$enableval],
              [ENABLE_TEST_TOOL=yes])
AM_CONDITIONAL([BUILD_TEST_TOOL],
               [expr "$ENABLE_TEST_TOOL" : yes > /dev/null 2>&1])

AC_ARG_ENABLE([tests],
              [AS_HELP_STRING([--enable-tests],
                              [Enable building tests])],
              [ENABLE_TESTS=$enableval],
              [ENABLE_TESTS=yes])
AM_CONDITIONAL([BUILD_TESTS],
               [expr "$ENABLE_TESTS" : yes > /dev/null 2>&1])

AC_ARG_ENABLE([examples],
              [AS_HELP_STRING([--enable-examples],
                              [Enable building examples])],
              [ENABLE_EXAMPLES=$enableval],
              [ENABLE_EXAMPLES=yes])
AM_CONDITIONAL([BUILD_EXAMPLES],
               [expr "$ENABLE_EXAMPLES" : yes > /dev/null 2>&1])

AC_CONFIG_HEADERS([config.h])

AC_ARG_WITH([gnutls],
	    [AS_HELP_STRING([--with-gnutls],
			    [Use gnutls to compute MD5])],
	    [WITH_GNUTLS=$withval],
	    [WITH_GNUTLS=auto])

AC_ARG_WITH([libgcrypt],
	    [AS_HELP_STRING([--with-libgcrypt],
			    [Use libgcrypt to compute MD5])],
	    [WITH_LIBGCRYPT=$withval],
	    [WITH_LIBGCRYPT=auto])

if test "$WITH_GNUTLS" != no; then
  AC_CHECK_LIB([gnutls], [gnutls_hash_init])
  if test "$WITH_GNUTLS" = yes && test "$ac_cv_lib_gnutls_gnutls_hash_init" != yes; then
    AC_MSG_ERROR([gnutls not found])
  fi
  WITH_GNUTLS=$ac_cv_lib_gnutls_gnutls_hash_init
fi
if test "$WITH_GNUTLS" = yes; then
  WITH_LIBGCRYPT=no
fi

if test "$WITH_LIBGCRYPT" != no; then
  AC_CHECK_LIB([gcrypt], [gcry_control])
  if test "$WITH_LIBGCRYPT" = yes && test "$ac_cv_lib_gcrypt_gcry_control" != yes; then
    AC_MSG_ERROR([libgcrypt not found])
  fi
  WITH_LIBGCRYPT=$ac_cv_lib_gcrypt_gcry_control
fi

NEED_MD5=no
if test "$WITH_GNUTLS" = no && test "$WITH_LIBGCRYPT" = no; then
  NEED_MD5=yes
fi
AM_CONDITIONAL([NEED_MD5],
	       [expr "$NEED_MD5" : yes > /dev/null 2>&1])

# For MinGW.
AC_CHECK_LIB([ws2_32], [gethostbyname])

AC_CHECK_HEADERS([sys/select.h])
AC_CHECK_HEADERS([winsock.h winsock2.h ws2tcpip.h],,,[#include <windows.h>])

#
# Check for the type of the fifth argument of select() since MinGW uses
# struct __ms_timeval.
#

ac_save_CFLAGS=$CFLAGS
CFLAGS="$CFLAGS -Werror"
AC_MSG_CHECKING([for the type of the fifth argument of select()])
arg_type=void
for t in "struct timeval" "struct __ms_timeval"; do
AC_COMPILE_IFELSE(
    [AC_LANG_PROGRAM([
#if defined(HAVE_WINSOCK2_H) && defined(HAVE_WS2TCPIP_H)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif
#include <stddef.h>
], [return select(0, NULL, NULL, NULL, ($t *)NULL)])],
    [arg_type=$t; break])
done
AC_MSG_RESULT(${arg_type})
AC_DEFINE_UNQUOTED([SELECT_TIMEVAL_TYPE], [${arg_type}],
        [Type of the fifth argument of select()])
CFLAGS=$ac_save_CFLAGS

AC_CHECK_FUNCS([tzset])

AC_CACHE_CHECK([for sin_len in sock],libiscsi_cv_HAVE_SOCK_SIN_LEN,[
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>]],
[[struct sockaddr_in sock;
sock.sin_len = sizeof(sock);]])],
[libiscsi_cv_HAVE_SOCK_SIN_LEN=yes],[libiscsi_cv_HAVE_SOCK_SIN_LEN=no])])
if test x"$libiscsi_cv_HAVE_SOCK_SIN_LEN" = x"yes"; then
    AC_DEFINE(HAVE_SOCK_SIN_LEN,1,[Whether the sockaddr_in struct has a sin_len property])
fi

AC_CACHE_CHECK([for sys filio.h],libiscsi_cv_NEED_SYS_FILIO_H,[
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stdint.h>
#include <sys/filio.h>]],
[[int foo = FIONREAD]])],
[libiscsi_cv_NEED_SYS_FILIO_H=yes],[libiscsi_cv_NEED_SYS_FILIO_H=no])])
if test x"$libiscsi_cv_NEED_SYS_FILIO_H" = x"yes"; then
    AC_DEFINE(NEED_SYS_FILIO_H,1,[Whether we need sys/filio.h])
fi

AC_CHECK_HEADERS(dnl
[arpa/inet.h]	dnl
[netinet/in.h]	dnl
[netinet/tcp.h]	dnl
[poll.h]	dnl
[sys/socket.h]	dnl
[sys/time.h]	dnl
[sys/uio.h]	dnl
)

AC_CACHE_CHECK([for sockaddr_in6 support],libiscsi_cv_HAVE_SOCKADDR_IN6,[
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>]], [[struct sockaddr_in6 sock; int len = sizeof(sock);]])],[libiscsi_cv_HAVE_SOCKADDR_IN6=yes],[libiscsi_cv_HAVE_SOCKADDR_IN6=no])])
if test x"$libiscsi_cv_HAVE_SOCKADDR_IN6" = x"yes"; then
    AC_DEFINE(HAVE_SOCKADDR_IN6,1,[Whether we have IPv6 support])
fi

AC_CACHE_CHECK([for SG_IO support],libiscsi_cv_HAVE_SG_IO,[
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <unistd.h>
#include <scsi/sg.h>]],
[[int sgio = SG_IO;]])],
[libiscsi_cv_HAVE_SG_IO=yes],[libiscsi_cv_HAVE_SG_IO=no])])
if test x"$libiscsi_cv_HAVE_SG_IO" = x"yes"; then
    AC_DEFINE(HAVE_SG_IO,1,[Whether we have SG_IO support])
fi

AC_CACHE_CHECK([for iSER support],libiscsi_cv_HAVE_LINUX_ISER,[
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>]],
[[int ibv = IBV_WC_SEND;]])],
[libiscsi_cv_HAVE_LINUX_ISER=yes],[libiscsi_cv_HAVE_LINUX_ISER=no])])
if test x"$libiscsi_cv_HAVE_LINUX_ISER" = x"yes"; then
    AC_DEFINE(HAVE_LINUX_ISER,1,[Whether we have iSER support])
fi
AM_CONDITIONAL([HAVE_LINUX_ISER], [test $libiscsi_cv_HAVE_LINUX_ISER = yes])

AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <rdma/rdma_cma.h>]], [[return RDMA_OPTION_ID_ACK_TIMEOUT;]])],[AC_DEFINE([HAVE_RDMA_ACK_TIMEOUT],[1],[Define to 1 if you have RDMA ack timeout support])],[])

# check for stdatomic.h
dnl Check for stdatomic.h
AC_CHECK_HEADERS([stdatomic.h])

# check for pthread
AC_CACHE_CHECK([for pthread support],libiscsi_cv_HAVE_PTHREAD,[
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
	#include <pthread.h>]], [[pthread_t thread1, thread2;]])],[libiscsi_cv_HAVE_PTHREAD=yes],[libiscsi_cv_HAVE_PTHREAD=no])])
	if test x"$libiscsi_cv_HAVE_PTHREAD" = x"yes"; then
	   AC_DEFINE(HAVE_PTHREAD,1,[Whether we have pthread support])
	   AC_DEFINE(HAVE_MULTITHREADING,1,[Whether we have multithreading support])
	fi
AM_CONDITIONAL([HAVE_PTHREAD], [test x$libiscsi_cv_HAVE_PTHREAD = xyes])

AC_CACHE_CHECK([whether libcunit is available],
               [ac_cv_have_cunit],
               [ac_save_CFLAGS="$CFLAGS"
                ac_save_LIBS="$LIBS"
                CFLAGS="$CFLAGS $GLIB_CFLAGS"
                LIBS="$GLIB_LIBS $LIBS -lcunit"
                AC_LINK_IFELSE([AC_LANG_PROGRAM([[
                    #include <CUnit/CUnit.h>
                    ]], [[]])],[ac_cv_have_cunit=yes],[ac_cv_have_cunit=no])
                CFLAGS="$ac_save_CFLAGS"
                LIBS="$ac_save_LIBS"])
if ! test "$ac_cv_have_cunit" = yes ; then
  AC_MSG_NOTICE(You need libcunit to build the test suite.)
  AC_MSG_NOTICE(The scsi/iscsi test suite will not be built.)
fi
AM_CONDITIONAL(ISCSITEST, [test "$ac_cv_have_cunit" = yes -a "$enable_shared" = "yes"])

AC_CHECK_MEMBER([struct timezone.tz_dsttime],
                [AC_DEFINE([HAVE_TIMEZONE_TZ_DSTTIME], [1],
                   [Define this macro if struct timezone has tz_dsttime])], [],
                [#include <sys/time.h>])

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
