AC_INIT([mafw-lastfm], [0.0.5], [https://bugs.maemo.org/enter_bug.cgi?product=mafw-lastfm])
AM_INIT_AUTOMAKE

AM_MAINTAINER_MODE

AC_CONFIG_SRCDIR([mafw-lastfm/mafw-lastfm.c])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AM_PROG_CC_C_O

GLIB_VERSION=2.16.0
LIBSOUP_VERSION=2.24.0

PKG_CHECK_MODULES([MAFW_LASTFM], [glib-2.0 >= $GLIB_VERSION
				 mafw-shared
				 mafw
				 libsoup-2.4 >= $LIBSOUP_VERSION])
AC_SUBST(MAFW_LASTFM_CFLAGS)
AC_SUBST(MAFW_LASTFM_LIBS)

HILDON_VERSION=2.1.30
GTK_VERSION=2.14.0

PKG_CHECK_MODULES([MAFW_LASTFM_CPA], [hildon-1 >= $HILDON_VERSION
				     gtk+-2.0 >= $GTK_VERSION
				     hildon-control-panel
				     libosso])
AC_SUBST(MAFW_LASTFM_CPA_CFLAGS)
AC_SUBST(MAFW_LASTFM_CPA_LIBS)

hildoncpdesktopentrydir=`pkg-config hildon-control-panel --variable=plugindesktopentrydir`
AC_SUBST(hildoncpdesktopentrydir)

CFLAGS="$CFLAGS -Wall -Werror -Wmissing-prototypes -Wmissing-declarations -Wno-format"

# Checks for header files.
AC_CHECK_HEADERS([string.h])

AC_CONFIG_FILES([
	Makefile
	control-panel/Makefile
	mafw-lastfm/Makefile
])

AC_OUTPUT
