// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-pw — the only door.
 *
 * This application speaks PipeWire and nothing else. It walks the registry for
 * the nodes that carry `reac.segment`, binds a `pw_node` proxy to each, reads
 * the box badge and the head-amp capability/readback out of the node's
 * properties as they arrive on the info event, and writes a change with
 * `pw_node_set_param(SPA_PARAM_Props, …)`. There is no REAC code here, no
 * socket, no libreac, and no second copy of anything reac-pw publishes — the
 * daemon is the one writer on the wire and the one holder of the shadow table.
 *
 * ONE THREAD. PipeWire's loop is driven from the GTK main loop through its own
 * fd rather than a `pw_thread_loop`, so every proxy call and every model update
 * happens on the main thread and there is no lock between them. The cost is
 * that a blocking call inside a widget handler would stall the graph client;
 * there are none.
 *
 * A WRITE IS NOT A CHANGE. `pw_node_set_param` returns success for a cell
 * reac-pw's parse drops on the floor, so nothing here treats the call as the
 * outcome. The outcome is the next info event: the daemon's re-published
 * `reac.headamp.asserted` and `reac.headamp.refused`. A caller writes, then
 * waits to be told. */
#ifndef RS_PW_H
#define RS_PW_H

#include <glib-object.h>

#include "rs-box.h"

G_BEGIN_DECLS

#define RS_TYPE_PW (rs_pw_get_type())
G_DECLARE_FINAL_TYPE(RsPw, rs_pw, RS, PW, GObject)

/* Connect to the session's PipeWire and start watching the registry. Returns
 * NULL and sets `error` when there is no session to connect to — which is a
 * sentence the window shows, never a silent empty list, because an empty list
 * and a failed connection look identical to an operator. */
RsPw *rs_pw_new(GError **error);

/* The segments currently in the graph, newest last. Elements are RsBox*, owned
 * by the RsPw. */
GPtrArray *rs_pw_boxes(RsPw *self);

/* Write one head-amp cell to `box`'s node. Returns FALSE, with `error` set,
 * when the write could not even be ATTEMPTED — no control door, no wire
 * address, a value the daemon's parse would drop. TRUE means the pod went to
 * the node, which is NOT the same as the box having moved: watch for
 * ::box-changed and compare the asserted value. */
gboolean rs_pw_write_headamp(RsPw *self, RsBox *box, int box_input,
                             enum rs_headamp_param param, int value,
                             GError **error);

G_END_DECLS

#endif /* RS_PW_H */
