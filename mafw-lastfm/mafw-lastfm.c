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

#include <glib.h>
#include <libmafw/mafw.h>
#include <libmafw-shared/mafw-shared.h>
#include <string.h>

#include "mafw-lastfm-scrobbler.h"

#define WANTED_RENDERER     "Mafw-Gst-Renderer"

static gchar *
mafw_metadata_lookup_string (GHashTable *table,
			     const gchar *key)
{
	return g_value_dup_string (mafw_metadata_first (table, key));
}

static int
mafw_metadata_lookup_int (GHashTable *table,
			  const gchar *key)
{
	return g_value_get_int (mafw_metadata_first (table, key));
}

static void
metadata_callback (MafwRenderer *self,
		   const gchar *object_id,
		   GHashTable *metadata,
		   gpointer user_data,
		   const GError *error)
{
	MafwLastfmScrobbler *scrobbler;
	MafwLastfmTrack *track;
	GTimeVal time_val;

	g_get_current_time (&time_val);

	scrobbler = MAFW_LASTFM_SCROBBLER (user_data);

	track = mafw_lastfm_track_new ();

	track->artist = mafw_metadata_lookup_string (metadata, MAFW_METADATA_KEY_ARTIST);
	track->title = mafw_metadata_lookup_string (metadata, MAFW_METADATA_KEY_TITLE);

	if (track->artist == NULL || track->title == NULL)
		return;

	track->timestamp = time_val.tv_sec; /* This should probably be obtained in the
					       state changed cb */
	track->source = 'P';
	track->album = mafw_metadata_lookup_string (metadata, MAFW_METADATA_KEY_ALBUM);
	track->number = mafw_metadata_lookup_int (metadata, MAFW_METADATA_KEY_TRACK);
	track->length = g_value_get_int64 (mafw_metadata_first (metadata, MAFW_METADATA_KEY_DURATION));

	mafw_lastfm_scrobbler_enqueue_scrobble (scrobbler, track);

	mafw_lastfm_track_free (track);
}

static void
state_changed_cb(MafwRenderer *renderer, MafwPlayState state,
                 gpointer user_data)
{
        switch (state) {
                case Playing:
			mafw_renderer_get_current_metadata (renderer,
							    metadata_callback,
							    user_data);
                        break;
                case Paused:
			mafw_lastfm_scrobbler_suspend (MAFW_LASTFM_SCROBBLER (user_data));
                        break;
                case Stopped:
			mafw_lastfm_scrobbler_flush_queue (MAFW_LASTFM_SCROBBLER (user_data));
                        break;
                default:
                        break;
        }
}

static void
renderer_added_cb (MafwRegistry *registry,
		   GObject *renderer,
		   gpointer user_data)
{
	if (MAFW_IS_RENDERER(renderer)) {
		const gchar *name =
			mafw_extension_get_name (MAFW_EXTENSION(renderer));

		if (strcmp (name, WANTED_RENDERER) == 0) {
			g_signal_connect (renderer,
					  "state-changed",
					  G_CALLBACK (state_changed_cb),
					  user_data);
		}
	}
}

#define MAFW_LASTFM_CREDENTIALS_FILE ".mafw-lastfm"

static gboolean
get_credentials (gchar **username,
		 gchar **pw_md5)
{
	gchar *file = g_build_filename (g_get_home_dir (),
					MAFW_LASTFM_CREDENTIALS_FILE, NULL);
	GKeyFile *keyfile;
	GError *error = NULL;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, file, G_KEY_FILE_NONE, &error))
	{
		if (error)
		{
			g_warning ("Error loading credentials file: %s",
				   error->message);
			g_error_free (error);
		}

		g_key_file_free (keyfile);

		return FALSE;
	}

	*username = g_key_file_get_string (keyfile,
					   "Credentials", "username", NULL);
	*pw_md5 = g_key_file_get_string (keyfile,
					 "Credentials", "password", NULL);

	if (*username == NULL || *pw_md5 == NULL)
	{
		g_warning ("Error loading username or password md5");

		g_free (*username);
		g_free (*pw_md5);
		g_key_file_free (keyfile);

		return FALSE;
	}

	g_key_file_free (keyfile);

	return TRUE;
}

int main ()
{
	GError *error = NULL;
	MafwRegistry *registry;
	GMainLoop *main_loop;
	GList *renderers;
	MafwLastfmScrobbler *scrobbler;
	gchar *username, *md5passwd;

	g_type_init ();
	if (!g_thread_supported ())
		g_thread_init (NULL);

	scrobbler = mafw_lastfm_scrobbler_new ();

	registry = MAFW_REGISTRY(mafw_registry_get_instance());
	if (registry == NULL) {
		g_warning ("Failed to get register.\n");
		return 1;
	}

	mafw_shared_init(registry, &error);
	if (error != NULL) {
		g_warning ("Failed to initialize the shared library.\n");
		return 1;
	}

	g_signal_connect (registry,
			  "renderer_added",
			  G_CALLBACK(renderer_added_cb), scrobbler);
	/* Also, check for already started extensions */
	renderers = mafw_registry_get_renderers(registry);
	while (renderers)
	{
		renderer_added_cb (registry,
				   G_OBJECT(renderers->data), scrobbler);
		renderers = g_list_next(renderers);
	}

	if (get_credentials (&username, &md5passwd)) {
	    mafw_lastfm_scrobbler_set_credentials (scrobbler, username, md5passwd);
	    mafw_lastfm_scrobbler_handshake (scrobbler);
	}

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}

