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

#include <hildon-cp-plugin/hildon-cp-plugin-interface.h>
#include <hildon/hildon.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <locale.h>
#include <glib/gi18n.h>
#include "config.h"

#define MAFW_LASTFM_CREDENTIALS_FILE ".osso/mafw-lastfm"

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
	gchar *username;
	const gchar *password;

	ctx = (SettingsContext *) user_data;

	if (response_id == GTK_RESPONSE_OK)
	{
		username = g_strstrip (g_strdup (gtk_entry_get_text (ctx->username)));
		password = gtk_entry_get_text (ctx->password);

		if (username[0] == '\0' || password[0] == '\0')
		{
			hildon_banner_show_information (GTK_WIDGET (dialog), NULL,
							_("Enter both your username "
							  "and password before saving."));
			return;
		} else
		{
			save_credentials (ctx->settings_file, username, password);
		}
		g_free (username);
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
	GtkWidget *table;
	GtkWidget *label_username;
	GtkWidget *label_password;
	gchar *usr, *settings_file;
	SettingsContext *ctx;

	setlocale(LC_ALL, "");
	textdomain(GETTEXT_PACKAGE);

	settings_file = g_build_filename (g_get_home_dir (),
					  MAFW_LASTFM_CREDENTIALS_FILE, NULL);

	dialog = gtk_dialog_new_with_buttons (_("Last.fm settings"),
					      GTK_WINDOW (data),
					      GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR,
					      /* NB! Leave untranslated. */
					      dgettext ("hildon-libs", "wdgt_bd_done"),
					      GTK_RESPONSE_OK,
					      NULL);

	table = gtk_table_new (2, 2, TRUE);
	label_username = gtk_label_new (_("Username:"));
	gtk_misc_set_alignment (GTK_MISC (label_username), 0.0, 0.5);
	username = hildon_entry_new (HILDON_SIZE_AUTO | HILDON_SIZE_FINGER_HEIGHT);

	gtk_table_attach_defaults (GTK_TABLE (table), label_username, 0, 1, 0, 1);

	gtk_table_attach_defaults (GTK_TABLE (table), username, 1, 2, 0, 1);
	label_password = gtk_label_new (_("Password:"));
	gtk_misc_set_alignment (GTK_MISC (label_password), 0.0, 0.5);
	password = hildon_entry_new (HILDON_SIZE_AUTO | HILDON_SIZE_FINGER_HEIGHT);
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (password),
					 HILDON_GTK_INPUT_MODE_FULL |
					 HILDON_GTK_INPUT_MODE_INVISIBLE);
	gtk_table_attach_defaults (GTK_TABLE (table), label_password, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE (table), password, 1, 2, 1, 2);

	if (g_file_test (settings_file, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
	{
		usr = load_username (settings_file);
		gtk_entry_set_text (GTK_ENTRY (username), usr);
		gtk_editable_select_region (GTK_EDITABLE (username), 0, -1);
		g_free (usr);
	}

	gtk_container_add (GTK_CONTAINER
			   (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
			   table);

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
