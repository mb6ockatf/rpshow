AC_PREREQ([2.71])
AC_INIT([rpshow], [0.1], [smokenmirror@tutamail.com])
AC_CONFIG_SRCDIR([src/rpshow.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.16 gnits subdir-objects -Wall -Werror])
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
AC_CHECK_FUNCS_ONCE([getopt_long])
AS_IF([test "x$ac_cv_func_getopt_long" != xyes],
      [AC_MSG_ERROR([getopt_long is required])])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
