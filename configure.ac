#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([xrestrict], [0.8.1], [ademan555@gmail.com])
AC_CONFIG_SRCDIR([src/xrestrict.c])
AC_CONFIG_HEADERS([src/config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(XRANDR, xrandr)
PKG_CHECK_MODULES(XINPUT, xext [xi >= 1.2.99.2] [inputproto >= 1.9.99.15])

AC_SUBST([X11_CFLAGS])
AC_SUBST([X11_LIBS])
AC_SUBST([XRANDR_CFLAGS])
AC_SUBST([XRANDR_LIBS])
AC_SUBST([XINPUT_CFLAGS])
AC_SUBST([XINPUT_LIBS])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE

# Checks for library functions.

AC_OUTPUT(Makefile src/Makefile)
