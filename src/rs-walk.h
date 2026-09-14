// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-walk — which nodes in the graph are stagebox segments.
 *
 * THE REGISTRY'S `global` EVENT CARRIES A SUBSET OF A NODE'S PROPERTIES.
 * node.name, media.class, object.serial and a handful more come through it;
 * every custom key — `reac.segment`, the box badge, the whole `reac.headamp.*`
 * set — arrives only in the node's `info` event, after `pw_registry_bind`. A
 * walk that filters on `reac.segment` at global time therefore finds NOTHING,
 * on a graph with twelve segment nodes in it, and an empty sidebar is
 * indistinguishable from a graph with no boxes. Measured on the desk against
 * reac-pw 1.0.5: twelve nodes carrying reac.segment, zero shown.
 *
 * So the rule is: BIND EVERY NODE, decide from `info->props`. Binding is the
 * only way to see the keys the decision needs, which makes "bind, then filter"
 * not an optimisation choice but the only correct order. Filtering on a
 * node.name prefix at global time would work today and break the first time a
 * node is named differently; the segment key is the contract, so the segment
 * key is what decides.
 *
 * This module is the decision and the bookkeeping, with the proxy mechanics
 * left to rs-pw: glib and SPA headers only, no libpipewire. That is what lets
 * a test drive the REAL walk from a fake registry — one whose globals carry
 * only the subset a real one carries — instead of a second copy of it. */
#ifndef RS_WALK_H
#define RS_WALK_H

#include <glib-object.h>
#include <spa/utils/dict.h>

#include "rs-box.h"

G_BEGIN_DECLS

typedef struct _RsWalk RsWalk;

typedef void (*RsWalkBoxFn)(RsBox *box, gpointer user_data);

RsWalk *rs_walk_new(RsWalkBoxFn added, RsWalkBoxFn changed, RsWalkBoxFn removed,
                    gpointer user_data);
void    rs_walk_free(RsWalk *self);

/* Should the caller bind this global? TRUE for every node, because the keys
 * that decide membership are not visible until it is bound. `type` is the
 * registry's interface type string. */
gboolean rs_walk_should_bind(const char *type);

/* A node global appeared. `global_props` is the registry's subset and may be
 * NULL; nothing is decided here, and no box is created. What is kept is the
 * media.class, as a FALLBACK for the one case where the info event does not
 * repeat it. */
void rs_walk_global(RsWalk *self, uint32_t id, const struct spa_dict *global_props);

/* A node's info arrived, with its full property dict. This is where a node
 * becomes a segment — or does not. Safe to call for an id the walk never saw a
 * global for. */
void rs_walk_info(RsWalk *self, uint32_t id, const struct spa_dict *info_props);

/* A global went away. Emits `removed` only if the node had become a segment. */
void rs_walk_remove(RsWalk *self, uint32_t id);

/* The segments found so far, as RsBox*, owned by the walk. ONE PER SEGMENT, not
 * one per node: reac-pw 1.0.5 stamps `reac.segment` on both the master's sink
 * and its capture source, and those two nodes are one stagebox. The box reads
 * from, and is written to, whichever of them is its door — a sink if there is
 * one, because only the sink carries the head-amp capability, the base and the
 * readback (the source publishes `channels=0` and `base=none`, which would blank
 * them if the two were merged). */
GPtrArray *rs_walk_boxes(RsWalk *self);

/* The box whose segment this bound node belongs to, door or not, or NULL if the
 * node is not part of a segment. */
RsBox *rs_walk_box(RsWalk *self, uint32_t id);

/* How many nodes are bound, segment or not — the denominator that tells a
 * "found nothing" apart from a "looked at nothing". */
guint rs_walk_n_bound(RsWalk *self);

G_END_DECLS

#endif /* RS_WALK_H */
