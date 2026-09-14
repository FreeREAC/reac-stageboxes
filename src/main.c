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

#include "rs-list.h"
#include "rs-pw.h"
#include "rs-window.h"

/* WHERE THE CATALOGUES ARE.
 *
 * `LOCALEDIR` is the installed path, and it is right for an installed binary and
 * wrong for every other way this program is ever run. A build-tree run —
 * `b/reac-stageboxes`, which is how it is tested and how it was first shown to
 * the operator — binds a directory that holds no .mo at all, so gettext falls
 * back to the msgids and a Catalan desktop gets an English window. Nothing
 * fails, nothing is logged: the untranslated string IS the fallback.
 *
 * So the search is: an explicit override first, then the catalogues meson built
 * beside this binary, then the installed path. Each candidate is accepted only
 * if a catalogue is actually THERE — a directory that exists but holds no .mo
 * would silently swallow the next candidate. */
static const char *locale_dir(void)
{
	static char *resolved;
	if (resolved)
		return resolved;

	/* Documented for developers: point it anywhere, e.g. at another build tree. */
	const char *env = g_getenv("REAC_STAGEBOXES_LOCALEDIR");
	if (env && *env) {
		resolved = g_strdup(env);
		return resolved;
	}

	/* Uninstalled: meson's i18n.gettext() writes
	 * <builddir>/po/<lang>/LC_MESSAGES/<domain>.mo, and the binary sits at
	 * <builddir>/reac-stageboxes. */
	g_autofree char *exe = g_file_read_link("/proc/self/exe", NULL);
	if (exe) {
		g_autofree char *dir = g_path_get_dirname(exe);
		g_autofree char *candidate = g_build_filename(dir, "po", NULL);
		/* Probe for a catalogue, not for the directory: an empty `po/` would
		 * otherwise shadow the installed one. Catalan is the probe because it
		 * is the translation this must not lose; `en` would pass on a build
		 * that dropped `ca`. */
		g_autofree char *probe = g_build_filename(candidate, "ca", "LC_MESSAGES",
		                                          GETTEXT_PACKAGE ".mo", NULL);
		if (g_file_test(probe, G_FILE_TEST_EXISTS)) {
			resolved = g_steal_pointer(&candidate);
			return resolved;
		}
	}

	resolved = g_strdup(LOCALEDIR);
	return resolved;
}

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
	bindtextdomain(GETTEXT_PACKAGE, locale_dir());
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	pw_init(&argc, &argv);

	/* --list: the same registry walk, printed, with no display and no write.
	 * It is how the walk is proven against a live graph over a terminal — the
	 * one part of this application that no offline test can reach. Handled
	 * before the application object exists, so it needs no session bus and no
	 * DISPLAY. */
	for (int i = 1; i < argc; i++) {
		if (g_strcmp0(argv[i], "--list") == 0) {
			int rc = rs_list_run();
			pw_deinit();
			return rc;
		}
	}

	g_autoptr(AdwApplication) app =
	        adw_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
	int status = g_application_run(G_APPLICATION(app), argc, argv);

	pw_deinit();
	return status;
}
