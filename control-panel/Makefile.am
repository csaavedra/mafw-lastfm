AM_LDFLAGS = -shared
INCLUDES = $(MAFW_LASTFM_CPA_CFLAGS)

libmafw_lastfm_cpa_PROGRAMS = libmafw-lastfm-cpa.so
libmafw_lastfm_cpa_so_LIBADD = $(MAFW_LASTFM_CPA_LIBS)
libmafw_lastfm_cpa_so_SOURCES = mafw-lastfm-cpa.c

libmafw_lastfm_cpadir = $(libdir)/hildon-control-panel

hildoncpdesktopentry_DATA = mafw-lastfm.desktop

EXTRA_DIST = $(hildoncpdesktopentry_DATA)
