// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-walk.h"

#include <string.h>

/* One bound node. `segment` is filled in the moment its info reveals one. */
typedef struct {
	uint32_t  id;
	char     *media_class;   /* from the global, as a fallback */
	char     *segment;       /* NULL until the info says otherwise */
	gboolean  is_sink;
} Node;

/* One SEGMENT — one stagebox, however many nodes carry its name. */
typedef struct {
	char      *segment;
	RsBox     *box;
	GPtrArray *nodes;        /* Node*, borrowed from `nodes` */
} Segment;

struct _RsWalk {
	GHashTable  *nodes;      /* id -> Node* (owning) */
	GHashTable  *segments;   /* segment name -> Segment* (owning) */
	GPtrArray   *boxes;      /* RsBox*, borrowed */
	RsWalkBoxFn  added, changed, removed;
	gpointer     user_data;
};

static void node_free(gpointer p)
{
	Node *n = p;
	g_free(n->media_class);
	g_free(n->segment);
	g_free(n);
}

static void segment_free(gpointer p)
{
	Segment *s = p;
	g_free(s->segment);
	g_clear_object(&s->box);
	g_clear_pointer(&s->nodes, g_ptr_array_unref);
	g_free(s);
}

RsWalk *rs_walk_new(RsWalkBoxFn added, RsWalkBoxFn changed, RsWalkBoxFn removed,
                    gpointer user_data)
{
	RsWalk *self = g_new0(RsWalk, 1);
	self->nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                    NULL, node_free);
	self->segments = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                       NULL, segment_free);
	self->boxes = g_ptr_array_new();
	self->added = added;
	self->changed = changed;
	self->removed = removed;
	self->user_data = user_data;
	return self;
}

void rs_walk_free(RsWalk *self)
{
	if (!self)
		return;
	g_clear_pointer(&self->boxes, g_ptr_array_unref);
	g_clear_pointer(&self->segments, g_hash_table_unref);
	g_clear_pointer(&self->nodes, g_hash_table_unref);
	g_free(self);
}

gboolean rs_walk_should_bind(const char *type)
{
	/* Every node, and only nodes. The membership keys are invisible until the
	 * bind, so there is nothing to filter on yet — see the header. */
	return type != NULL && strcmp(type, "PipeWire:Interface:Node") == 0;
}

void rs_walk_global(RsWalk *self, uint32_t id, const struct spa_dict *global_props)
{
	g_return_if_fail(self != NULL);
	if (g_hash_table_contains(self->nodes, GUINT_TO_POINTER(id)))
		return;

	Node *n = g_new0(Node, 1);
	n->id = id;
	if (global_props)
		n->media_class = g_strdup(spa_dict_lookup(global_props, "media.class"));
	g_hash_table_insert(self->nodes, GUINT_TO_POINTER(id), n);
}

/* The node a segment should be read from and written to: a sink if it has one,
 * otherwise whatever it has. Returns NULL for a segment with no nodes left. */
static Node *pick_door(Segment *seg)
{
	Node *fallback = NULL;
	for (guint i = 0; i < seg->nodes->len; i++) {
		Node *n = g_ptr_array_index(seg->nodes, i);
		if (n->is_sink)
			return n;
		if (!fallback)
			fallback = n;
	}
	return fallback;
}

void rs_walk_info(RsWalk *self, uint32_t id, const struct spa_dict *info_props)
{
	g_return_if_fail(self != NULL);
	if (!info_props)
		return;

	Node *n = g_hash_table_lookup(self->nodes, GUINT_TO_POINTER(id));
	if (!n) {
		/* An info for a node we never saw a global for. Take it anyway rather
		 * than dropping a segment on an ordering assumption. */
		rs_walk_global(self, id, NULL);
		n = g_hash_table_lookup(self->nodes, GUINT_TO_POINTER(id));
	}

	/* THE DECISION, and the only place it is made: `reac.segment` is not in the
	 * registry's global props, so this is the first moment it can be read.
	 *
	 * A NODE ALREADY KNOWN TO BE A SEGMENT STAYS ONE. A later info need not
	 * repeat every key, and requiring it here made an update carrying only
	 * `reac.headamp.asserted` — which is exactly what a knob turn produces —
	 * fall on the floor: the readback would never move and every write would
	 * look unanswered. */
	const char *segment = spa_dict_lookup(info_props, RS_PROP_SEGMENT);
	if (!segment || !*segment)
		segment = n->segment;
	if (!segment || !*segment)
		return;                  /* a node, not a segment — a normal outcome */

	/* Which door this node is. The info dict is authoritative; the global's
	 * media.class stands in only if neither this info nor an earlier one said. */
	const char *mc = spa_dict_lookup(info_props, "media.class");
	if (!mc)
		mc = n->media_class;
	if (mc)
		n->is_sink = strcmp(mc, "Audio/Sink") == 0;

	if (!n->segment)
		n->segment = g_strdup(segment);

	Segment *seg = g_hash_table_lookup(self->segments, segment);
	if (!seg) {
		seg = g_new0(Segment, 1);
		seg->segment = g_strdup(segment);
		seg->nodes = g_ptr_array_new();
		g_ptr_array_add(seg->nodes, n);
		seg->box = rs_box_new(id, segment, n->is_sink);
		rs_box_update_from_dict(seg->box, info_props);
		g_hash_table_insert(self->segments, seg->segment, seg);
		g_ptr_array_add(self->boxes, seg->box);
		if (self->added)
			self->added(seg->box, self->user_data);
		return;
	}

	if (!g_ptr_array_find(seg->nodes, n, NULL))
		g_ptr_array_add(seg->nodes, n);

	/* ONE SEGMENT IS ONE STAGEBOX. Its properties come from its door and from
	 * nowhere else: the source node publishes the badge but `channels=0` and
	 * `base=none`, so merging it in would blank the very capability the sink
	 * just published — the box would go from `ok` to `no-box` on a timer tick. */
	Node *door = pick_door(seg);
	gboolean changed = FALSE;
	if (n == door) {
		if (rs_box_id(seg->box) != id
		    || rs_box_is_sink_door(seg->box) != n->is_sink) {
			rs_box_set_door(seg->box, id, n->is_sink);
			changed = TRUE;
		}
		changed |= rs_box_update_from_dict(seg->box, info_props);
	}
	if (changed && self->changed)
		self->changed(seg->box, self->user_data);
}

void rs_walk_remove(RsWalk *self, uint32_t id)
{
	g_return_if_fail(self != NULL);
	Node *n = g_hash_table_lookup(self->nodes, GUINT_TO_POINTER(id));
	if (!n)
		return;

	Segment *seg = n->segment
	             ? g_hash_table_lookup(self->segments, n->segment) : NULL;
	if (!seg) {
		g_hash_table_remove(self->nodes, GUINT_TO_POINTER(id));
		return;
	}

	g_ptr_array_remove_fast(seg->nodes, n);
	g_hash_table_remove(self->nodes, GUINT_TO_POINTER(id));

	Node *door = pick_door(seg);
	if (door) {
		/* The segment is still there on its other node. Re-point the box; its
		 * next info refreshes what that node publishes. */
		rs_box_set_door(seg->box, door->id, door->is_sink);
		if (self->changed)
			self->changed(seg->box, self->user_data);
		return;
	}

	RsBox *box = g_object_ref(seg->box);
	g_ptr_array_remove_fast(self->boxes, seg->box);
	g_hash_table_remove(self->segments, seg->segment);
	if (self->removed)
		self->removed(box, self->user_data);
	g_object_unref(box);
}

GPtrArray *rs_walk_boxes(RsWalk *self)
{
	g_return_val_if_fail(self != NULL, NULL);
	return self->boxes;
}

RsBox *rs_walk_box(RsWalk *self, uint32_t id)
{
	g_return_val_if_fail(self != NULL, NULL);
	Node *n = g_hash_table_lookup(self->nodes, GUINT_TO_POINTER(id));
	if (!n || !n->segment)
		return NULL;
	Segment *seg = g_hash_table_lookup(self->segments, n->segment);
	return seg ? seg->box : NULL;
}

guint rs_walk_n_bound(RsWalk *self)
{
	g_return_val_if_fail(self != NULL, 0);
	return g_hash_table_size(self->nodes);
}
