// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-pw.h"
#include "rs-props.h"

#include <glib-unix.h>
#include <glib/gi18n.h>

#include <pipewire/pipewire.h>
#include <spa/pod/builder.h>
#include <spa/utils/result.h>

/* One bound node: the segment's door, plus the model object the surface renders. */
typedef struct {
	RsPw            *owner;
	uint32_t         id;
	struct pw_node  *proxy;
	struct spa_hook  listener;
	struct spa_hook  proxy_listener;
	RsBox           *box;
} NodeEntry;

struct _RsPw {
	GObject parent_instance;

	struct pw_loop     *loop;
	struct pw_context  *context;
	struct pw_core     *core;
	struct pw_registry *registry;
	struct spa_hook     core_listener;
	struct spa_hook     registry_listener;
	guint               fd_source;
	gboolean            entered;

	GHashTable *nodes;      /* uint32 id -> NodeEntry* */
	GPtrArray  *boxes;      /* RsBox*, borrowed from the entries */
};

enum {
	SIG_BOX_ADDED,
	SIG_BOX_CHANGED,
	SIG_BOX_REMOVED,
	SIG_DISCONNECTED,
	N_SIGNALS
};
static guint signals[N_SIGNALS];

G_DEFINE_FINAL_TYPE(RsPw, rs_pw, G_TYPE_OBJECT)

G_DEFINE_QUARK(rs-pw-error, rs_pw_error)
#define RS_PW_ERROR (rs_pw_error_quark())

/* ---- node events -------------------------------------------------------- */

/* A node's properties arrive here on every change reac-pw stamps — the 200 ms
 * badge timer, a box establishing or dropping, and (once the readback lane
 * lands) every head-amp assertion. This is the ONLY place a value reaches the
 * model: there is no path from a widget straight into its own state. */
static void on_node_info(void *data, const struct pw_node_info *info)
{
	NodeEntry *e = data;
	if (!info || !(info->change_mask & PW_NODE_CHANGE_MASK_PROPS) || !info->props)
		return;
	if (rs_box_update_from_dict(e->box, info->props))
		g_signal_emit(e->owner, signals[SIG_BOX_CHANGED], 0, e->box);
}

static const struct pw_node_events node_events = {
	PW_VERSION_NODE_EVENTS,
	.info = on_node_info,
};

static void node_entry_free(gpointer p)
{
	NodeEntry *e = p;
	if (!e)
		return;
	spa_hook_remove(&e->listener);
	spa_hook_remove(&e->proxy_listener);
	if (e->proxy)
		pw_proxy_destroy((struct pw_proxy *)e->proxy);
	g_clear_object(&e->box);
	g_free(e);
}

static void on_proxy_removed(void *data)
{
	NodeEntry *e = data;
	/* The server dropped the object under us. Let the proxy go here; the
	 * registry's global_remove does the bookkeeping. */
	if (e->proxy) {
		pw_proxy_destroy((struct pw_proxy *)e->proxy);
		e->proxy = NULL;
	}
}

static const struct pw_proxy_events proxy_events = {
	PW_VERSION_PROXY_EVENTS,
	.removed = on_proxy_removed,
};

/* ---- registry ----------------------------------------------------------- */

/* WHICH NODES ARE SEGMENTS. Exactly the ones carrying `reac.segment`, because
 * reac-pw stamps that key on the node that IS the segment's door and on no
 * other: the master role's Audio/Sink, or — when we are the segment's slave and
 * no sink exists — the Audio/Source. Keying on the node NAME instead would miss
 * a segment the moment the role swapped, which is the case this key exists for.
 *
 * The media.class then says which of the two we got, and that is the difference
 * between a segment whose preamps we can move and one whose preamps belong to
 * whoever is mastering it. */
static void on_registry_global(void *data, uint32_t id,
                               uint32_t permissions G_GNUC_UNUSED,
                               const char *type, uint32_t version G_GNUC_UNUSED,
                               const struct spa_dict *props)
{
	RsPw *self = data;

	if (!props || !type || !spa_streq(type, PW_TYPE_INTERFACE_Node))
		return;
	const char *segment = spa_dict_lookup(props, RS_PROP_SEGMENT);
	if (!segment || !*segment)
		return;
	const char *media_class = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
	gboolean is_sink = media_class && spa_streq(media_class, "Audio/Sink");

	struct pw_node *proxy = pw_registry_bind(self->registry, id,
	                                         PW_TYPE_INTERFACE_Node,
	                                         PW_VERSION_NODE, 0);
	if (!proxy)
		return;

	NodeEntry *e = g_new0(NodeEntry, 1);
	e->owner = self;
	e->id = id;
	e->proxy = proxy;
	e->box = rs_box_new(id, segment, is_sink);
	/* The registry's own dict already carries the create-time badge, so the
	 * sidebar is correct before the first info event rather than briefly blank. */
	rs_box_update_from_dict(e->box, props);

	pw_node_add_listener(proxy, &e->listener, &node_events, e);
	pw_proxy_add_listener((struct pw_proxy *)proxy, &e->proxy_listener,
	                      &proxy_events, e);

	g_hash_table_insert(self->nodes, GUINT_TO_POINTER(id), e);
	g_ptr_array_add(self->boxes, e->box);
	g_signal_emit(self, signals[SIG_BOX_ADDED], 0, e->box);
}

static void on_registry_global_remove(void *data, uint32_t id)
{
	RsPw *self = data;
	NodeEntry *e = g_hash_table_lookup(self->nodes, GUINT_TO_POINTER(id));
	if (!e)
		return;
	RsBox *box = g_object_ref(e->box);
	g_ptr_array_remove_fast(self->boxes, e->box);
	g_hash_table_remove(self->nodes, GUINT_TO_POINTER(id));
	g_signal_emit(self, signals[SIG_BOX_REMOVED], 0, box);
	g_object_unref(box);
}

static const struct pw_registry_events registry_events = {
	PW_VERSION_REGISTRY_EVENTS,
	.global = on_registry_global,
	.global_remove = on_registry_global_remove,
};

static void on_core_error(void *data, uint32_t id, int seq, int res,
                          const char *message)
{
	RsPw *self = data;
	g_warning("PipeWire error on id %u, seq %d: %s (%s)", id, seq,
	          message ? message : "", spa_strerror(res));
	if (id == PW_ID_CORE && res == -EPIPE)
		g_signal_emit(self, signals[SIG_DISCONNECTED], 0);
}

static const struct pw_core_events core_events = {
	PW_VERSION_CORE_EVENTS,
	.error = on_core_error,
};

/* ---- the loop, driven from GLib ---------------------------------------- */

static gboolean on_pw_readable(gint fd G_GNUC_UNUSED,
                               GIOCondition cond G_GNUC_UNUSED,
                               gpointer user_data)
{
	RsPw *self = user_data;
	/* Non-blocking: dispatch whatever is ready and hand the main loop back. */
	pw_loop_iterate(self->loop, 0);
	return G_SOURCE_CONTINUE;
}

/* ---- lifetime ----------------------------------------------------------- */

static void rs_pw_dispose(GObject *object)
{
	RsPw *self = RS_PW(object);

	g_clear_handle_id(&self->fd_source, g_source_remove);
	g_clear_pointer(&self->nodes, g_hash_table_unref);
	g_clear_pointer(&self->boxes, g_ptr_array_unref);

	if (self->registry) {
		spa_hook_remove(&self->registry_listener);
		pw_proxy_destroy((struct pw_proxy *)self->registry);
		self->registry = NULL;
	}
	if (self->core) {
		spa_hook_remove(&self->core_listener);
		pw_core_disconnect(self->core);
		self->core = NULL;
	}
	g_clear_pointer(&self->context, pw_context_destroy);
	if (self->loop) {
		if (self->entered) {
			pw_loop_leave(self->loop);
			self->entered = FALSE;
		}
		pw_loop_destroy(self->loop);
		self->loop = NULL;
	}
	G_OBJECT_CLASS(rs_pw_parent_class)->dispose(object);
}

static void rs_pw_class_init(RsPwClass *klass)
{
	G_OBJECT_CLASS(klass)->dispose = rs_pw_dispose;

	signals[SIG_BOX_ADDED] = g_signal_new("box-added", RS_TYPE_PW,
	        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, RS_TYPE_BOX);
	signals[SIG_BOX_CHANGED] = g_signal_new("box-changed", RS_TYPE_PW,
	        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, RS_TYPE_BOX);
	signals[SIG_BOX_REMOVED] = g_signal_new("box-removed", RS_TYPE_PW,
	        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, RS_TYPE_BOX);
	signals[SIG_DISCONNECTED] = g_signal_new("disconnected", RS_TYPE_PW,
	        G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void rs_pw_init(RsPw *self)
{
	self->nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                    NULL, node_entry_free);
	self->boxes = g_ptr_array_new();
}

RsPw *rs_pw_new(GError **error)
{
	RsPw *self = g_object_new(RS_TYPE_PW, NULL);

	self->loop = pw_loop_new(NULL);
	if (!self->loop) {
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("Could not create a PipeWire loop."));
		g_object_unref(self);
		return NULL;
	}
	pw_loop_enter(self->loop);
	self->entered = TRUE;

	self->context = pw_context_new(self->loop, NULL, 0);
	if (!self->context) {
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("Could not create a PipeWire context."));
		g_object_unref(self);
		return NULL;
	}

	self->core = pw_context_connect(self->context, NULL, 0);
	if (!self->core) {
		/* Said as a sentence, because an operator cannot tell a failed
		 * connection from a graph with no stageboxes in it by looking. */
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("Could not connect to PipeWire. Is the session running?"));
		g_object_unref(self);
		return NULL;
	}
	pw_core_add_listener(self->core, &self->core_listener, &core_events, self);

	self->registry = pw_core_get_registry(self->core, PW_VERSION_REGISTRY, 0);
	if (!self->registry) {
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("Could not read the PipeWire registry."));
		g_object_unref(self);
		return NULL;
	}
	pw_registry_add_listener(self->registry, &self->registry_listener,
	                         &registry_events, self);

	self->fd_source = g_unix_fd_add(pw_loop_get_fd(self->loop), G_IO_IN,
	                                on_pw_readable, self);
	return self;
}

GPtrArray *rs_pw_boxes(RsPw *self)
{
	g_return_val_if_fail(RS_IS_PW(self), NULL);
	return self->boxes;
}

gboolean rs_pw_write_headamp(RsPw *self, RsBox *box, int box_input,
                             enum rs_headamp_param param, int value,
                             GError **error)
{
	g_return_val_if_fail(RS_IS_PW(self), FALSE);
	g_return_val_if_fail(RS_IS_BOX(box), FALSE);

	NodeEntry *e = g_hash_table_lookup(self->nodes,
	                                   GUINT_TO_POINTER(rs_box_id(box)));
	if (!e || !e->proxy) {
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("That stagebox has left the graph."));
		return FALSE;
	}
	if (!rs_box_is_sink_door(box)) {
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("This segment has no control door: another master owns it."));
		return FALSE;
	}

	int wire_ch = rs_box_wire_ch(box, box_input);
	if (wire_ch < 0) {
		/* No announced chassis strap. Guessing 0 here would address a different
		 * box's preamps and say nothing, so this refuses instead. */
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("No wire address for this input: the box has announced no base."));
		return FALSE;
	}

	uint8_t buf[RS_PROPS_POD_BUF];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof buf);
	const struct spa_pod *pod = rs_props_build_headamp(&b, wire_ch, param, value);
	if (!pod) {
		g_set_error_literal(error, RS_PW_ERROR, 0,
		                    _("That value is outside what the daemon accepts."));
		return FALSE;
	}

	pw_node_set_param(e->proxy, SPA_PARAM_Props, 0, pod);
	/* Deliberately no success claim beyond "the pod was handed over". The
	 * daemon's parse drops what it does not like and answers nothing; the
	 * caller learns the outcome from the next info event. */
	return TRUE;
}
