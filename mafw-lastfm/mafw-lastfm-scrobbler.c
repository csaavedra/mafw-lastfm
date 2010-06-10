/**
 * mafw-lastfm: a last.fm scrobbler for mafw
 *
 * Copyright (C) 2009-2010  Claudio Saavedra <csaavedra@igalia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <glib.h>
#include <libsoup/soup.h>
#include <string.h>

#include "mafw-lastfm-scrobbler.h"

#define CLIENT_ID "maf"
#define CLIENT_VERSION "0.0.1"
#define MAFW_LASTFM_QUEUE_FILE ".osso/mafw-lastfm.queue"

G_DEFINE_TYPE (MafwLastfmScrobbler, mafw_lastfm_scrobbler, G_TYPE_OBJECT);

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MAFW_LASTFM_TYPE_SCROBBLER, MafwLastfmScrobblerPrivate))

typedef enum {
  MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE,
  MAFW_LASTFM_SCROBBLER_HANDSHAKING,
  MAFW_LASTFM_SCROBBLER_READY,
  MAFW_LASTFM_SCROBBLER_SUBMITTING
} MafwLastfmScrobblerStatus;

struct MafwLastfmScrobblerPrivate {
  SoupSession *session;
  gchar *session_id;
  gchar *np_url;
  gchar *sub_url;
  GQueue *scrobbling_queue;
  guint handshake_id;
  guint retry_id;
  guint playing_now_id;
  guint cache_id;
  MafwLastfmTrack *playing_now_track;

  guint retry_interval;
  SoupMessage *retry_message;

  MafwLastfmScrobblerStatus status;

  gchar *username;
  gchar *md5password;

  MafwLastfmTrack *suspended_track;
};

#ifndef MAFW_LASTFM_ENABLE_DEBUG
 #undef g_print
 #define g_print(...)
#endif

static MafwLastfmTrack *
mafw_lastfm_track_encode (MafwLastfmTrack *track);
static gboolean mafw_lastfm_track_cmp (MafwLastfmTrack *a,
                                       MafwLastfmTrack *b);
static  MafwLastfmTrack *
mafw_lastfm_track_dup (MafwLastfmTrack *track);

static void
mafw_lastfm_scrobbler_flush_to_disk (MafwLastfmScrobbler *scrobbler);
static void
mafw_lastfm_scrobbler_scrobble_cached (MafwLastfmScrobbler *scrobbler);
static void
mafw_lastfm_scrobbler_drop_pending_track (MafwLastfmScrobbler *scrobbler);

static void handshake_cb (SoupSession *session,
                          SoupMessage *message,
                          gpointer user_data);

static void
mafw_lastfm_scrobbler_finalize (GObject *object)
{
  MafwLastfmScrobblerPrivate *priv = MAFW_LASTFM_SCROBBLER (object)->priv;

  if (priv->session) {
    soup_session_abort (priv->session);
    g_object_unref (priv->session);
  }

  g_free (priv->session_id);
  g_free (priv->np_url);
  g_free (priv->sub_url);

  if (priv->scrobbling_queue) {
    g_queue_foreach (priv->scrobbling_queue, (GFunc) mafw_lastfm_track_free, NULL);
    g_queue_free (priv->scrobbling_queue);
  }

  if (priv->playing_now_id)
    g_source_remove (priv->playing_now_id);
  if (priv->cache_id)
    g_source_remove (priv->cache_id);
  if (priv->retry_id)
    g_source_remove (priv->retry_id);
  if (priv->handshake_id)
    g_source_remove (priv->handshake_id);

  if (priv->playing_now_track)
    mafw_lastfm_track_free (priv->playing_now_track);

  g_free (priv->username);
  g_free (priv->md5password);

  G_OBJECT_CLASS (mafw_lastfm_scrobbler_parent_class)->finalize (object);
}

static void
mafw_lastfm_scrobbler_class_init (MafwLastfmScrobblerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  g_type_class_add_private (klass, sizeof (MafwLastfmScrobblerPrivate));

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
  priv->handshake_id = 0;
  priv->retry_id = 0;

  priv->retry_message = NULL;
  priv->retry_interval = 5;
  priv->suspended_track = NULL;

  priv->playing_now_track = NULL;
  priv->playing_now_id = 0;

  priv->cache_id = 0;

  priv->username = NULL;
  priv->md5password = NULL;

  priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
}

MafwLastfmScrobbler*
mafw_lastfm_scrobbler_new (void)
{
  return g_object_new (MAFW_LASTFM_TYPE_SCROBBLER, NULL);
}

void
mafw_lastfm_scrobbler_set_credentials (MafwLastfmScrobbler *scrobbler,
                                       const gchar *username,
                                       const gchar *md5password)
{
  g_free (scrobbler->priv->username);
  scrobbler->priv->username = g_strdup (username);

  g_free (scrobbler->priv->md5password);
  scrobbler->priv->md5password = g_strdup (md5password);

  scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
}

static gboolean
on_deferred_handshake_timeout_cb (gpointer user_data)
{
  MafwLastfmScrobbler *scrobbler = user_data;
  scrobbler->priv->handshake_id = 0;

  mafw_lastfm_scrobbler_handshake (scrobbler);

  return FALSE;
}

static void
mafw_lastfm_scrobbler_defer_handshake (MafwLastfmScrobbler *scrobbler)
{
  if (scrobbler->priv->handshake_id != 0)
    return;

  scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
  scrobbler->priv->handshake_id = g_timeout_add_seconds (5,
                                                         (GSourceFunc) on_deferred_handshake_timeout_cb,
                                                         scrobbler);
}

static void
scrobbler_send_message (MafwLastfmScrobbler *scrobbler,
                         const char *url,
                         const char *body,
                         SoupSessionCallback callback)
{
  SoupMessage *message;
  message = soup_message_new ("POST", url);
  soup_message_set_request (message,
                            "application/x-www-form-urlencoded",
                            SOUP_MEMORY_TAKE,
                            body,
                            strlen (body));
  soup_session_queue_message (scrobbler->priv->session,
                              message,
                              callback,
                              scrobbler);
}

static void
set_playing_now_cb (SoupSession *session,
                    SoupMessage *message,
                    gpointer user_data)
{
  MafwLastfmScrobbler *scrobbler = MAFW_LASTFM_SCROBBLER (user_data);

  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("Playing-now: %s", message->response_body->data);
    if (strcmp (message->response_body->data, "BADSESSION\n") == 0)
      mafw_lastfm_scrobbler_defer_handshake (scrobbler);
  }
}

void
mafw_lastfm_scrobbler_set_playing_now (MafwLastfmScrobbler *scrobbler,
                                       MafwLastfmTrack *encoded)
{
  gchar *post_data;

  g_return_if_fail (MAFW_LASTFM_IS_SCROBBLER (scrobbler));
  g_return_if_fail (encoded);
  g_return_if_fail (scrobbler->priv->status == MAFW_LASTFM_SCROBBLER_READY);

  post_data = g_strdup_printf ("s=%s&a=%s&t=%s&b=%s&l=%lli&n=%i&m=",
                               scrobbler->priv->session_id,
                               encoded->artist,
                               encoded->title,
                               encoded->album ? encoded->album : "",
                               encoded->length,
                               encoded->number);

  scrobbler_send_message (scrobbler, scrobbler->priv->np_url,
                          post_data, set_playing_now_cb);
}

/**
 * mafw_lastfm_scrobbler_flush_queue:
 * @scrobbler: a #MafwLastfmScrobbler
 *
 * Flushes the scrobbling queue. This will remove the last song in
 * the queue if it has not been played long enough, and then submit
 * the remaining tracks in the queue.
 **/
void
mafw_lastfm_scrobbler_flush_queue (MafwLastfmScrobbler *scrobbler)
{
  if (scrobbler->priv->suspended_track) {
    mafw_lastfm_track_free (scrobbler->priv->suspended_track);
    scrobbler->priv->suspended_track = NULL;
  }
  mafw_lastfm_scrobbler_drop_pending_track (scrobbler);
  mafw_lastfm_scrobbler_scrobble_cached (scrobbler);
}

void
mafw_lastfm_scrobbler_suspend (MafwLastfmScrobbler *scrobbler)
{
  /* Nothing to suspend. */
  if (scrobbler->priv->suspended_track)
    return;

  /* Remove the last track from the queue, since it is suspended. */
  scrobbler->priv->suspended_track = g_queue_pop_tail (scrobbler->priv->scrobbling_queue);
  /* Nothing to cache */
  if (scrobbler->priv->cache_id) {
    g_source_remove (scrobbler->priv->cache_id);
    scrobbler->priv->cache_id = 0;
  }
}

static gboolean
defer_set_playing_now_cb (MafwLastfmScrobbler *scrobbler)
{
  mafw_lastfm_scrobbler_set_playing_now (scrobbler,
                                         scrobbler->priv->playing_now_track);
  mafw_lastfm_track_free (scrobbler->priv->playing_now_track);
  scrobbler->priv->playing_now_track = NULL;
  scrobbler->priv->playing_now_id = 0;

  return FALSE;
}

static gboolean
cache_scrobble_queue (MafwLastfmScrobbler *scrobbler)
{
  scrobbler->priv->cache_id = 0;

  mafw_lastfm_scrobbler_flush_to_disk (scrobbler);

  return FALSE;
}

static void
mafw_lastfm_scrobbler_drop_pending_track (MafwLastfmScrobbler *scrobbler)
{
  /* If this is != 0, there is a track awaiting to be cached but it shouldn't.
     Drop it. */
  if (scrobbler->priv->cache_id) {
    g_source_remove (scrobbler->priv->cache_id);
    scrobbler->priv->cache_id = 0;
    mafw_lastfm_track_free ((MafwLastfmTrack *)g_queue_pop_tail (scrobbler->priv->scrobbling_queue));
  }
}

void
mafw_lastfm_scrobbler_enqueue_scrobble (MafwLastfmScrobbler *scrobbler,
                                        MafwLastfmTrack *track,
                                        gint position)
{
  MafwLastfmTrack *encoded;
  gint t;

  mafw_lastfm_scrobbler_scrobble_cached (scrobbler);

  encoded = mafw_lastfm_track_encode (track);

  if (scrobbler->priv->playing_now_id) {
    g_source_remove (scrobbler->priv->playing_now_id);
    scrobbler->priv->playing_now_id = 0;
  }
  if (scrobbler->priv->playing_now_track) {
    mafw_lastfm_track_free (scrobbler->priv->playing_now_track);
    scrobbler->priv->playing_now_track = 0;
  }

  mafw_lastfm_scrobbler_drop_pending_track (scrobbler);

  if (scrobbler->priv->suspended_track) {
    if (mafw_lastfm_track_cmp (scrobbler->priv->suspended_track, encoded) &&
        position > 0) {
      mafw_lastfm_track_free (encoded);
      encoded = scrobbler->priv->suspended_track;
    } else {
      mafw_lastfm_track_free (scrobbler->priv->suspended_track);
    }
    scrobbler->priv->suspended_track = NULL;
  }

  /* calculate how much to play before it should be considered worth scrobbling. */
  t = MIN (240, encoded->length/2) - position;
  if (t >= 0) {
    /* Track has not been played enough (or at all). */
    if (scrobbler->priv->status == MAFW_LASTFM_SCROBBLER_READY) {
      /* Set its playing now status. */
      scrobbler->priv->playing_now_track = mafw_lastfm_track_dup (encoded);
      scrobbler->priv->playing_now_id = g_timeout_add_seconds (3,
                                                               (GSourceFunc) defer_set_playing_now_cb,
                                                               scrobbler);
    }
    /* Schedule its caching once it has played enough. */
    g_queue_push_tail (scrobbler->priv->scrobbling_queue, encoded);
    scrobbler->priv->cache_id = g_timeout_add_seconds (t, (GSourceFunc)cache_scrobble_queue, scrobbler);
  }
}

/**
 * get_auth_string:
 * @md5password: the md5sum of the password to build the authorization string from
 * @timestamp: a pointer to store the used timestamp.
 *
 * Builds an authorization string based on the password
 * and the current epoch time. The Last.fm authorization string
 * is of the form md5 (md5 (password) + @timestamp).
 *
 * Returns: a newly allocated string with the authorization md5sum,
 * to be used together with @timestamp.
 **/
static gchar *
get_auth_string (const gchar *md5passwd,
                 glong *timestamp)
{
  GTimeVal time_val;
  gchar *auth;
  gchar *md5;

  g_return_val_if_fail (timestamp, NULL);

  g_get_current_time (&time_val);

  auth = g_strdup_printf ("%s%li", md5passwd, time_val.tv_sec);

  *timestamp = time_val.tv_sec;

  md5 = g_compute_checksum_for_string (G_CHECKSUM_MD5, auth, -1);
  g_free (auth);

  return md5;
}

typedef enum {
  AS_RESPONSE_OK,
  /* AS_RESPONSE_BANNED, */
  /* AS_RESPONSE_BADAUTH, */
  AS_RESPONSE_BADTIME,
  /* AS_RESPONSE_FAILED, */
  AS_RESPONSE_OTHER
} AsHandshakeResponse;

static AsHandshakeResponse
parse_handshake_response (MafwLastfmScrobbler *scrobbler,
                          const gchar *response_data)
{
  gchar **response;
  AsHandshakeResponse retval;
  response = g_strsplit (response_data, "\n", 5);

  if (g_str_has_prefix (response [0], "OK")) {
    g_free (scrobbler->priv->session_id);
    g_free (scrobbler->priv->np_url);
    g_free (scrobbler->priv->sub_url);

    scrobbler->priv->session_id = response[1];
    scrobbler->priv->np_url = response[2];
    scrobbler->priv->sub_url = response[3];

    /* We take ownership on the relevant parsed data, free the
       array and response code. */
    g_free (response[0]);
    g_free (response[4]);
    g_free (response);

    retval = AS_RESPONSE_OK;
  } else if (g_str_has_prefix (response [0], "BADTIME")) {
    retval = AS_RESPONSE_BADTIME;
  } else {
    retval = AS_RESPONSE_OTHER;
  }

  if (retval != AS_RESPONSE_OK) {
    g_warning ("Couldn't handshake: %s", response[0]);
    g_strfreev (response);
  }

  return retval;
}

static gboolean
retry_queue_message (gpointer userdata)
{
  MafwLastfmScrobblerPrivate *priv = MAFW_LASTFM_SCROBBLER (userdata)->priv;

  priv->retry_id = 0;
  g_print ("retrying to queue message\n");
  soup_session_queue_message (priv->session,
                              priv->retry_message,
                              handshake_cb,
                              MAFW_LASTFM_SCROBBLER (userdata));
  priv->retry_message = NULL;

  return FALSE;
}

static void
handshake_cb (SoupSession *session,
              SoupMessage *message,
              gpointer user_data)
{
  MafwLastfmScrobbler *scrobbler = MAFW_LASTFM_SCROBBLER (user_data);

  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("%s", message->response_body->data);
    switch (parse_handshake_response (scrobbler, message->response_body->data)) {
    case AS_RESPONSE_OK:
      scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_READY;
      scrobbler->priv->retry_interval = 5;
      mafw_lastfm_scrobbler_scrobble_cached (scrobbler);
      return;
    case AS_RESPONSE_BADTIME:
      scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
      scrobbler->priv->retry_interval = 5;
      mafw_lastfm_scrobbler_handshake (scrobbler);
      return;
    case AS_RESPONSE_OTHER:
      break;
    }
  }

  /* If something went wrong, try to recover. */
  g_print ("message failed, trying to send in %d seconds.\n", scrobbler->priv->retry_interval);
  scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_NEED_HANDSHAKE;
  scrobbler->priv->retry_message = g_object_ref (message);
  scrobbler->priv->retry_id = g_timeout_add_seconds (scrobbler->priv->retry_interval,
                                                     retry_queue_message,
                                                     scrobbler);
  if (scrobbler->priv->retry_interval < 320)
    scrobbler->priv->retry_interval *= 2;
}

void
mafw_lastfm_scrobbler_handshake (MafwLastfmScrobbler *scrobbler)
{
  gchar *auth;
  glong timestamp;
  gchar *handshake_url;
  SoupMessage *message;

  g_return_if_fail (scrobbler->priv->status != MAFW_LASTFM_SCROBBLER_HANDSHAKING);
  g_return_if_fail (scrobbler->priv->username || scrobbler->priv->md5password);
  if (scrobbler->priv->retry_id) {
    g_source_remove (scrobbler->priv->retry_id);
    scrobbler->priv->retry_id = 0;
    if (scrobbler->priv->retry_message) {
      g_object_unref (scrobbler->priv->retry_message);
      scrobbler->priv->retry_message = NULL;
    }
  }
  if (scrobbler->priv->handshake_id) {
    g_source_remove (scrobbler->priv->handshake_id);
    scrobbler->priv->handshake_id = 0;
  }
  scrobbler->priv->status = MAFW_LASTFM_SCROBBLER_HANDSHAKING;

  auth = get_auth_string (scrobbler->priv->md5password, &timestamp);

  handshake_url = g_strdup_printf ("http://post.audioscrobbler.com/?hs=true&p=1.2.1&c=%s&v=%s&u=%s&t=%li&a=%s",
                                   CLIENT_ID, CLIENT_VERSION,
                                   scrobbler->priv->username,
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

static void
mafw_lastfm_scrobbler_flush_to_disk (MafwLastfmScrobbler *scrobbler)
{
  MafwLastfmTrack *track;
  GFileOutputStream *outstream;
  gchar *buffer = NULL, **tracks;
  gint length, i;
  GList *iter;
  GError *error = NULL;
  gboolean success = TRUE;
  GFile *file;
  gchar *filename;

  filename = g_build_filename (g_get_home_dir(),
                               MAFW_LASTFM_QUEUE_FILE, NULL);
  file = g_file_new_for_path (filename);
  g_free (filename);

  outstream = g_file_append_to (file, G_FILE_CREATE_PRIVATE, NULL, &error);

  if (error) {
    g_warning ("Couldn't open output file: %s\n", error->message);
    g_error_free (error);
    goto out;
  }

  length = g_queue_get_length (scrobbler->priv->scrobbling_queue);
  tracks = g_new0 (gchar *, length + 1);

  for (i = 0, iter = scrobbler->priv->scrobbling_queue->head;
       iter != NULL;
       i++, iter = g_list_next (iter)) {
    track = (MafwLastfmTrack *) iter->data;
    tracks[i] = g_strdup_printf ("%s&%s&%li&%c&%lld&%s&%i\n",
                                 track->artist,
                                 track->title,
                                 track->timestamp,
                                 track->source,
                                 /* ratio skipped */
                                 track->length,
                                 track->album ? track->album : "",
                                 track->number
                                 /* musicbrainz id skipped */);
  }

  buffer = g_strjoinv (NULL, tracks);
  g_strfreev (tracks);
  g_output_stream_write (G_OUTPUT_STREAM (outstream), buffer, strlen (buffer), NULL, &error);

  if (error) {
    g_warning ("Error appending tracks: %s\n", error->message);
    g_error_free (error);
    success = FALSE;
    error = NULL;
  }

  g_output_stream_close (G_OUTPUT_STREAM (outstream), NULL, &error);
  if (error) {
    g_warning ("Error closing file: %s\n", error->message);
    success = FALSE;
    g_error_free (error);
  }

  if (success) {
    g_print ("Cached %i track(s) on disk\n", length);
    g_queue_foreach (scrobbler->priv->scrobbling_queue, (GFunc)mafw_lastfm_track_free, NULL);
    g_queue_clear (scrobbler->priv->scrobbling_queue);
  }

out:
  g_free (buffer);
  g_object_unref (outstream);
  g_object_unref (file);
}

static void
cached_scrobble_cb (SoupSession *session,
                    SoupMessage *message,
                    gpointer user_data)
{
  GFile *file;
  gchar *filename;

  if (SOUP_STATUS_IS_SUCCESSFUL (message->status_code)) {
    g_print ("Scrobble: %s", message->response_body->data);
    if (g_str_has_prefix (message->response_body->data, "OK")) {
      filename = g_build_filename (g_get_home_dir (), MAFW_LASTFM_QUEUE_FILE, NULL);
      file = g_file_new_for_path (filename);
      g_file_delete (file, NULL, NULL);
      g_object_unref (file);
      g_free (filename);
      return;
    }
  }
  /* If we are here, we failed to submit. */
  mafw_lastfm_scrobbler_defer_handshake (MAFW_LASTFM_SCROBBLER  (user_data));
}

static void
mafw_lastfm_scrobbler_scrobble_cached (MafwLastfmScrobbler *scrobbler)
{
  gchar *filename;
  gchar *buffer;
  gchar **tracks = NULL;
  gchar **new_tracks;
  gchar **track;
  gchar *post_data;
  gint i, n;

  filename = g_build_filename (g_get_home_dir(),
                               MAFW_LASTFM_QUEUE_FILE, NULL);

  if (g_file_get_contents (filename, &buffer, NULL, NULL) && buffer) {
    tracks = g_strsplit (buffer, "\n", 0);
    g_free (buffer);
    buffer = NULL;
  }
  g_free (filename);

  if (tracks) {
    n = g_strv_length (tracks);
    new_tracks = g_new0 (gchar *, n);
    for (i = 0; i < n - 1; i++) {
      track = g_strsplit (tracks[i], "&", 0);
      new_tracks[i] = g_strdup_printf ("a[%i]=%s&t[%i]=%s&i[%i]=%s&o[%i]=%s&r[%i]=&l[%i]=%s&b[%i]=%s&n[%i]=%s&m[%i]=",
                                       i, track [0],
                                       i, track [1],
                                       i, track [2],
                                       i, track [3],
                                       i, /* ratio skipped */
                                       i, track [4],
                                       i, track [5] ? track [5] : "",
                                       i, track [6],
                                       i, track [7],
                                       i /* musicbrainz id skipped */);
      g_strfreev (track);
    }
    buffer = g_strjoinv ("&", new_tracks);
    g_strfreev (tracks);
    g_strfreev (new_tracks);
  }

  if (buffer) {
    post_data = g_strdup_printf ("s=%s&%s",
                                 scrobbler->priv->session_id,
                                 buffer);
    g_free (buffer);
    scrobbler_send_message (scrobbler, scrobbler->priv->sub_url,
                            post_data, cached_scrobble_cb);
  }
}

MafwLastfmTrack *
mafw_lastfm_track_new (void)
{
  return g_new0 (MafwLastfmTrack, 1);
}

void
mafw_lastfm_track_free (MafwLastfmTrack *track)
{
  if (!track)
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

  if (track->artist)
    encoded->artist = soup_uri_encode (track->artist, EXTRA_URI_ENCODE_CHARS);

  if (track->title)
    encoded->title = soup_uri_encode (track->title, EXTRA_URI_ENCODE_CHARS);

  if (track->album)
    encoded->album = soup_uri_encode (track->album, EXTRA_URI_ENCODE_CHARS);

  encoded->length = track->length;
  encoded->number = track->number;
  encoded->timestamp = track->timestamp;
  encoded->source = track->source;

  return encoded;
}

static gboolean
mafw_lastfm_track_cmp (MafwLastfmTrack *a,
                       MafwLastfmTrack *b)
{
  return (strcmp (a->artist, b->artist) == 0 &&
          strcmp (a->title, b->title) == 0 &&
          a->length == b->length &&
          (!(a->album || b->album) ||
           ((a->album && b->album) && strcmp (a->album, b->album) == 0)));
}

static MafwLastfmTrack *
mafw_lastfm_track_dup (MafwLastfmTrack *track)
{
  MafwLastfmTrack *track2 = mafw_lastfm_track_new ();

  track2->artist = g_strdup (track->artist);
  track2->title = g_strdup (track->title);
  track2->album = g_strdup (track->album);
  track2->timestamp = track->timestamp;
  track2->source = track->source;
  track2->length = track->length;
  track2->number = track->number;

  return track2;
}
