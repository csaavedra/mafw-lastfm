AC_INIT([mafw-lastfm], [0.0.1], [mafw-lastfm-devel@garage.maemo.org])
AM_INIT_AUTOMAKE

AM_MAINTAINER_MODE

AC_CONFIG_SRCDIR([mafw-lastfm/mafw-lastfm.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC

GLIB_VERSION=2.16.0
MAFW_SHARED_VERSION=0.1.2009.37-2
MAFW_VERSION=0.1.2009.37-2
LIBSOUP_VERSION=2.24.0

PKG_CHECK_MODULES([MAFW_LASTFM], [glib-2.0 >= $GLIB_VERSION
				 mafw-shared >= $MAFW_SHARED_VERSION
				 mafw >= $MAFW_VERSION
				 libsoup-2.4 >= $LIBSOUP_VERSION])
AC_SUBST(MAFW_LASTFM_CFLAGS)
AC_SUBST(MAFW_LASTFM_LIBS)

# Checks for header files.
AC_CHECK_HEADERS([string.h])

AC_CONFIG_FILES([
	Makefile
	mafw-lastfm/Makefile
])

AC_OUTPUT
