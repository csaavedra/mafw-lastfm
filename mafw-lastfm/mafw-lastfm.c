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
	g_print ("artist: %s\n", mafw_metadata_lookup_string (metadata, MAFW_METADATA_KEY_ARTIST));
	g_print ("title: %s\n", mafw_metadata_lookup_string (metadata, MAFW_METADATA_KEY_TITLE));
	g_print ("album: %s\n\n", mafw_metadata_lookup_string (metadata, MAFW_METADATA_KEY_ALBUM));
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
                        /* Enable Resume button */
                        break;
                case Stopped:
                        /* Disable Pause/Resume button */
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

int main ()
{
	GError *error = NULL;
	MafwRegistry *registry;
	GMainLoop *main_loop;
	GList *renderers;
	MafwLastfmScrobbler *scrobbler;

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

	mafw_lastfm_scrobbler_handshake (scrobbler, "user", "password");

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}

