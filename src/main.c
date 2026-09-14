// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* reac-stageboxes — manage every REAC stagebox's preamps with nothing but
 * reac-pw installed.
 *
 * The daemon already publishes the whole door: per-input phantom / pad /
 * sensitivity as SPA_PARAM_Props on the node that carries `reac.segment`, and
 * the box badge, the preamp shape and the asserted state as node properties on
 * the same node. So this is a PipeWire client and nothing more — no REAC
 * protocol code, no socket, no REST, no second copy of any fact the daemon
 * holds. */

#include <adwaita.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <pipewire/pipewire.h>

#include "rs-pw.h"
#include "rs-window.h"

static void on_activate(GApplication *app, gpointer user_data G_GNUC_UNUSED)
{
	GtkWindow *existing = gtk_application_get_active_window(GTK_APPLICATION(app));
	if (existing) {
		gtk_window_present(existing);
		return;
	}

	/* A failed connection is carried into the window as a SENTENCE rather than
	 * becoming an empty list: "PipeWire is not running" and "there are no
	 * stageboxes" are different facts and an operator cannot tell them apart
	 * from a blank sidebar. */
	g_autoptr(GError) error = NULL;
	RsPw *pw = rs_pw_new(&error);

	RsWindow *win = rs_window_new(ADW_APPLICATION(app), pw,
	                              pw ? NULL : (error ? error->message : NULL));
	g_clear_object(&pw);
	gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	pw_init(&argc, &argv);

	g_autoptr(AdwApplication) app =
	        adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
	int status = g_application_run(G_APPLICATION(app), argc, argv);

	pw_deinit();
	return status;
}
