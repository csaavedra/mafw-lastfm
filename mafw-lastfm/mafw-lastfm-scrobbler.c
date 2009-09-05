#include <glib.h>
#include <libsoup/soup.h>

#include "mafw-lastfm-scrobbler.h"

#define CLIENT_ID "tst"
#define CLIENT_VERSION "1.0"

G_DEFINE_TYPE (MafwLastfmScrobbler, mafw_lastfm_scrobbler, G_TYPE_OBJECT)

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAFW_LASTFM_TYPE_SCROBBLER, MafwLastfmScrobblerPrivate))

typedef enum {
  MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE,
  MAFW_LASTFM_SCROBBLER_HANDSHAKING,
  MAFW_LASTFM_SCROBBLER_READY,
  MAFW_LASTFM_SCROBBLER_SUBMITTING
} MafwLastfmScrobblerStatus;

struct _MafwLastfmScrobblerPrivate {
  SoupSession *session;
  gchar *session_id;
  gchar *np_url;
  gchar *sub_url;

  MafwLastfmScrobblerStatus status;
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

  if (priv->session_id != NULL) {
    g_free (priv->session_id);
    priv->session_id = NULL;
  }
  if (priv->np_url == NULL) {
    g_free (priv->np_url);
    priv->np_url = NULL;
  }

  if (priv->sub_url == NULL) {
    g_free (priv->sub_url);
    priv->sub_url = NULL;
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

  priv->session_id = NULL;
  priv->np_url = NULL;
  priv->sub_url = NULL;

  priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
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

static gboolean
parse_handshake_response (MafwLastfmScrobbler *scrobbler,
			  const gchar *response_data)
{
  gchar **response;

  response = g_strsplit (response_data, "\n", 5);

  if (g_str_has_prefix (response [0], "OK")) {
    scrobbler->priv->session_id = response [1];
    scrobbler->priv->np_url = response[2];
    scrobbler->priv->sub_url = response[3];

    /* We take ownership on the relevant parsed data, free the
       array and response code. */
    g_free (response [0]);
    g_free (response [4]);
    g_free (response);

    return TRUE;
  } else {
    g_warning ("Couldn't handshake: %s", response [0]);
    g_strfreev (response);

    return FALSE;
  }
}

static void
handshake_cb (SoupSession *session,
	       SoupMessage *message,
	       gpointer user_data)
{
  MafwLastfmScrobbler *scrobbler = MAFW_LASTFM_SCROBBLER (user_data);

  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("%s", message->response_body->data);
    if (parse_handshake_response (scrobbler, message->response_body->data)) {
      scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_READY;
    } else {
      scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
    }
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

	g_return_if_fail (scrobbler->priv->status != MAFW_LASTFM_SCROBBLER_HANDSHAKING);

	scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_HANDSHAKING;

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
