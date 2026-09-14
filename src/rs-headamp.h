// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-headamp — the head-amp vocabulary, as a PURE module.
 *
 * Everything this application knows about REAC lives here, and it is a very
 * short list, because the application is a PipeWire client and not a REAC one:
 * the control-key grammar reac-pw parses, the wire-channel address, the
 * sensitivity curve, the readback cell list, and the rule that decides whether
 * a segment's preamps are reachable at all. No PipeWire, no GTK, no libreac —
 * so `meson test` drives every line of it offline.
 *
 * THE NAMES ARE NOT GUESSED. Each one below is the spelling reac-pw publishes
 * or parses, quoted from its source, and the citation is on the definition. A
 * misspelt key is the one defect this module can hide: reac-pw's parse SKIPS a
 * cell it does not recognise and returns success, so a typo here would be a
 * silent no-op on a live desk. That is why the unit test writes the expected
 * strings out longhand instead of calling the formatter to check itself.
 *
 * WHAT THIS MODULE MUST NEVER GROW. It does not build a REAC frame, address a
 * box by anything but the base the daemon published, or remember a value it was
 * not told. reac-pw is the one writer on the wire and the one holder of the
 * shadow table (design of record §3b, `reac.headamp.asserted`). This file
 * converts between what the operator sees and what that door takes. */
#ifndef RS_HEADAMP_H
#define RS_HEADAMP_H

#include <stddef.h>

/* The parameter ids, as the wire numbers them — reac-pw's
 * `enum reac_headamp_param` (libreac <reac/reac_ctrlblk.h>), which is also the
 * number that appears in a `reac.headamp.asserted` cell (`ch:param=value`). */
enum rs_headamp_param {
	RS_HEADAMP_PHANTOM = 0x00,   /* +48 V, 0 | 1 */
	RS_HEADAMP_PAD     = 0x01,   /* the box's own -20 dB pad, 0 | 1 */
	RS_HEADAMP_SENS    = 0x02,   /* sensitivity step, 0 .. sens-max */
};
#define RS_HEADAMP_NPARAMS 3

/* The SPA_PROP_params key prefix reac-pw's `reac_headamp_prop_parse` matches
 * (src/reac_headamp_prop.h, REAC_HEADAMP_PROP_PREFIX). A full control key is
 * RS_HEADAMP_PROP_PREFIX "<wireCh>." "<phantom|pad|sens>". */
#define RS_HEADAMP_PROP_PREFIX "reac.headamp."

/* The head-amp WIRE-CHANNEL space, 0x00..0x2f (libreac REAC_HEADAMP_MAX_CH).
 * NOT the 40-slot audio space: an S-1608 strapped at base 0x20 occupies 32..47,
 * and a table bounded by 40 silently drops its inputs 9..16. */
#define RS_HEADAMP_MAX_CH 48

/* The sensitivity curve, in whole dBu, from libreac <reac/reac_ctrlblk.h>:
 *
 *     sensitivity_dBu = -10 - value + (pad ? 20 : 0)
 *
 * -10 dBu at step 0 down to -65 dBu at step 0x37, a flat 1 dB per step
 * (measured over all 56 steps on an S-0808: span 54.60 dB, slope 0.988
 * dB/step, pad 20.12/20.20 dB). SENSITIVITY RUNS THE OTHER WAY FROM GAIN — the
 * hottest setting is the most negative number — and it is not silently
 * convertible into the gain figure openmixer shows, which uses a zero 10 dB
 * away. This application shows dBu and says dBu.
 *
 * The ceiling is a DEFAULT, not a law: the daemon publishes its own in
 * RS_PROP_HEADAMP_SENS_MAX when it has one, and a box whose travel differs
 * needs no new client. Use this only when that property is absent. */
#define RS_HEADAMP_SENS_MAX_DEFAULT 0x37   /* 55 — the 56th and last step */
#define RS_HEADAMP_SENS_REF_DBU     (-10)  /* step 0, pad off */
#define RS_HEADAMP_SENS_STEP_DB     1
#define RS_HEADAMP_PAD_DB           20

/* The node properties this application reads. Every one is published by
 * reac-pw on the node that carries `reac.segment` for that segment; the
 * citation is the header that defines the spelling. */
#define RS_PROP_SEGMENT            "reac.segment"             /* reac_segment_ident.h */
#define RS_PROP_LINK_STATE         "reac.link-state"          /* reac_link_state.h */
#define RS_PROP_BOX_MODEL          "reac.box-model"
#define RS_PROP_BOX_WIDTH          "reac.box-width"
#define RS_PROP_BOX_MAC            "reac.box.mac"
#define RS_PROP_BOX_FIRMWARE       "reac.box-firmware"
#define RS_PROP_BOX_REAC_VERSION   "reac.box.reac_version"
#define RS_PROP_MASTER_STATE       "reac.master.state"        /* us | foreign | none */
#define RS_PROP_ROLE_STATE         "reac.cfg.role.state"      /* reac_role_cfg.h */
#define RS_PROP_HEADAMP_CHANNELS   "reac.headamp.channels"    /* reac_headamp_prop.h */
#define RS_PROP_HEADAMP_CAPS       "reac.headamp.caps"
#define RS_PROP_HEADAMP_BASE       "reac.headamp.base"        /* reac_link_state.h */
/* The three keys the readback lane is adding (design of record §3a). ABSENT on
 * a daemon that predates it, and absence is a fact this application renders
 * rather than papers over — see RS_AVAIL_NO_READBACK. */
#define RS_PROP_HEADAMP_ASSERTED   "reac.headamp.asserted"    /* "34:0=1,34:2=52" */
#define RS_PROP_HEADAMP_STATE      "reac.headamp.state"       /* applied | unavailable */
#define RS_PROP_HEADAMP_REFUSED    "reac.headamp.refused"     /* none | no-box | ... */
#define RS_PROP_HEADAMP_SENS_MAX   "reac.headamp.sens.max"    /* decimal top step */

/* The value reac-pw writes into a key that has no box behind it — both
 * `reac.box.mac` and `reac.headamp.base` use it (REAC_BOX_SOURCE_NONE /
 * REAC_BOX_MAC_NONE). A numeric parse of the base must FAIL on this rather
 * than reading as base 0, which would address an S-1608 thirty-two slots low
 * and silently. */
#define RS_PROP_VALUE_NONE "none"

/* The refusal code that is not an error: the box is strapped to REAC master
 * mode, its preamps are configured out of band through its serial port, and
 * head-amp control over it DOES NOT EXIST on the wire in either direction
 * (mixer-protocol.md §9; measured — a byte-identical SET moved a box on M not
 * at all, against +18.9 dB on the same box enrolled). */
#define RS_REFUSED_BOX_MASTER "box-master"
#define RS_REFUSED_NONE       "none"

/* The link state that means a box is actually there (libreac
 * reac_link_state_name: probing | granting | established | dropped). */
#define RS_LINK_ESTABLISHED "established"


/* ---- the control-key grammar ------------------------------------------- */

/* The trailing name of a control key for `p`, or NULL for an id out of range.
 * These are the three spellings reac-pw's `param_name_to_id` accepts. */
const char *rs_headamp_param_name(enum rs_headamp_param p);

/* The inverse: an id, or -1 for a name reac-pw would not recognise. */
int rs_headamp_param_from_name(const char *name);

/* The WIRE channel for a box input, `base + (box_input - 1)` — the law the
 * head-amp record's CH field carries (libreac reac_ctrl.h; base is the box's
 * own chassis strap, announced on the wire and published by the daemon, never
 * derived from the width). `box_input` is 1-based, the way the box's panel is
 * numbered. Returns -1 for a base of -1 (no box), an input below 1, or a
 * result outside the 0..RS_HEADAMP_MAX_CH-1 space. */
int rs_headamp_wire_ch(int base, int box_input);

/* Format the control key for (wire_ch, p) into `out`. Returns 0 on success, -1
 * on a bad channel/param or a buffer too small. A key this returns is one
 * reac-pw's parse accepts; a key it refuses is one reac-pw would silently drop. */
int rs_headamp_param_key(char *out, size_t cap, int wire_ch, enum rs_headamp_param p);

/* The longest key this module can produce, plus the terminator. */
#define RS_HEADAMP_KEY_CAP 48


/* ---- the sensitivity curve --------------------------------------------- */

/* Sensitivity in whole dBu for a step, `pad_on` adding the pad's 20 dB.
 * `value` is clamped to 0..sens_max the way libreac clamps it. */
int rs_headamp_sens_dbu(int value, int pad_on, int sens_max);

/* The inverse: the nearest step for a sensitivity in dBu, clamped into
 * 0..sens_max. Exact and round-tripping, because the step IS a whole decibel. */
int rs_headamp_sens_value(int dbu, int pad_on, int sens_max);

/* Parse the daemon's published ceiling. Returns the step, or
 * RS_HEADAMP_SENS_MAX_DEFAULT when `s` is NULL, empty or not a sane decimal —
 * the only default in this file, and it is a RANGE, never a measurement. */
int rs_headamp_sens_max_parse(const char *s);


/* ---- the readback cell list -------------------------------------------- */

struct rs_headamp_cell {
	int ch;      /* wire channel */
	int param;   /* enum rs_headamp_param */
	int value;   /* absolute setting */
};

/* Parse `reac.headamp.asserted` — "ch:param=value" cells, comma separated,
 * empty when nothing is set — into `out` (capacity `max`). Malformed cells are
 * skipped so one bad cell never hides the rest. Returns the number written, or
 * -1 if `s` is NULL. An EMPTY string is a valid answer meaning "this daemon is
 * asserting nothing", and returns 0 — which is not the same fact as the
 * property being absent; see rs_headamp_availability. */
int rs_headamp_asserted_parse(const char *s, struct rs_headamp_cell *out, int max);

/* Look one cell up in a parsed list. Returns the value, or -1 when the daemon
 * is asserting nothing for it — an honest "unset", never a zero. */
int rs_headamp_cell_lookup(const struct rs_headamp_cell *cells, int n,
                           int ch, enum rs_headamp_param p);

/* Is `token` one of the comma-separated tokens in a `reac.headamp.caps` value?
 * A capability the box does not carry gets no control. */
int rs_headamp_caps_has(const char *caps, const char *token);


/* ---- is this segment's head-amp reachable at all? ----------------------- */

enum rs_avail {
	RS_AVAIL_OK = 0,
	RS_AVAIL_NO_CONTROL_DOOR,  /* we are this segment's SLAVE: no sink node exists,
	                            * so there is nowhere to write. reac-pw builds the
	                            * master-role sink only, and only a master can put a
	                            * head-amp record on the wire. */
	RS_AVAIL_BOX_MASTER,       /* the daemon refused with box-master */
	RS_AVAIL_NO_BOX,           /* channels = 0: no model recognised yet */
	RS_AVAIL_NO_BASE,          /* no announced chassis strap: no wire address */
	RS_AVAIL_NOT_ESTABLISHED,  /* probing / granting / dropped */
	RS_AVAIL_NO_READBACK,      /* the door is there, but this daemon publishes no
	                            * reac.headamp.asserted — so a control could be
	                            * moved and never confirmed. Read-only. */
};

/* Decide, from exactly the properties the node published. `have_sink_door` is
 * whether the segment's door is an Audio/Sink (we are master) rather than an
 * Audio/Source (we are slave). `base` and `refused` are the raw property
 * strings, NULL when the key is absent; `have_asserted` is whether
 * `reac.headamp.asserted` was PRESENT, regardless of its content.
 *
 * The order is most-specific-first, so the reason a surface renders is the one
 * the operator can act on. */
enum rs_avail rs_headamp_availability(int have_sink_door, const char *link_state,
                                      int channels, const char *base,
                                      const char *refused, int have_asserted);

/* A stable, untranslated id for a reason — what a log line or a test names.
 * The user-facing sentence is the window's, through gettext. */
const char *rs_avail_id(enum rs_avail a);

/* The announced chassis strap as a number, or -1 when there is none. "none",
 * NULL, empty and anything non-numeric all give -1: there is no wire address
 * and guessing 0 is the documented way to move the wrong box's preamps. */
int rs_headamp_base_parse(const char *s);

#endif /* RS_HEADAMP_H */
