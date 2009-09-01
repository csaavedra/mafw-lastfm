#include <glib.h>
#include <libmafw/mafw.h>
#include <libmafw-shared/mafw-shared.h>

#define WANTED_RENDERER     "Mafw-Gst-Renderer"

static void
browse_metadata (gchar *key,
		 gchar *value,
		 gpointer user_data)
{
	/* g_print ("%s: %s",  */
}

static void
metadata_callback (MafwRenderer *self,
		   const gchar *object_id,
		   GHashTable *metadata,
		   gpointer user_data,
		   const GError *error)
{
	/* g_hash_table_foreach (metadata, browse_metadata, user_data); */
	g_print ("object_id: %s", object_id);
	mafw_metadata_print (metadata, NULL);
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

		g_print("[INFO] Renderer %s available.\n", name);

		if (strcmp (name, WANTED_RENDERER) == 0) {
			g_print ("[INFO]     Wanted renderer found!\n");
			/* Connect to a few interesting signals */
			/* g_signal_connect (renderer, */
			/* 		  "media-changed", */
			/* 		  G_CALLBACK (media_changed_cb), */
			/* 		  NULL); */
			g_signal_connect (renderer,
					  "state-changed",
					  G_CALLBACK (state_changed_cb),
					  NULL);
			/* g_signal_connect (renderer, */
			/* 		  "metadata-changed", */
			/* 		  G_CALLBACK (metadata_changed_cb), */
			/* 		  NULL); */
			/* g_signal_connect (renderer, */
			/* 		  "error", */
			/* 		  G_CALLBACK (error_cb), */
			/* 		  NULL); */

		} else {
			g_print ("[INFO]     Not interesting. Skipping...\n");
		}
	}
}

int main ()
{
	GError *error = NULL;
        MafwRenderer *renderer = NULL;
	MafwRegistry *registry;
	GMainLoop *main_loop;
	GList *renderers;

	g_type_init ();

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
			  G_CALLBACK(renderer_added_cb), NULL);
	/* Also, check for already started extensions */
	renderers = mafw_registry_get_renderers(registry);
	while (renderers)
	{
		renderer_added_cb (registry,
				   G_OBJECT(renderers->data), NULL);
		renderers = g_list_next(renderers);
	}

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	return 0;
}

