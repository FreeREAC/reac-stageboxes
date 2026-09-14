// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-box — one REAC segment, as the graph describes it.
 *
 * A segment has exactly ONE door in the PipeWire graph: the node that carries
 * `reac.segment`. In the master role that is reac-pw's Audio/Sink
 * (`reac-playback[.<segment>]`) — the node that both publishes the box badge
 * and accepts the head-amp control keys. In the slave role the sink does not
 * exist at all and the Audio/Source (`reac-capture.<segment>`) carries the
 * identity instead. So `is_sink_door` is not cosmetic: it is the difference
 * between a segment whose preamps we can move and one whose preamps are
 * somebody else's.
 *
 * This object holds NO value of its own. Every field is the last thing the
 * daemon published, and the asserted table is the daemon's shadow table, read
 * back. reac-pw is the one writer (design of record §3b); a second copy here
 * would be a second ledger with neither door announcing the other.
 *
 * GLib only — no GTK, no PipeWire — so a test drives it with a synthetic
 * property dict shaped exactly like a node info event's. */
#ifndef RS_BOX_H
#define RS_BOX_H

#include <glib-object.h>
#include <spa/utils/dict.h>

#include "rs-headamp.h"

G_BEGIN_DECLS

#define RS_TYPE_BOX (rs_box_get_type())
G_DECLARE_FINAL_TYPE(RsBox, rs_box, RS, BOX, GObject)

/* `segment` is the segment name; `id` is the PipeWire global id of the door
 * node, which is what a write is addressed to. */
RsBox      *rs_box_new(uint32_t id, const char *segment, gboolean is_sink_door);

/* Merge a node property dict in. PipeWire delivers node properties as a full
 * dict on every info event, but a key that vanished from the dict means "not
 * stamped this time", not "cleared" — reac-pw itself relies on
 * pw_stream_update_properties MERGING — so absent keys hold their last value
 * and only a present key moves one. The one exception is the readback trio,
 * whose ABSENCE is itself the fact (see rs_box_has_readback).
 *
 * Returns TRUE when anything the surface renders actually changed, so a caller
 * can skip a rebuild on the 200 ms badge timer's no-op stamps. */
gboolean    rs_box_update_from_dict(RsBox *self, const struct spa_dict *props);

uint32_t    rs_box_id(RsBox *self);
const char *rs_box_segment(RsBox *self);
gboolean    rs_box_is_sink_door(RsBox *self);
const char *rs_box_model(RsBox *self);          /* "none" until recognised */
const char *rs_box_width(RsBox *self);          /* "16x8" … "0x0" */
const char *rs_box_mac(RsBox *self);
const char *rs_box_firmware(RsBox *self);       /* "" until the box answers */
const char *rs_box_reac_version(RsBox *self);   /* "" until the box answers */
const char *rs_box_link_state(RsBox *self);
const char *rs_box_master_state(RsBox *self);   /* us | foreign | none */
const char *rs_box_role_state(RsBox *self);
const char *rs_box_caps(RsBox *self);
const char *rs_box_refused(RsBox *self);        /* NULL when the key is absent */
int         rs_box_channels(RsBox *self);       /* preamp-capable box inputs, 0 = none */
int         rs_box_base(RsBox *self);           /* chassis strap, -1 = no wire address */
int         rs_box_sens_max(RsBox *self);       /* published, else the default */
gboolean    rs_box_sens_max_published(RsBox *self);

/* Whether this daemon publishes `reac.headamp.asserted` at all. FALSE means a
 * control could be moved and never confirmed, which this application renders as
 * read-only rows rather than as a switch that might be lying. */
gboolean    rs_box_has_readback(RsBox *self);

/* The asserted value for one cell, or -1 when the daemon asserts nothing for it.
 * Never defaulted to 0: an absent cell is "unset", and a measurement read with a
 * numeric default is a measurement of nothing that reads as a number. */
int         rs_box_asserted(RsBox *self, int box_input, enum rs_headamp_param p);

/* The wire channel for a box input on this box, or -1 without a base. */
int         rs_box_wire_ch(RsBox *self, int box_input);

enum rs_avail rs_box_availability(RsBox *self);

/* A one-line sidebar subtitle: model, width, firmware · REAC version. Free with
 * g_free. Translated through the caller's domain. */
char       *rs_box_subtitle(RsBox *self);

G_END_DECLS

#endif /* RS_BOX_H */
