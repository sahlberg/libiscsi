AC_PREREQ(2.50)
AC_INIT(libiscsi, m4_esyscmd([grep 'Version:' ./packaging/RPM/libiscsi.spec.in 2>/dev/null | head -1 | sed -e 's/[ \t]*Version:[ \t]*\([^ \t]*\)[ \t]*.*/\1/' | tr -d '\n']))
AC_CONFIG_SRCDIR([lib/init.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE
AM_SILENT_RULES
LT_INIT

AC_CANONICAL_HOST
AM_CONDITIONAL(LD_ISCSI,
               [expr "$host_os" : linux > /dev/null 2>&1])

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
   if test "x$enable_werror" != "xno"; then
       WARN_CFLAGS="$WARN_CFLAGS -Werror"
   fi
   if test "x$enable_write_strings" != "xno"; then
       WARN_CFLAGS="$WARN_CFLAGS -Wwrite-strings"
   fi
fi
AC_SUBST(WARN_CFLAGS)

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

# check for arpa/inet.h
dnl Check for arpa/inet.h
AC_CHECK_HEADERS([arpa/inet.h])

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


AC_MSG_CHECKING(whether libcunit is available)
ac_save_CFLAGS="$CFLAGS"
ac_save_LIBS="$LIBS"
CFLAGS="$CFLAGS $GLIB_CFLAGS"
LIBS="$GLIB_LIBS $LIBS -lcunit"
AC_TRY_RUN([
/*
 * Just see if we can compile/link with libcunit
 */
#include <CUnit/CUnit.h>

int main(int argc, const char *argv[])
{
	return 0;
}
], ac_cv_have_cunit=yes, ac_cv_have_cunit=no,
   [echo $ac_n "compile with CUNIT. Assuming OK... $ac_c"
    ac_cv_have_cunit=yes])
CFLAGS="$ac_save_CFLAGS"
LIBS="$ac_save_LIBS"
if test "$ac_cv_have_cunit" = yes ; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  AC_MSG_NOTICE(You need libcunit to build the test suite.)
  AC_MSG_NOTICE(The scsi/iscsi test suite will not be built.)
fi
AM_CONDITIONAL(ISCSITEST, [test "$ac_cv_have_cunit" = yes])


AC_CHECK_MEMBER([struct CU_SuiteInfo.pSetUpFunc],
		[AC_DEFINE([HAVE_CU_SUITEINFO_PSETUPFUNC], 1,
		 [Define to 1 if struct CU_SuiteInfo has a member called pSetUpFunc])],
		 [], [[
#include <CUnit/CUnit.h>
#include <CUnit/TestDB.h>
]])


AC_CONFIG_FILES(Makefile libiscsi.pc)
AC_OUTPUT
