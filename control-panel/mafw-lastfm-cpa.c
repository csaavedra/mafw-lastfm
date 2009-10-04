#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#include <hildon/hildon.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <libintl.h>

#define MAFW_LASTFM_CREDENTIALS_FILE ".mafw-lastfm"

typedef struct {
	GtkEntry *username;
	GtkEntry *password;
	gchar *settings_file;
} SettingsContext;

static gchar*
load_username (const gchar *file)
{
	GKeyFile *keyfile;
	gchar *username;

	keyfile = g_key_file_new ();
	g_key_file_load_from_file (keyfile, file, G_KEY_FILE_NONE, NULL);
	username = g_key_file_get_string (keyfile,
					  "Credentials", "username", NULL);

	g_key_file_free (keyfile);

	return username;
}

static void
save_credentials (const gchar *file,
		  const gchar *username,
		  const gchar *password)
{
	gchar *md5passwd;
	GKeyFile *keyfile;

	keyfile = g_key_file_new ();
	md5passwd = g_compute_checksum_for_string (G_CHECKSUM_MD5,
						   password, -1);

	g_key_file_set_string (keyfile, "Credentials",
			       "username", username);
	g_key_file_set_string (keyfile, "Credentials",
			       "password", md5passwd);

	g_file_set_contents (file,
			     g_key_file_to_data (keyfile, NULL, NULL),
			     -1, NULL);
	g_free (md5passwd);
	g_key_file_free (keyfile);
}

static void
on_dialog_response (GtkDialog *dialog,
		    gint response_id,
		    gpointer user_data)
{
	SettingsContext *ctx;
	const gchar *username;
	const gchar *password;

	ctx = (SettingsContext *) user_data;

	if (response_id == GTK_RESPONSE_OK)
	{
		username = gtk_entry_get_text (ctx->username);
		password = gtk_entry_get_text (ctx->password);

		if (username[0] == '\0' || password[0] == '\0')
		{
			hildon_banner_show_information (GTK_WIDGET (dialog), NULL,
							"Enter both your username "
							"and password before saving.");
			return;
		} else
		{
			save_credentials (ctx->settings_file, username, password);
		}
	}

	gtk_widget_hide (GTK_WIDGET (dialog));
	gtk_widget_destroy (GTK_WIDGET (dialog));

	g_free (ctx->settings_file);
	g_free (ctx);
}

osso_return_t
execute(osso_context_t *osso, gpointer data, gboolean user_activated)
{
	GtkWidget *dialog;
	GtkWidget *username;
	GtkWidget *password;
	GtkWidget *vbox, *hbox;
	GtkWidget *label_username;
	GtkWidget *label_password;
	gchar *usr, *settings_file;
	SettingsContext *ctx;

	settings_file = g_build_filename (g_get_home_dir (),
					  MAFW_LASTFM_CREDENTIALS_FILE, NULL);

	dialog = gtk_dialog_new_with_buttons ("Last.fm settings",
					      GTK_WINDOW (data),
					      GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
					      dgettext ("hildon-libs", "wdgt_bd_done"),
					      GTK_RESPONSE_OK,
					      NULL);
	vbox = gtk_vbox_new (TRUE, 0);
	label_username = gtk_label_new ("Username:");
	gtk_misc_set_alignment (GTK_MISC (label_username), 0.0, 0.5);
	username = hildon_entry_new (HILDON_SIZE_AUTO | HILDON_SIZE_FINGER_HEIGHT);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_username, TRUE, TRUE, 20);
	gtk_box_pack_start (GTK_BOX (hbox), username, TRUE, TRUE, 0);

	label_password = gtk_label_new ("Password:");
	gtk_misc_set_alignment (GTK_MISC (label_password), 0.0, 0.5);
	password = hildon_entry_new (HILDON_SIZE_AUTO | HILDON_SIZE_FINGER_HEIGHT);
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (password),
					 HILDON_GTK_INPUT_MODE_FULL |
					 HILDON_GTK_INPUT_MODE_INVISIBLE);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label_password, TRUE, TRUE, 20);
	gtk_box_pack_start (GTK_BOX (hbox), password, TRUE, TRUE, 0);

	if (g_file_test (settings_file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
	{
		usr = load_username (settings_file);
		gtk_entry_set_text (GTK_ENTRY (username), usr);
		g_free (usr);
	}

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER
			   (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
			   vbox);

	ctx = g_new0 (SettingsContext, 1);

	ctx->username = GTK_ENTRY (username);
	ctx->password = GTK_ENTRY (password);
	ctx->settings_file = settings_file;

	g_signal_connect (dialog, "response",
			  G_CALLBACK (on_dialog_response),
			  ctx);

	gtk_widget_show_all (dialog);

	return OSSO_OK;
}

osso_return_t
save_state(osso_context_t *osso, gpointer data)
{
        return OSSO_OK;
}
