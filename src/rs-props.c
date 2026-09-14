// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-props.h"

#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>

const struct spa_pod *rs_props_build_headamp(struct spa_pod_builder *b,
                                             int wire_ch,
                                             enum rs_headamp_param param,
                                             int value)
{
	if (!b)
		return NULL;

	char key[RS_HEADAMP_KEY_CAP];
	if (rs_headamp_param_key(key, sizeof key, wire_ch, param) != 0)
		return NULL;

	/* The per-param range gate reac-pw applies (value_in_range): out of range
	 * is dropped there silently, so it is refused HERE, where the caller can be
	 * told. `sens` is gated against the caller's ceiling upstream — this is the
	 * absolute byte bound the parse enforces. */
	if (value < 0 || value > 255)
		return NULL;
	if ((param == RS_HEADAMP_PHANTOM || param == RS_HEADAMP_PAD) && value > 1)
		return NULL;

	struct spa_pod_frame f[2];
	spa_pod_builder_push_object(b, &f[0], SPA_TYPE_OBJECT_Props, SPA_PARAM_Props);
	spa_pod_builder_prop(b, SPA_PROP_params, 0);
	spa_pod_builder_push_struct(b, &f[1]);
	spa_pod_builder_string(b, key);
	spa_pod_builder_int(b, value);
	spa_pod_builder_pop(b, &f[1]);
	struct spa_pod *pod = spa_pod_builder_pop(b, &f[0]);

	/* A builder that ran out of room keeps counting offsets past the end of the
	 * buffer and still hands back a pod-shaped pointer into it. The offset is
	 * the honest answer, and RS_PROPS_POD_BUF exists so this never fires — it
	 * is here because the one thing worse than a refused write is a write of
	 * whatever was next in the stack. */
	if (!pod || b->state.offset > b->size)
		return NULL;
	return pod;
}
