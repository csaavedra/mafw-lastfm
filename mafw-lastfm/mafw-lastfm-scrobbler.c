#include <glib.h>
#include <libsoup/soup.h>

#include "mafw-lastfm-scrobbler.h"

#define CLIENT_ID "tst"
#define CLIENT_VERSION "1.0"

G_DEFINE_TYPE (MafwLastfmScrobbler, mafw_lastfm_scrobbler, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAFW_LASTFM_TYPE_SCROBBLER, MafwLastfmScrobblerPrivate))

struct _MafwLastfmScrobblerPrivate {
    SoupSession *session;
};

static void
mafw_lastfm_scrobbler_get_property (GObject *object, guint property_id,
                              GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mafw_lastfm_scrobbler_set_property (GObject *object, guint property_id,
                              const GValue *value, GParamSpec *pspec)
{
  switch (property_id) {
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
  }
}

static void
mafw_lastfm_scrobbler_dispose (GObject *object)
{
  MafwLastfmScrobblerPrivate *priv = MAFW_LASTFM_SCROBBLER (object)->priv;

  if (priv->session != NULL) {
    soup_session_abort (priv->session);
    g_object_unref (priv->session);
    priv->session = NULL;
  }

  G_OBJECT_CLASS (mafw_lastfm_scrobbler_parent_class)->dispose (object);
}

static void
mafw_lastfm_scrobbler_finalize (GObject *object)
{
  G_OBJECT_CLASS (mafw_lastfm_scrobbler_parent_class)->finalize (object);
}

static void
mafw_lastfm_scrobbler_class_init (MafwLastfmScrobblerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MafwLastfmScrobblerPrivate));

  object_class->get_property = mafw_lastfm_scrobbler_get_property;
  object_class->set_property = mafw_lastfm_scrobbler_set_property;
  object_class->dispose = mafw_lastfm_scrobbler_dispose;
  object_class->finalize = mafw_lastfm_scrobbler_finalize;
}

static void
mafw_lastfm_scrobbler_init (MafwLastfmScrobbler *scrobbler)
{
  MafwLastfmScrobblerPrivate *priv = scrobbler->priv = GET_PRIVATE (scrobbler);

  priv->session = soup_session_async_new ();
}

MafwLastfmScrobbler*
mafw_lastfm_scrobbler_new (void)
{
  return g_object_new (MAFW_LASTFM_TYPE_SCROBBLER, NULL);
}

/*void
mafw_lastfm_scrobbler_set_playing_now (MafwLastfmScrobbler *scrobbler,
				       MafwLastfmTrack     *track)
{

}

void
mafw_lastfm_scrobbler_scrobble (MafwLastfmScrobbler *scrobbler,
			        MafwLastfmTrack     *track)
{

}
*/
/**
 * get_auth_string:
 * @password: a password to build the authorization string from
 * @timestamp: a pointer to store the used timestamp.
 *
 * Builds an authorization string based on the password
 * and the current epoch time. The Last.fm authorization string
 * is of the form md5 (md5 (@password) + @timestamp).
 *
 * Returns: a newly allocated string with the authorization md5sum,
 * to be used together with @timestamp.
 **/
static gchar *
get_auth_string (const gchar *password,
		 glong *timestamp)
{
	GTimeVal time_val;
	gchar *auth;
	gchar *md5;

	g_return_val_if_fail (timestamp != NULL, NULL);

	g_get_current_time (&time_val);
	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, password, -1);

	auth = g_strdup_printf ("%s%li", md5, time_val.tv_sec);
	g_free (md5);

	*timestamp = time_val.tv_sec;

	md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, auth, -1);
	g_free (auth);

	return md5;
}

static void
handshake_cb (SoupSession *session,
	       SoupMessage *message,
	       gpointer user_data)
{
  MafwLastfmScrobbler *scrobbler = MAFW_LASTFM_SCROBBLER (user_data);

  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("%s", message->response_body->data);
  }
}

void
mafw_lastfm_scrobbler_handshake (MafwLastfmScrobbler *scrobbler,
				 const gchar *username,
				 const gchar *passwd)
{
	gchar *auth;
	glong timestamp;
	gchar *handshake_url;
	SoupMessage *message;

	auth = get_auth_string (passwd, &timestamp);

	handshake_url = g_strdup_printf ("http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=%s&v=%s&u=%s&t=%li&a=%s",
					 CLIENT_ID, CLIENT_VERSION,
					 username,
					 timestamp,
					 auth);

	message = soup_message_new ("GET", handshake_url);
	soup_session_queue_message (scrobbler->priv->session,
				    message,
				    handshake_cb,
				    scrobbler);
	g_free (handshake_url);
	g_free (auth);
}
