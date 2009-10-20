/*
    mafw-lastfm: a last.fm scrobbler for mafw
    Copyright (C) 2009  Claudio Saavedra <csaavedra@igalia.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _MAFW_LASTFM_SCROBBLER
#define _MAFW_LASTFM_SCROBBLER

#include <glib-object.h>

G_BEGIN_DECLS

#define MAFW_LASTFM_TYPE_SCROBBLER mafw_lastfm_scrobbler_get_type()

#define MAFW_LASTFM_SCROBBLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), MAFW_LASTFM_TYPE_SCROBBLER, MafwLastfmScrobbler))

#define MAFW_LASTFM_SCROBBLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), MAFW_LASTFM_TYPE_SCROBBLER, MafwLastfmScrobblerClass))

#define MAFW_LASTFM_IS_SCROBBLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MAFW_LASTFM_TYPE_SCROBBLER))

#define MAFW_LASTFM_IS_SCROBBLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), MAFW_LASTFM_TYPE_SCROBBLER))

#define MAFW_LASTFM_SCROBBLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), MAFW_LASTFM_TYPE_SCROBBLER, MafwLastfmScrobblerClass))

typedef struct _MafwLastfmScrobblerPrivate MafwLastfmScrobblerPrivate;

typedef struct {
  GObject parent;
  MafwLastfmScrobblerPrivate *priv;
} MafwLastfmScrobbler;

typedef struct {
  GObjectClass parent_class;
} MafwLastfmScrobblerClass;

typedef struct {
  gchar *artist;
  gchar *title;
  gchar *album;
  glong timestamp;
  gchar source;
  gint64 length;
  gint number;
} MafwLastfmTrack;

GType mafw_lastfm_scrobbler_get_type (void);

MafwLastfmScrobbler* mafw_lastfm_scrobbler_new (void);

void                 mafw_lastfm_scrobbler_set_credentials (MafwLastfmScrobbler *scrobbler,
							    const gchar *username,
							    const gchar *passwd);

void                 mafw_lastfm_scrobbler_handshake (MafwLastfmScrobbler *scrobbler);

void
mafw_lastfm_scrobbler_set_playing_now (MafwLastfmScrobbler *scrobbler,
				       MafwLastfmTrack     *track);
void
mafw_lastfm_scrobbler_enqueue_scrobble (MafwLastfmScrobbler *scrobbler,
					MafwLastfmTrack *track);

void
mafw_lastfm_scrobbler_flush_queue (MafwLastfmScrobbler *scrobbler);

void
mafw_lastfm_scrobbler_suspend (MafwLastfmScrobbler *scrobbler);

MafwLastfmTrack * mafw_lastfm_track_new (void);
void mafw_lastfm_track_free (MafwLastfmTrack *track);

G_END_DECLS

#endif /* _MAFW_LASTFM_SCROBBLER */

