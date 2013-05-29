dnl Process this file with autoconf to produce a configure script.
dnl
dnl This file is free software; as a special exception the author gives
dnl unlimited permission to copy and/or distribute it, with or without
dnl modifications, as long as this notice is preserved.
dnl
dnl This program is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
dnl implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

AC_INIT([HAL Flash], [0.2.0rc1], [cshorler@googlemail.com])

# Minimum Autoconf version required.
AC_PREREQ(2.60)

# Where to generate output; srcdir location.
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/libhal.c])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall -Wno-extra-portability -Werror foreign])

dbus_module="dbus-1 >= 0.61"

# libtool versioning - this applies to libhal and libhal-storage
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
LT_CURRENT=1
LT_REVISION=0
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Checks for programs.
# We need a C compiler.
AC_PROG_CC
AC_HEADER_ASSERT

LT_INIT([disable-static])

# D-Bus libs
PKG_CHECK_MODULES([DBUS], [$dbus_module])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 test/Makefile])
AC_OUTPUT
