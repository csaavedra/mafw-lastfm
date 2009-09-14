/* mafw-lastfm-scrobbler.h */

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
  guint number;
  gint64 length;
} MafwLastfmTrack;

GType mafw_lastfm_scrobbler_get_type (void);

MafwLastfmScrobbler* mafw_lastfm_scrobbler_new (void);
void                 mafw_lastfm_scrobbler_handshake (MafwLastfmScrobbler *scrobbler,
						      const gchar *username,
						      const gchar *passwd);
void
mafw_lastfm_scrobbler_set_playing_now (MafwLastfmScrobbler *scrobbler,
				       MafwLastfmTrack     *track);

MafwLastfmTrack * mafw_lastfm_track_new (void);
void mafw_lastfm_track_free (MafwLastfmTrack *track);

G_END_DECLS

#endif /* _MAFW_LASTFM_SCROBBLER */

