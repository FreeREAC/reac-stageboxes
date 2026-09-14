// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-props — build the ONE pod this application ever writes.
 *
 * reac-pw takes a head-amp change as SPA_PARAM_Props carrying SPA_PROP_params:
 * SPA's extensible "simple control params" bag, a Struct of alternating
 * (String key, Pod value) pairs. There is no standard SPA_PROP_* id for
 * per-channel phantom/pad/sens, which is why the extensible bag is the door.
 *
 * It is its own module, and not three lines inside the click handler, because
 * this pod is the whole write: get the object type, the property id, the struct
 * nesting or the pairing wrong and `pw_node_set_param` still returns success —
 * reac-pw's parse skips what it does not understand and reports nothing. So the
 * pod is built where a test can build it too, and the test takes it apart with
 * the same SPA iteration reac-pw's parser uses and checks the key STRING it
 * finds against a longhand literal.
 *
 * SPA headers only: no libpipewire, no GTK. */
#ifndef RS_PROPS_H
#define RS_PROPS_H

#include <stddef.h>
#include <stdint.h>

#include "rs-headamp.h"

struct spa_pod;
struct spa_pod_builder;

/* Build `Props { params = [ "reac.headamp.<ch>.<param>", <value> ] }` into the
 * caller's builder. Returns the pod, or NULL if the channel/param is one
 * reac-pw would refuse (so a refusable write is never sent at all) or the
 * builder overflowed.
 *
 * The value goes on as an Int. reac-pw accepts Bool, Int and Float — a toggle,
 * a spin button and a slider each sending its natural encoding — and one
 * encoding for all three keeps a single path under test. */
const struct spa_pod *rs_props_build_headamp(struct spa_pod_builder *b,
                                             int wire_ch,
                                             enum rs_headamp_param param,
                                             int value);

/* A buffer big enough for any pod the function above builds: one object, one
 * property, one struct, one key string and one int. */
#define RS_PROPS_POD_BUF 256

#endif /* RS_PROPS_H */
