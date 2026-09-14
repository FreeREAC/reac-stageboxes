// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-list.h"

#include <stdio.h>

#include "rs-headamp.h"
#include "rs-pw.h"

typedef struct {
	RsPw       *pw;
	GMainLoop  *loop;
	int         rounds_left;
} ListRun;

static const char *onoff(int v)
{
	/* An UNSET cell prints as a dash, never as "off". The daemon asserting
	 * nothing for a channel is not the same as the daemon asserting zero, and
	 * printing one as the other is how a readback becomes a lie. */
	if (v < 0)
		return "—";
	return v ? "on" : "off";
}

static void print_box(RsBox *box)
{
	enum rs_avail avail = rs_box_availability(box);

	printf("segment %-16s door=%-6s model=%-14s width=%-6s link=%-12s\n",
	       rs_box_segment(box),
	       rs_box_is_sink_door(box) ? "sink" : "source",
	       rs_box_model(box), rs_box_width(box), rs_box_link_state(box));

	if (rs_box_base(box) >= 0)
		printf("        base=%d channels=%d caps=%s sens.max=%d%s\n",
		       rs_box_base(box), rs_box_channels(box),
		       *rs_box_caps(box) ? rs_box_caps(box) : "(none)",
		       rs_box_sens_max(box),
		       rs_box_sens_max_published(box) ? "" : " (assumed)");
	else
		printf("        base=none channels=%d caps=%s\n",
		       rs_box_channels(box),
		       *rs_box_caps(box) ? rs_box_caps(box) : "(none)");

	printf("        mac=%s fw=%s reac=%s master=%s refused=%s readback=%s -> %s\n",
	       rs_box_mac(box),
	       *rs_box_firmware(box) ? rs_box_firmware(box) : "(unanswered)",
	       *rs_box_reac_version(box) ? rs_box_reac_version(box) : "(unanswered)",
	       rs_box_master_state(box),
	       rs_box_refused(box) ? rs_box_refused(box) : "(absent)",
	       rs_box_has_readback(box) ? "yes" : "no",
	       rs_avail_id(avail));

	int channels = rs_box_channels(box);
	for (int i = 1; i <= channels; i++) {
		int ch      = rs_box_wire_ch(box, i);
		int phantom = rs_box_asserted(box, i, RS_HEADAMP_PHANTOM);
		int pad     = rs_box_asserted(box, i, RS_HEADAMP_PAD);
		int sens    = rs_box_asserted(box, i, RS_HEADAMP_SENS);

		printf("          in %2d  ch %3d  phantom=%-3s pad=%-3s sens=",
		       i, ch, onoff(phantom), onoff(pad));
		if (sens < 0)
			printf("—\n");
		else
			printf("%-2d (%d dBu)\n", sens,
			       rs_headamp_sens_dbu(sens, pad > 0, rs_box_sens_max(box)));
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
		fprintf(stderr, "%s\n", error ? error->message : "could not connect to PipeWire");
		return 1;
	}

	ListRun run = { .pw = pw, .loop = g_main_loop_new(NULL, FALSE), .rounds_left = 3 };
	rs_pw_sync(pw, on_round, &run);
	g_main_loop_run(run.loop);
	g_main_loop_unref(run.loop);

	GPtrArray *boxes = rs_pw_boxes(pw);
	guint bound = rs_pw_n_bound_nodes(pw);

	if (!boxes || boxes->len == 0) {
		/* THE DENOMINATOR IS PRINTED. A walk that bound nothing and a graph
		 * with no stageboxes in it produce the same empty list, and only this
		 * number tells them apart. */
		printf("no REAC segments found (%u node%s bound)\n",
		       bound, bound == 1 ? "" : "s");
		g_object_unref(pw);
		return 1;
	}

	g_ptr_array_sort(boxes, by_segment);
	printf("%u REAC segment%s (%u node%s bound)\n\n",
	       boxes->len, boxes->len == 1 ? "" : "s",
	       bound, bound == 1 ? "" : "s");
	for (guint i = 0; i < boxes->len; i++)
		print_box(g_ptr_array_index(boxes, i));

	g_object_unref(pw);
	return 0;
}
