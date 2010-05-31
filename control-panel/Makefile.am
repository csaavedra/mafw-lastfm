AM_LDFLAGS = -shared
AM_CPPFLAGS = $(MAFW_LASTFM_CPA_CFLAGS)
INCLUDES = -DLOCALEDIR=\"$(localedir)\"

libmafw_lastfm_cpa_PROGRAMS = libmafw-lastfm-cpa.so
libmafw_lastfm_cpa_so_LDADD = $(MAFW_LASTFM_CPA_LIBS)
libmafw_lastfm_cpa_so_SOURCES = mafw-lastfm-cpa.c

libmafw_lastfm_cpadir = $(libdir)/hildon-control-panel

hildoncpdesktopentry_in_files = mafw-lastfm.desktop.in
hildoncpdesktopentry_DATA = $(hildoncpdesktopentry_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(hildoncpdesktopentry_DATA)

DISTCLEANFILES = $(hildoncpdesktopentry_DATA)