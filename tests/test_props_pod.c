// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* The pod this application writes, taken apart the way reac-pw takes it apart.
 *
 * `pw_node_set_param` returns success for a pod reac-pw's parse drops on the
 * floor: the wrong object type, the wrong property id, an unnested struct, a
 * desynced (key, value) pairing — every one of them is a write that changes a
 * number on our side and moves no audio, with nothing said. So the pod is built
 * here by the same function the click handler uses, and then walked with
 * `spa_pod_object_find_prop(SPA_PROP_params)` + `SPA_POD_STRUCT_FOREACH`, which
 * is exactly what `reac_headamp_prop_parse` does — and the key it finds is
 * compared against a longhand literal, not against another call to the
 * formatter.
 *
 * SPA headers only: no PipeWire daemon, no graph, no session. */

#include <stdio.h>
#include <string.h>

#include <spa/param/param.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/pod.h>

#include "rs-props.h"

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

/* The reader half: pull the single (key, value) pair out of a Props pod the
 * way the daemon does. Returns 0 on success. */
static int read_cell(const struct spa_pod *pod, const char **key, int32_t *value)
{
	*key = NULL;
	*value = -1;

	if (!pod || !spa_pod_is_object_type(pod, SPA_TYPE_OBJECT_Props))
		return -1;

	const struct spa_pod_object *obj = (const struct spa_pod_object *)pod;
	const struct spa_pod_prop *prop =
	        spa_pod_object_find_prop(obj, NULL, SPA_PROP_params);
	if (!prop || !spa_pod_is_struct(&prop->value))
		return -1;

	const char *pending = NULL;
	struct spa_pod *child;
	SPA_POD_STRUCT_FOREACH(&prop->value, child) {
		if (!pending) {
			const char *s;
			if (spa_pod_get_string(child, &s) == 0)
				pending = s;
			continue;
		}
		int32_t v;
		if (spa_pod_get_int(child, &v) != 0)
			return -1;
		*key = pending;
		*value = v;
		return 0;
	}
	return -1;
}

static void check_cell(int wire_ch, enum rs_headamp_param param, int value,
                       const char *want_key, int want_value)
{
	uint8_t buf[RS_PROPS_POD_BUF];
	struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof buf);
	const struct spa_pod *pod = rs_props_build_headamp(&b, wire_ch, param, value);
	if (!pod) {
		fprintf(stderr, "FAIL: no pod for ch %d param %d value %d\n",
		        wire_ch, (int)param, value);
		failures++;
		return;
	}

	/* The object type and property id are checked inside read_cell, which is
	 * the point: a pod built as anything but Props/SPA_PROP_params fails here
	 * rather than on the desk. */
	const char *key = NULL;
	int32_t got = -1;
	if (read_cell(pod, &key, &got) != 0) {
		fprintf(stderr, "FAIL: pod for %s did not read back as a Props params cell\n",
		        want_key);
		failures++;
		return;
	}
	if (!key || strcmp(key, want_key) != 0) {
		fprintf(stderr, "FAIL: key \"%s\", want \"%s\"\n",
		        key ? key : "(null)", want_key);
		failures++;
	}
	if (got != want_value) {
		fprintf(stderr, "FAIL: value %d, want %d (key %s)\n",
		        got, want_value, want_key);
		failures++;
	}
}

int main(void)
{
	/* An S-1608 at base 32: input 3 is wire channel 34. */
	check_cell(34, RS_HEADAMP_PHANTOM, 1, "reac.headamp.34.phantom", 1);
	check_cell(34, RS_HEADAMP_PHANTOM, 0, "reac.headamp.34.phantom", 0);
	check_cell(34, RS_HEADAMP_PAD,     1, "reac.headamp.34.pad",     1);
	check_cell(34, RS_HEADAMP_SENS,   52, "reac.headamp.34.sens",   52);
	/* An S-0808 at base 0, and the top of the wire space. */
	check_cell(0,  RS_HEADAMP_SENS,    0, "reac.headamp.0.sens",     0);
	check_cell(47, RS_HEADAMP_SENS, 0x37, "reac.headamp.47.sens",   55);

	uint8_t buf[RS_PROPS_POD_BUF];

	/* REFUSED BEFORE IT LEAVES. Each of these is a cell reac-pw's parse would
	 * drop in silence, so it is refused here where the operator can be told. */
	struct { int ch; enum rs_headamp_param p; int v; const char *why; } bad[] = {
		{ 48, RS_HEADAMP_PHANTOM, 1, "channel past the wire space" },
		{ -1, RS_HEADAMP_PHANTOM, 1, "no wire address" },
		{ 34, RS_HEADAMP_PHANTOM, 2, "phantom above 1" },
		{ 34, RS_HEADAMP_PAD,     7, "pad above 1" },
		{ 34, RS_HEADAMP_SENS,  256, "value outside a byte" },
		{ 34, RS_HEADAMP_SENS,   -1, "negative value" },
	};
	for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof buf);
		const struct spa_pod *pod =
		        rs_props_build_headamp(&b, bad[i].ch, bad[i].p, bad[i].v);
		if (pod) {
			fprintf(stderr, "FAIL: built a pod for %s\n", bad[i].why);
			failures++;
		}
	}

	/* A builder with no room refuses rather than handing back whatever was
	 * next in the buffer. */
	uint8_t tiny[8];
	struct spa_pod_builder small = SPA_POD_BUILDER_INIT(tiny, sizeof tiny);
	CHECK(rs_props_build_headamp(&small, 34, RS_HEADAMP_SENS, 52) == NULL);

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("props pod: ok\n");
	return 0;
}
