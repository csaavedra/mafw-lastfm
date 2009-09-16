#include <glib.h>
#include <libsoup/soup.h>
#include <string.h>

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
  GQueue *scrobbling_queue;
  guint timeout;

  MafwLastfmScrobblerStatus status;
};

static MafwLastfmTrack *
mafw_lastfm_track_encode (MafwLastfmTrack *track);

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

  if (priv->scrobbling_queue) {
    g_queue_foreach (priv->scrobbling_queue, (GFunc) mafw_lastfm_track_free, NULL);
    g_queue_free (priv->scrobbling_queue);
    priv->scrobbling_queue = NULL;
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
  priv->scrobbling_queue = g_queue_new ();

  priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
}

MafwLastfmScrobbler*
mafw_lastfm_scrobbler_new (void)
{
  return g_object_new (MAFW_LASTFM_TYPE_SCROBBLER, NULL);
}

static void
scrobble_cb (SoupSession *session,
	     SoupMessage *message,
	     gpointer user_data)
{
  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("%s", message->response_body->data);
  }
}

/**
 * mafw_lastfm_scrobbler_scrobble_list:
 * @scrobbler: a scrobbler
 * @list: a %NULL-terminated list of %MafwLastfmTrack elements
 *
 * Submits a series of tracks from the list.
 **/
static void
mafw_lastfm_scrobbler_scrobble_list (MafwLastfmScrobbler *scrobbler,
				     GList *list)
{
  gint i;
  GList *iter;
  gchar *post_data;
  gchar *track_data, *tmp;
  MafwLastfmTrack *track;
  SoupMessage *message;

  post_data = g_strdup_printf ("s=%s", scrobbler->priv->session_id);

  for (iter = list, i = 0; iter != NULL; iter = iter->next, i++) {
    track = (MafwLastfmTrack *) iter->data;
    track_data = g_strdup_printf ("&a[%i]=%s&t[%i]=%s&i[%i]=%li&o[%i]=%c&r[%i]=&l[%i]=%lld&b[%i]=%s&n[%i]=%i&m[%i]=",
				  i, track->artist,
				  i, track->title,
				  i, track->timestamp,
				  i, track->source,
				  i, /* ratio skipped */
				  i, track->length,
				  i, track->album,
				  i, track->number,
				  i /* musicbrainz id skipped */);
    tmp = post_data;
    post_data = g_strconcat (tmp, track_data, NULL);
    g_free (tmp);
  }

  message = soup_message_new ("POST", scrobbler->priv->sub_url);
  soup_message_set_request (message,
			    "application/x-www-form-urlencoded",
			    SOUP_MEMORY_TAKE,
			    post_data,
			    strlen (post_data));
  soup_session_queue_message (scrobbler->priv->session,
			      message,
			      scrobble_cb,
			      scrobbler);
}

static void
set_playing_now_cb (SoupSession *session,
		    SoupMessage *message,
		    gpointer user_data)
{
  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("%s", message->response_body->data);
  }
}

void
mafw_lastfm_scrobbler_set_playing_now (MafwLastfmScrobbler *scrobbler,
				       MafwLastfmTrack     *track)
{
  gchar *post_data;
  SoupMessage *message;
  MafwLastfmTrack *encoded;

  g_return_if_fail (MAFW_LASTFM_IS_SCROBBLER (scrobbler));
  g_return_if_fail (track != NULL);
  g_return_if_fail (scrobbler->priv->status == MAFW_LASTFM_SCROBBLER_READY);

  encoded = mafw_lastfm_track_encode (track);
  post_data = g_strdup_printf ("s=%s&a=%s&t=%s&b=%s&l=%lli&n=%u&m=",
			       scrobbler->priv->session_id,
			       encoded->artist,
			       encoded->title,
			       encoded->album,
			       encoded->length,
			       encoded->number);
  g_free (encoded);

  message = soup_message_new ("POST",
			      scrobbler->priv->np_url);
  soup_message_set_request (message,
			    "application/x-www-form-urlencoded",
			    SOUP_MEMORY_TAKE,
			    post_data,
			    strlen (post_data));
  soup_session_queue_message (scrobbler->priv->session,
			      message,
			      set_playing_now_cb,
			      scrobbler);
}

static gboolean
on_timeout (gpointer data)
{
  MafwLastfmScrobbler *scrobbler;
  MafwLastfmTrack *track;
  GList *list = NULL;
  gint tracks = 0;

  scrobbler = MAFW_LASTFM_SCROBBLER (data);

  while (!g_queue_is_empty (scrobbler->priv->scrobbling_queue) && tracks < 50) {
    track = g_queue_pop_head (scrobbler->priv->scrobbling_queue);
    list = g_list_append (list, track);
    tracks ++;
  }

  if (list)
  {
    mafw_lastfm_scrobbler_scrobble_list (scrobbler, list);
  }

  scrobbler->priv->timeout = 0;
  return FALSE;
}

void
mafw_lastfm_scrobbler_clean_queue (MafwLastfmScrobbler *scrobbler)
{
  glong timestamp = time (NULL);
  MafwLastfmTrack *track;

  track = (MafwLastfmTrack *) g_queue_peek_head (scrobbler->priv->scrobbling_queue);

  if (timestamp - track->timestamp < MIN (240, track->length/2))
  {
    g_queue_pop_head (scrobbler->priv->scrobbling_queue);
    mafw_lastfm_track_free (track);
  }
}

void
mafw_lastfm_scrobbler_enqueue_scrobble (MafwLastfmScrobbler *scrobbler,
					MafwLastfmTrack *track)
{
  if (scrobbler->priv->timeout != 0) {
    g_source_remove (scrobbler->priv->timeout);
    mafw_lastfm_scrobbler_clean_queue (scrobbler);
    on_timeout ((gpointer) scrobbler);
  }

  mafw_lastfm_scrobbler_set_playing_now (scrobbler, track);

  g_queue_push_tail (scrobbler->priv->scrobbling_queue,
		     mafw_lastfm_track_encode (track));

  scrobbler->priv->timeout = g_timeout_add_seconds ((gint)track->length, on_timeout, scrobbler);
}

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

MafwLastfmTrack *
mafw_lastfm_track_new (void)
{
  return g_new0 (MafwLastfmTrack, 1);
}

void
mafw_lastfm_track_free (MafwLastfmTrack *track)
{
  if (track == NULL)
    return;

  g_free (track->artist);
  g_free (track->title);
  g_free (track->album);

  g_free (track);
}

#define EXTRA_URI_ENCODE_CHARS "&+"

static MafwLastfmTrack *
mafw_lastfm_track_encode (MafwLastfmTrack *track)
{
  MafwLastfmTrack *encoded;

  encoded = mafw_lastfm_track_new ();

  encoded->artist = soup_uri_encode (track->artist, EXTRA_URI_ENCODE_CHARS);
  encoded->title = soup_uri_encode (track->title, EXTRA_URI_ENCODE_CHARS);
  encoded->album = soup_uri_encode (track->album, EXTRA_URI_ENCODE_CHARS);
  encoded->length = track->length;
  encoded->number = track->number;
  encoded->timestamp = track->timestamp;
  encoded->source = track->source;

  return encoded;
}
