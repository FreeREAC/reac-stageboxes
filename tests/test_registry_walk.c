// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* The registry walk, driven by a FAKE REGISTRY that delivers what a real one
 * delivers: a `global` event carrying only the subset — node.name, media.class,
 * object.serial — and the custom `reac.*` keys arriving later, in the node's
 * `info` event, after the bind.
 *
 * THIS IS THE REGRESSION. The first version of this application filtered on
 * `reac.segment` at global time and found nothing, on a live graph carrying
 * twelve nodes with that key: the window said "no REAC segment is present in
 * the PipeWire graph" while pw-dump listed them all. An empty result and a
 * broken search are the same picture, so the fixture below is built to be the
 * difference — globals WITHOUT the key, and the assertion that a segment is
 * found anyway.
 *
 * It drives the real rs_walk, not a restatement of it: the app and this test
 * call the same three entry points in the same order the PipeWire client does. */

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "rs-walk.h"

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

#define CHECK_INT(got, want) do { \
	int g_ = (int)(got), w_ = (int)(want); \
	if (g_ != w_) { \
		fprintf(stderr, "FAIL %s:%d: got %d, want %d\n", __FILE__, __LINE__, g_, w_); \
		failures++; \
	} \
} while (0)

#define CHECK_STR(got, want) do { \
	const char *g_ = (got), *w_ = (want); \
	if (!g_ || !g_str_equal(g_, w_)) { \
		fprintf(stderr, "FAIL %s:%d: got \"%s\", want \"%s\"\n", \
		        __FILE__, __LINE__, g_ ? g_ : "(null)", w_); \
		failures++; \
	} \
} while (0)

#define DICT(name, ...) \
	struct spa_dict_item name##_items[] = { __VA_ARGS__ }; \
	struct spa_dict name = SPA_DICT_INIT(name##_items, \
	                                     SPA_N_ELEMENTS(name##_items))

/* What the callbacks saw. */
typedef struct {
	guint added, changed, removed;
	char *last_added_segment;
	char *last_removed_segment;
} Seen;

static void on_added(RsBox *box, gpointer u)
{
	Seen *s = u;
	s->added++;
	g_free(s->last_added_segment);
	s->last_added_segment = g_strdup(rs_box_segment(box));
}
static void on_changed(RsBox *box G_GNUC_UNUSED, gpointer u) { ((Seen *)u)->changed++; }
static void on_removed(RsBox *box, gpointer u)
{
	Seen *s = u;
	s->removed++;
	g_free(s->last_removed_segment);
	s->last_removed_segment = g_strdup(rs_box_segment(box));
}

/* The walk must bind NODES, and must not be fooled into binding nothing. */
static void test_should_bind(void)
{
	CHECK(rs_walk_should_bind("PipeWire:Interface:Node"));
	CHECK(!rs_walk_should_bind("PipeWire:Interface:Port"));
	CHECK(!rs_walk_should_bind("PipeWire:Interface:Link"));
	CHECK(!rs_walk_should_bind("PipeWire:Interface:Device"));
	CHECK(!rs_walk_should_bind(NULL));
}

/* THE REGRESSION ITSELF: a global with no reac.* keys at all, then an info that
 * carries them. A walk that decides at global time reports zero here. */
static void test_custom_props_only_in_info(void)
{
	Seen seen = { 0 };
	RsWalk *w = rs_walk_new(on_added, on_changed, on_removed, &seen);

	/* Exactly what the registry hands over for reac-playback.enp131s0.11: the
	 * subset, with no reac.segment in it. */
	DICT(global,
	     { "node.name",     "reac-playback.enp131s0.11" },
	     { "media.class",   "Audio/Sink" },
	     { "object.serial", "1187" },
	     { "factory.id",    "18" });
	CHECK(spa_dict_lookup(&global, RS_PROP_SEGMENT) == NULL);   /* the fixture's point */

	rs_walk_global(w, 40, &global);
	/* Nothing is decided yet, and nothing may be reported yet. */
	CHECK_INT(seen.added, 0);
	CHECK_INT(rs_walk_boxes(w)->len, 0);
	/* But the node IS bound — the denominator that tells "found nothing" from
	 * "looked at nothing". */
	CHECK_INT(rs_walk_n_bound(w), 1);

	/* The info event, with the full dict. */
	DICT(info,
	     { "node.name",               "reac-playback.enp131s0.11" },
	     { "media.class",             "Audio/Sink" },
	     { RS_PROP_SEGMENT,           "enp131s0.11" },
	     { RS_PROP_LINK_STATE,        "established" },
	     { RS_PROP_BOX_MODEL,         "S-1608" },
	     { RS_PROP_BOX_WIDTH,         "16x8" },
	     { RS_PROP_HEADAMP_CHANNELS,  "16" },
	     { RS_PROP_HEADAMP_CAPS,      "phantom,pad,sens" },
	     { RS_PROP_HEADAMP_BASE,      "32" },
	     { RS_PROP_HEADAMP_SENS_MAX,  "55" },
	     { RS_PROP_HEADAMP_STATE,     "applied" },
	     { RS_PROP_HEADAMP_REFUSED,   "none" },
	     { RS_PROP_HEADAMP_ASSERTED,  "32:0=1,32:2=52" });
	rs_walk_info(w, 40, &info);

	CHECK_INT(seen.added, 1);
	CHECK_STR(seen.last_added_segment, "enp131s0.11");
	CHECK_INT(rs_walk_boxes(w)->len, 1);

	RsBox *box = rs_walk_box(w, 40);
	CHECK(box != NULL);
	CHECK(rs_box_is_sink_door(box));
	CHECK_STR(rs_box_model(box), "S-1608");
	CHECK_INT(rs_box_base(box), 32);
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_PHANTOM), 1);
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_SENS), 52);
	CHECK_INT(rs_box_availability(box), RS_AVAIL_OK);

	/* A second info with a changed assertion is a change, not a second box. */
	DICT(info2,
	     { RS_PROP_HEADAMP_ASSERTED, "32:0=0,32:2=52" });
	rs_walk_info(w, 40, &info2);
	CHECK_INT(seen.added, 1);
	CHECK_INT(seen.changed, 1);
	CHECK_INT(rs_walk_boxes(w)->len, 1);
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_PHANTOM), 0);

	/* An identical info is not a change — the badge timer republishes these. */
	rs_walk_info(w, 40, &info2);
	CHECK_INT(seen.changed, 1);

	g_free(seen.last_added_segment);
	g_free(seen.last_removed_segment);
	rs_walk_free(w);
}

/* A whole graph, the shape the desk actually has: twelve nodes carrying
 * reac.segment across three boxes, beside ordinary nodes that carry none. */
static void test_whole_graph(void)
{
	Seen seen = { 0 };
	RsWalk *w = rs_walk_new(on_added, on_changed, on_removed, &seen);

	const char *segments[3] = { "enp131s0.11", "enp131s0.12", "enp131s0.13" };
	const char *models[3]   = { "S-1608", "S-4000S-3208", "S-4000S-3208" };

	/* Phase one: every global, none of them carrying a reac key. */
	for (int i = 0; i < 3; i++) {
		DICT(gsink,  { "node.name", "reac-playback" }, { "media.class", "Audio/Sink" });
		DICT(gsrc,   { "node.name", "reac-capture" },  { "media.class", "Audio/Source" });
		rs_walk_global(w, 100 + i * 2, &gsink);
		rs_walk_global(w, 101 + i * 2, &gsrc);
	}
	DICT(galsa, { "node.name", "alsa_output.pci-0000_00_1f.3" },
	            { "media.class", "Audio/Sink" });
	rs_walk_global(w, 200, &galsa);

	CHECK_INT(rs_walk_boxes(w)->len, 0);     /* nothing decided at global time */
	CHECK_INT(rs_walk_n_bound(w), 7);

	/* Phase two: the infos. The sink and the source of one segment BOTH carry
	 * reac.segment only in the role that owns the door, which is how reac-pw
	 * stamps it; here both are given it, and the walk must keep them apart by
	 * media.class rather than merging or dropping one. */
	for (int i = 0; i < 3; i++) {
		DICT(isink,
		     { "media.class",            "Audio/Sink" },
		     { RS_PROP_SEGMENT,          segments[i] },
		     { RS_PROP_LINK_STATE,       "established" },
		     { RS_PROP_BOX_MODEL,        models[i] },
		     { RS_PROP_HEADAMP_CHANNELS, "16" },
		     { RS_PROP_HEADAMP_CAPS,     "phantom,pad,sens" },
		     { RS_PROP_HEADAMP_BASE,     "32" },
		     { RS_PROP_HEADAMP_ASSERTED, "" });
		rs_walk_info(w, 100 + i * 2, &isink);

		DICT(isrc,
		     { "media.class",      "Audio/Source" },
		     { RS_PROP_SEGMENT,    segments[i] },
		     { RS_PROP_BOX_MODEL,  models[i] });
		rs_walk_info(w, 101 + i * 2, &isrc);
	}
	DICT(ialsa, { "media.class", "Audio/Sink" },
	            { "node.name", "alsa_output.pci-0000_00_1f.3" });
	rs_walk_info(w, 200, &ialsa);

	/* THREE STAGEBOXES, not six doors. Both nodes of a segment are one box, and
	 * the ALSA node is not one at all. */
	CHECK_INT(rs_walk_boxes(w)->len, 3);
	CHECK_INT(seen.added, 3);
	CHECK(rs_walk_box(w, 200) == NULL);
	/* Either node reaches the same box, and that box's door is the SINK. */
	CHECK(rs_walk_box(w, 100) == rs_walk_box(w, 101));
	CHECK(rs_box_is_sink_door(rs_walk_box(w, 101)));
	CHECK_INT(rs_box_id(rs_walk_box(w, 101)), 100);

	/* AND THE SOURCE'S EMPTIES DID NOT BLANK THE SINK'S ANSWERS. The capture
	 * node publishes channels=0 and base=none; the box must still carry the
	 * sink's 16 and 32, or it would flip to `no-box` on a timer tick. */
	CHECK_INT(rs_box_channels(rs_walk_box(w, 100)), 16);
	CHECK_INT(rs_box_base(rs_walk_box(w, 100)), 32);
	CHECK_INT(rs_box_availability(rs_walk_box(w, 100)), RS_AVAIL_OK);

	/* A node leaving. Only a segment's departure is news. */
	rs_walk_remove(w, 200);
	CHECK_INT(seen.removed, 0);
	/* The SINK goes: the segment survives on its source node, and the box
	 * downgrades to no-control-door rather than vanishing. */
	rs_walk_remove(w, 100);
	CHECK_INT(seen.removed, 0);
	CHECK_INT(rs_walk_boxes(w)->len, 3);
	CHECK(!rs_box_is_sink_door(rs_walk_box(w, 101)));
	CHECK_INT(rs_box_availability(rs_walk_box(w, 101)), RS_AVAIL_NO_CONTROL_DOOR);
	/* The last node goes, and now the box does too. */
	rs_walk_remove(w, 101);
	CHECK_INT(seen.removed, 1);
	CHECK_STR(seen.last_removed_segment, "enp131s0.11");
	CHECK_INT(rs_walk_boxes(w)->len, 2);
	/* Removing twice is not two departures. */
	rs_walk_remove(w, 101);
	CHECK_INT(seen.removed, 1);

	g_free(seen.last_added_segment);
	g_free(seen.last_removed_segment);
	rs_walk_free(w);
}

/* Nodes that appear after the first pass, and an info for a node whose global
 * was never seen — neither may be dropped. */
static void test_late_and_unseen(void)
{
	Seen seen = { 0 };
	RsWalk *w = rs_walk_new(on_added, on_changed, on_removed, &seen);

	DICT(info,
	     { "media.class",            "Audio/Sink" },
	     { RS_PROP_SEGMENT,          "enp131s0.14" },
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_HEADAMP_CHANNELS, "8" },
	     { RS_PROP_HEADAMP_BASE,     "0" },
	     { RS_PROP_HEADAMP_ASSERTED, "" });
	/* No rs_walk_global first. */
	rs_walk_info(w, 77, &info);
	CHECK_INT(seen.added, 1);
	CHECK(rs_walk_box(w, 77) != NULL);

	/* A node that is not a segment when first seen and becomes one later — a
	 * role swap re-stamps the identity onto the other node. */
	DICT(g2, { "node.name", "reac-capture" }, { "media.class", "Audio/Source" });
	rs_walk_global(w, 88, &g2);
	DICT(i2_nokey, { "media.class", "Audio/Source" }, { "node.name", "reac-capture" });
	rs_walk_info(w, 88, &i2_nokey);
	CHECK_INT(seen.added, 1);                 /* still not a segment */
	DICT(i2_key, { "media.class", "Audio/Source" }, { RS_PROP_SEGMENT, "enp131s0.15" });
	rs_walk_info(w, 88, &i2_key);
	CHECK_INT(seen.added, 2);                 /* now it is */
	CHECK_STR(seen.last_added_segment, "enp131s0.15");

	/* An empty segment value is not a segment. */
	DICT(g3, { "media.class", "Audio/Sink" });
	rs_walk_global(w, 99, &g3);
	DICT(i3, { "media.class", "Audio/Sink" }, { RS_PROP_SEGMENT, "" });
	rs_walk_info(w, 99, &i3);
	CHECK_INT(seen.added, 2);

	g_free(seen.last_added_segment);
	g_free(seen.last_removed_segment);
	rs_walk_free(w);
}

/* THE SOURCE ARRIVES FIRST. A segment seen first on its capture node must be
 * UPGRADED when its sink turns up — not duplicated, and not left read-only. */
static void test_source_then_sink(void)
{
	Seen seen = { 0 };
	RsWalk *w = rs_walk_new(on_added, on_changed, on_removed, &seen);

	DICT(gsrc, { "node.name", "reac-capture" }, { "media.class", "Audio/Source" });
	DICT(gsink, { "node.name", "reac-playback" }, { "media.class", "Audio/Sink" });
	rs_walk_global(w, 10, &gsrc);
	rs_walk_global(w, 11, &gsink);

	DICT(isrc,
	     { "media.class",            "Audio/Source" },
	     { RS_PROP_SEGMENT,          "enp131s0.11" },
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_BOX_MODEL,        "S-1608" },
	     { RS_PROP_HEADAMP_CHANNELS, "0" },
	     { RS_PROP_HEADAMP_BASE,     "none" });
	rs_walk_info(w, 10, &isrc);
	CHECK_INT(seen.added, 1);
	CHECK_INT(rs_box_availability(rs_walk_box(w, 10)), RS_AVAIL_NO_CONTROL_DOOR);

	DICT(isink,
	     { "media.class",            "Audio/Sink" },
	     { RS_PROP_SEGMENT,          "enp131s0.11" },
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_BOX_MODEL,        "S-1608" },
	     { RS_PROP_HEADAMP_CHANNELS, "16" },
	     { RS_PROP_HEADAMP_CAPS,     "phantom,pad,sens" },
	     { RS_PROP_HEADAMP_BASE,     "32" },
	     { RS_PROP_HEADAMP_ASSERTED, "32:0=1" });
	rs_walk_info(w, 11, &isink);
	CHECK_INT(seen.added, 1);                 /* upgraded, not duplicated */
	CHECK_INT(rs_walk_boxes(w)->len, 1);
	CHECK(rs_box_is_sink_door(rs_walk_box(w, 10)));
	CHECK_INT(rs_box_id(rs_walk_box(w, 10)), 11);
	CHECK_INT(rs_box_availability(rs_walk_box(w, 10)), RS_AVAIL_OK);
	CHECK_INT(rs_box_asserted(rs_walk_box(w, 10), 1, RS_HEADAMP_PHANTOM), 1);

	/* AND THE SOURCE KEEPS REPUBLISHING. Its channels=0 / base=none must not
	 * reach the box now that the sink is the door — this is the regression that
	 * would take a working page to "no stagebox recognised" on a timer tick. */
	rs_walk_info(w, 10, &isrc);
	CHECK_INT(rs_box_channels(rs_walk_box(w, 10)), 16);
	CHECK_INT(rs_box_base(rs_walk_box(w, 10)), 32);
	CHECK_INT(rs_box_availability(rs_walk_box(w, 10)), RS_AVAIL_OK);

	g_free(seen.last_added_segment);
	g_free(seen.last_removed_segment);
	rs_walk_free(w);
}

/* media.class missing from the info dict falls back to the global's. */
static void test_media_class_fallback(void)
{
	Seen seen = { 0 };
	RsWalk *w = rs_walk_new(on_added, on_changed, on_removed, &seen);
	DICT(g, { "node.name", "reac-playback" }, { "media.class", "Audio/Sink" });
	rs_walk_global(w, 5, &g);
	DICT(i, { RS_PROP_SEGMENT, "enp131s0.11" }, { RS_PROP_LINK_STATE, "established" });
	rs_walk_info(w, 5, &i);
	CHECK(rs_walk_box(w, 5) != NULL);
	CHECK(rs_box_is_sink_door(rs_walk_box(w, 5)));
	g_free(seen.last_added_segment);
	g_free(seen.last_removed_segment);
	rs_walk_free(w);
}

int main(void)
{
	test_should_bind();
	test_custom_props_only_in_info();
	test_whole_graph();
	test_late_and_unseen();
	test_source_then_sink();
	test_media_class_fallback();

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("registry walk: ok\n");
	return 0;
}
