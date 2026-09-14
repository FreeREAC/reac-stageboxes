// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-list.h"

#include <glib/gi18n.h>
#include <stdio.h>

#include "rs-headamp.h"
#include "rs-pw.h"

typedef struct {
	RsPw       *pw;
	GMainLoop  *loop;
	int         rounds_left;
} ListRun;

/* An UNSET cell prints as a dash, never as "off". The daemon asserting nothing
 * for a channel is not the same as the daemon asserting zero, and printing one
 * as the other is how a readback becomes a lie. The dash is not translated: it
 * means "no answer" in every language. */
static const char *onoff(int v)
{
	if (v < 0)
		return "—";
	return v ? _("on") : _("off");
}

/* An empty published string means the box has not answered that poll yet — a
 * fact, and not a blank field. */
static const char *answered(const char *s)
{
	return (s && *s) ? s : _("(unanswered)");
}

static void print_box(RsBox *box)
{
	enum rs_avail avail = rs_box_availability(box);

	printf(_("segment %s — %s %s, link %s, door %s\n"),
	       rs_box_segment(box), rs_box_model(box), rs_box_width(box),
	       rs_box_link_state(box),
	       rs_box_is_sink_door(box) ? _("sink") : _("source"));

	if (rs_box_base(box) >= 0)
		printf(_("  base %d · %d inputs · caps %s · sensitivity max %d%s\n"),
		       rs_box_base(box), rs_box_channels(box),
		       *rs_box_caps(box) ? rs_box_caps(box) : _("(none)"),
		       rs_box_sens_max(box),
		       rs_box_sens_max_published(box) ? "" : _(" (assumed)"));
	else
		printf(_("  no base · %d inputs · caps %s\n"),
		       rs_box_channels(box),
		       *rs_box_caps(box) ? rs_box_caps(box) : _("(none)"));

	printf(_("  mac %s · firmware %s · REAC %s · master %s · refused %s · readback %s\n"),
	       rs_box_mac(box),
	       answered(rs_box_firmware(box)),
	       answered(rs_box_reac_version(box)),
	       rs_box_master_state(box),
	       rs_box_refused(box) ? rs_box_refused(box) : _("(not published)"),
	       rs_box_has_readback(box) ? _("yes") : _("no"));

	/* The status line carries the SENTENCE the window shows, not a code, so a
	 * terminal and the page agree word for word; the untranslated id rides
	 * along in brackets for a log or a bug report. */
	const char *why = rs_avail_sentence(avail);
	if (why)
		printf(_("  status: %s [%s]\n"), why, rs_avail_id(avail));
	else
		printf(_("  status: preamps ready [%s]\n"), rs_avail_id(avail));

	int channels = rs_box_channels(box);
	for (int i = 1; i <= channels; i++) {
		int ch      = rs_box_wire_ch(box, i);
		int phantom = rs_box_asserted(box, i, RS_HEADAMP_PHANTOM);
		int pad     = rs_box_asserted(box, i, RS_HEADAMP_PAD);
		int sens    = rs_box_asserted(box, i, RS_HEADAMP_SENS);

		g_autofree char *sens_str = NULL;
		if (sens < 0)
			sens_str = g_strdup("—");
		else
			sens_str = g_strdup_printf(_("%d (%d dBu)"), sens,
			                           rs_headamp_sens_dbu(sens, pad > 0,
			                                               rs_box_sens_max(box)));

		printf(_("    input %2d  wire channel %3d  phantom %-4s  pad %-4s  sensitivity %s\n"),
		       i, ch, onoff(phantom), onoff(pad), sens_str);
	}
	printf("\n");
}

static gint by_segment(gconstpointer a, gconstpointer b)
{
	RsBox *x = *(RsBox *const *)a, *y = *(RsBox *const *)b;
	return g_strcmp0(rs_box_segment(x), rs_box_segment(y));
}

static void on_round(RsPw *pw, gpointer user_data)
{
	ListRun *run = user_data;
	/* The first roundtrip completes when every global has arrived and every
	 * bind has gone out; the second when those binds' info events are back. A
	 * third costs nothing and covers a node bound late in the second. */
	if (--run->rounds_left > 0) {
		rs_pw_sync(pw, on_round, run);
		return;
	}
	g_main_loop_quit(run->loop);
}

int rs_list_run(void)
{
	g_autoptr(GError) error = NULL;
	RsPw *pw = rs_pw_new(&error);
	if (!pw) {
		fprintf(stderr, "%s\n",
		        error ? error->message : _("Could not connect to PipeWire."));
		return 1;
	}

	ListRun run = { .pw = pw, .loop = g_main_loop_new(NULL, FALSE), .rounds_left = 3 };
	rs_pw_sync(pw, on_round, &run);
	g_main_loop_run(run.loop);
	g_main_loop_unref(run.loop);

	GPtrArray *boxes = rs_pw_boxes(pw);
	guint bound = rs_pw_n_bound_nodes(pw);

	/* THE DENOMINATOR IS ALWAYS PRINTED. A walk that bound nothing and a graph
	 * with no stageboxes in it produce the same empty list, and only this
	 * number tells them apart. */
	g_autofree char *nodes_str =
	        g_strdup_printf(ngettext("%u node bound", "%u nodes bound", bound), bound);

	if (!boxes || boxes->len == 0) {
		printf(_("No REAC segments found (%s).\n"), nodes_str);
		g_object_unref(pw);
		return 1;
	}

	g_ptr_array_sort(boxes, by_segment);
	g_autofree char *segs_str =
	        g_strdup_printf(ngettext("%u REAC segment", "%u REAC segments", boxes->len),
	                        boxes->len);
	printf(_("%s (%s).\n\n"), segs_str, nodes_str);
	for (guint i = 0; i < boxes->len; i++)
		print_box(g_ptr_array_index(boxes, i));

	g_object_unref(pw);
	return 0;
}
