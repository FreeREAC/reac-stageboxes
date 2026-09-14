// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* The model, driven by a FAKE node — a property dict shaped exactly like the
 * one a `pw_node_info` event carries.
 *
 * This is as far as a mock gets without a session bus: `struct spa_dict` is the
 * literal type PipeWire hands to the info callback, so feeding one by hand
 * exercises the whole read path — merge semantics, the base parse, the
 * readback's presence-versus-content distinction, and the availability rule —
 * with no daemon, no graph and no box.
 *
 * WHAT IT DOES NOT COVER, and what is therefore owed: the registry walk (which
 * nodes are segments), the proxy binding, and that `pw_node_set_param` reaches
 * reac-pw at all. Those need a PipeWire server, and a fake registry would be a
 * second implementation of the thing under test. They are proven on the desk,
 * against the live boxes, by the operator. */

#include <glib.h>
#include <stdio.h>

#include "rs-box.h"

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

#define CHECK_INT(got, want) do { \
	int g_ = (got), w_ = (want); \
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

/* Build a spa_dict from (key, value) pairs, the way a node info event delivers
 * one. The items array must outlive the dict, so callers keep both on the
 * stack for the duration of the update. */
#define DICT(name, ...) \
	struct spa_dict_item name##_items[] = { __VA_ARGS__ }; \
	struct spa_dict name = SPA_DICT_INIT(name##_items, \
	                                     SPA_N_ELEMENTS(name##_items))

/* An enrolled S-1608 on a segment we master: the full answer set. */
static void test_enrolled_s1608(void)
{
	RsBox *box = rs_box_new(42, "A", TRUE);

	DICT(d,
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_BOX_MODEL,        "S-1608" },
	     { RS_PROP_BOX_WIDTH,        "16x8" },
	     { RS_PROP_BOX_MAC,          "00:11:22:33:44:55" },
	     { RS_PROP_BOX_FIRMWARE,     "2.200" },
	     { RS_PROP_BOX_REAC_VERSION, "2.302" },
	     { RS_PROP_MASTER_STATE,     "us" },
	     { RS_PROP_HEADAMP_CHANNELS, "16" },
	     { RS_PROP_HEADAMP_CAPS,     "phantom,pad,sens" },
	     { RS_PROP_HEADAMP_BASE,     "32" },
	     { RS_PROP_HEADAMP_ASSERTED, "32:0=1,32:2=52,34:1=1" },
	     { RS_PROP_HEADAMP_REFUSED,  "none" });
	CHECK(rs_box_update_from_dict(box, &d));

	CHECK_STR(rs_box_model(box), "S-1608");
	CHECK_STR(rs_box_firmware(box), "2.200");
	/* The firmware and the REAC version are different numbers and one is never
	 * substituted for the other. */
	CHECK_STR(rs_box_reac_version(box), "2.302");
	CHECK_INT(rs_box_channels(box), 16);
	CHECK_INT(rs_box_base(box), 32);
	CHECK(rs_box_has_readback(box));
	CHECK_INT(rs_box_availability(box), RS_AVAIL_OK);

	/* base + (input - 1): input 1 is wire channel 32, input 16 is 47. */
	CHECK_INT(rs_box_wire_ch(box, 1), 32);
	CHECK_INT(rs_box_wire_ch(box, 16), 47);
	CHECK_INT(rs_box_wire_ch(box, 17), -1);

	/* The asserted table, read by BOX INPUT — the number on the panel. */
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_PHANTOM), 1);
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_SENS),    52);
	CHECK_INT(rs_box_asserted(box, 3, RS_HEADAMP_PAD),     1);
	/* Unset is -1, never 0. */
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_PAD),     -1);
	CHECK_INT(rs_box_asserted(box, 9, RS_HEADAMP_PHANTOM), -1);

	/* A second identical publish is not a change: the badge timer stamps these
	 * every 200 ms and a rebuild per stamp would repaint the page under the
	 * operator's hand. */
	CHECK(!rs_box_update_from_dict(box, &d));

	/* The default ceiling stands until a node publishes one. */
	CHECK_INT(rs_box_sens_max(box), RS_HEADAMP_SENS_MAX_DEFAULT);
	CHECK(!rs_box_sens_max_published(box));
	DICT(m, { RS_PROP_HEADAMP_SENS_MAX, "40" });
	CHECK(rs_box_update_from_dict(box, &m));
	CHECK_INT(rs_box_sens_max(box), 40);
	CHECK(rs_box_sens_max_published(box));

	/* A PARTIAL publish MERGES. reac-pw stamps a subset on most publishes and
	 * relies on pw_stream_update_properties merging, so an absent key holds its
	 * last value rather than clearing the field. */
	DICT(p, { RS_PROP_HEADAMP_ASSERTED, "32:0=0,32:2=52,34:1=1" });
	CHECK(rs_box_update_from_dict(box, &p));
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_PHANTOM), 0);
	CHECK_STR(rs_box_model(box), "S-1608");
	CHECK_INT(rs_box_channels(box), 16);

	g_object_unref(box);
}

/* A box that dropped: the channels and the base go back to their "nothing
 * known" spellings, and the page must say so instead of addressing base 0. */
static void test_box_dropped(void)
{
	RsBox *box = rs_box_new(42, "A", TRUE);
	DICT(up,
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_HEADAMP_CHANNELS, "16" },
	     { RS_PROP_HEADAMP_BASE,     "32" },
	     { RS_PROP_HEADAMP_ASSERTED, "" });
	rs_box_update_from_dict(box, &up);
	CHECK_INT(rs_box_availability(box), RS_AVAIL_OK);
	/* An EMPTY asserted string is a daemon asserting nothing — a real answer,
	 * and not the same as the key being absent. */
	CHECK(rs_box_has_readback(box));
	CHECK_INT(rs_box_asserted(box, 1, RS_HEADAMP_PHANTOM), -1);

	DICT(down,
	     { RS_PROP_LINK_STATE,       "dropped" },
	     { RS_PROP_HEADAMP_CHANNELS, "0" },
	     { RS_PROP_HEADAMP_BASE,     "none" });
	CHECK(rs_box_update_from_dict(box, &down));
	CHECK_INT(rs_box_base(box), -1);
	CHECK_INT(rs_box_wire_ch(box, 1), -1);
	CHECK_INT(rs_box_availability(box), RS_AVAIL_NO_BOX);
	g_object_unref(box);
}

/* A box strapped to REAC master mode. reac-pw publishes channels 0 and base
 * none on such a segment rather than a capability the wire cannot carry. */
static void test_box_master(void)
{
	RsBox *box = rs_box_new(7, "B", TRUE);
	DICT(d,
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_BOX_MODEL,        "S-1608" },
	     { RS_PROP_HEADAMP_CHANNELS, "0" },
	     { RS_PROP_HEADAMP_BASE,     "none" },
	     { RS_PROP_HEADAMP_REFUSED,  "box-master" },
	     { RS_PROP_HEADAMP_ASSERTED, "" });
	rs_box_update_from_dict(box, &d);
	CHECK_INT(rs_box_availability(box), RS_AVAIL_BOX_MASTER);
	CHECK_STR(rs_box_refused(box), "box-master");
	g_object_unref(box);
}

/* We are this segment's slave: the door is the Audio/Source, reac-pw builds no
 * sink at all in that role, and there is nowhere to write. */
static void test_slave_segment(void)
{
	RsBox *box = rs_box_new(9, "C", FALSE);
	DICT(d,
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_BOX_MODEL,        "S-1608" },
	     { RS_PROP_MASTER_STATE,     "foreign" },
	     { RS_PROP_HEADAMP_ASSERTED, "" });
	rs_box_update_from_dict(box, &d);
	CHECK_INT(rs_box_availability(box), RS_AVAIL_NO_CONTROL_DOOR);
	CHECK(!rs_box_is_sink_door(box));
	g_object_unref(box);
}

/* A reac-pw that predates the readback lane: everything else is there, but
 * nothing publishes what is asserted, so a change could never be confirmed. */
static void test_no_readback(void)
{
	RsBox *box = rs_box_new(11, "A", TRUE);
	DICT(d,
	     { RS_PROP_LINK_STATE,       "established" },
	     { RS_PROP_BOX_MODEL,        "S-0808" },
	     { RS_PROP_HEADAMP_CHANNELS, "8" },
	     { RS_PROP_HEADAMP_CAPS,     "phantom,pad,sens" },
	     { RS_PROP_HEADAMP_BASE,     "0" });
	rs_box_update_from_dict(box, &d);
	CHECK(!rs_box_has_readback(box));
	CHECK(rs_box_refused(box) == NULL);
	CHECK_INT(rs_box_availability(box), RS_AVAIL_NO_READBACK);
	/* Base 0 is a REAL base — an S-0808 straps there — and must not be
	 * confused with "no base". */
	CHECK_INT(rs_box_base(box), 0);
	CHECK_INT(rs_box_wire_ch(box, 1), 0);
	CHECK_INT(rs_box_wire_ch(box, 8), 7);
	g_object_unref(box);
}

int main(void)
{
	test_enrolled_s1608();
	test_box_dropped();
	test_box_master();
	test_slave_segment();
	test_no_readback();

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("box model: ok\n");
	return 0;
}
