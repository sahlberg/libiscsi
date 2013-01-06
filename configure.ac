AC_PREREQ(2.50)
AC_INIT(libiscsi, m4_esyscmd([grep 'Version:' ./packaging/RPM/libiscsi.spec.in 2>/dev/null | head -1 | sed -e 's/[ \t]*Version:[ \t]*\([^ \t]*\)[ \t]*.*/\1/' | tr -d '\n']))
AC_CONFIG_SRCDIR([lib/init.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE
AM_SILENT_RULES
LT_INIT
if test "${libdir}" = '${exec_prefix}/lib'; then
  case `uname -m` in
    x86_64|ppc64|powerpc64)
      libdir='${exec_prefix}/lib64'
      ;;
    *)
      libdir='${exec_prefix}/lib'
      ;;
  esac
fi

AC_CANONICAL_HOST
AM_CONDITIONAL(LD_ISCSI,
               [expr "$host_os" : linux > /dev/null 2>&1])

if test "$ac_cv_prog_gcc" = yes; then
   WARN_CFLAGS="-Wall -W -Wshadow -Wstrict-prototypes -Wpointer-arith -Wcast-align -Wwrite-strings -Werror -Wno-strict-aliasing"
fi
AC_SUBST(WARN_CFLAGS)

AC_CONFIG_HEADER(config.h)

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

AC_MSG_CHECKING(whether libpopt is available)
ac_save_CFLAGS="$CFLAGS"
ac_save_LIBS="$LIBS"
CFLAGS="$CFLAGS $GLIB_CFLAGS"
LIBS="$GLIB_LIBS $LIBS -lpopt"
AC_TRY_RUN([
/*
 * Just see if we can compile/link with popt
 */
#include <popt.h>

int main(int argc, const char *argv[])
{
	struct poptOption popt_options[] = {
		POPT_TABLEEND
	};
	poptGetContext(argv[0], argc, argv, popt_options, POPT_CONTEXT_KEEP_FIRST);

	return 0;
}
], ac_cv_have_popt=yes, ac_cv_have_popt=no,
   [echo $ac_n "compile with POPT. Assuming OK... $ac_c"
    ac_cv_have_popt=yes])
CFLAGS="$ac_save_CFLAGS"
LIBS="$ac_save_LIBS"
if test "$ac_cv_have_popt" = yes ; then
  AC_MSG_RESULT(yes)
else
  AC_MSG_RESULT(no)
  AC_MSG_NOTICE(You need libpopt to compile the sample libiscsi clients.)
  AC_MSG_NOTICE(Only the library will be compiled and installed.)
fi
AM_CONDITIONAL(PROGRAMS, [test "$ac_cv_have_popt" = yes])

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


AC_CONFIG_FILES(Makefile)
AC_OUTPUT
