// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-headamp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *rs_headamp_param_name(enum rs_headamp_param p)
{
	switch (p) {
	case RS_HEADAMP_PHANTOM: return "phantom";
	case RS_HEADAMP_PAD:     return "pad";
	case RS_HEADAMP_SENS:    return "sens";
	}
	return NULL;
}

int rs_headamp_param_from_name(const char *name)
{
	if (!name)
		return -1;
	if (strcmp(name, "phantom") == 0)
		return RS_HEADAMP_PHANTOM;
	if (strcmp(name, "pad") == 0)
		return RS_HEADAMP_PAD;
	if (strcmp(name, "sens") == 0)
		return RS_HEADAMP_SENS;
	return -1;
}

int rs_headamp_wire_ch(int base, int box_input)
{
	if (base < 0 || box_input < 1)
		return -1;
	int ch = base + (box_input - 1);
	if (ch < 0 || ch >= RS_HEADAMP_MAX_CH)
		return -1;
	return ch;
}

int rs_headamp_param_key(char *out, size_t cap, int wire_ch, enum rs_headamp_param p)
{
	if (!out || cap == 0)
		return -1;
	out[0] = '\0';
	if (wire_ch < 0 || wire_ch >= RS_HEADAMP_MAX_CH)
		return -1;
	const char *name = rs_headamp_param_name(p);
	if (!name)
		return -1;
	int n = snprintf(out, cap, "%s%d.%s", RS_HEADAMP_PROP_PREFIX, wire_ch, name);
	if (n < 0 || (size_t)n >= cap) {
		out[0] = '\0';          /* never hand back a truncated key: reac-pw would
		                         * drop it and report nothing */
		return -1;
	}
	return 0;
}

/* Clamp the way libreac's reac_headamp_sens_cdb clamps — high values saturate at
 * the ceiling rather than wrapping or refusing, so a slider that overshoots by a
 * step lands on the last step instead of somewhere else entirely. */
static int sens_clamp(int value, int sens_max)
{
	if (sens_max < 0)
		sens_max = RS_HEADAMP_SENS_MAX_DEFAULT;
	if (value < 0)
		return 0;
	if (value > sens_max)
		return sens_max;
	return value;
}

int rs_headamp_sens_dbu(int value, int pad_on, int sens_max)
{
	value = sens_clamp(value, sens_max);
	return RS_HEADAMP_SENS_REF_DBU - value * RS_HEADAMP_SENS_STEP_DB
	       + (pad_on ? RS_HEADAMP_PAD_DB : 0);
}

int rs_headamp_sens_value(int dbu, int pad_on, int sens_max)
{
	if (sens_max < 0)
		sens_max = RS_HEADAMP_SENS_MAX_DEFAULT;
	/* `want` is the attenuation asked for below step 0. A sensitivity hotter
	 * than the box's step 0 gives a negative want and must land on 0 — taken
	 * first rather than left to C's truncation, which rounds -1.5 toward zero. */
	int want = RS_HEADAMP_SENS_REF_DBU + (pad_on ? RS_HEADAMP_PAD_DB : 0) - dbu;
	if (want <= 0)
		return 0;
	int step = want / RS_HEADAMP_SENS_STEP_DB;
	if (step > sens_max)
		step = sens_max;
	return step;
}

int rs_headamp_sens_max_parse(const char *s)
{
	if (!s || !*s)
		return RS_HEADAMP_SENS_MAX_DEFAULT;
	char *end = NULL;
	long v = strtol(s, &end, 10);
	if (end == s || *end != '\0')
		return RS_HEADAMP_SENS_MAX_DEFAULT;
	if (v < 1 || v > 255)
		return RS_HEADAMP_SENS_MAX_DEFAULT;
	return (int)v;
}

int rs_headamp_base_parse(const char *s)
{
	if (!s || !*s)
		return -1;
	if (strcmp(s, RS_PROP_VALUE_NONE) == 0)
		return -1;
	char *end = NULL;
	long v = strtol(s, &end, 10);
	if (end == s || *end != '\0')
		return -1;
	if (v < 0 || v >= RS_HEADAMP_MAX_CH)
		return -1;
	return (int)v;
}

int rs_headamp_asserted_parse(const char *s, struct rs_headamp_cell *out, int max)
{
	if (!s)
		return -1;
	int n = 0;
	const char *p = s;
	while (*p && n < max) {
		/* One cell: <ch> ':' <param> '=' <value>, then ',' or end. A cell that
		 * does not parse is skipped whole — resync on the next comma — so one
		 * malformed entry never truncates the readback. */
		char *end = NULL;
		long ch = strtol(p, &end, 10);
		int ok = (end != p && *end == ':');
		long param = 0, value = 0;
		if (ok) {
			const char *q = end + 1;
			param = strtol(q, &end, 10);
			ok = (end != q && *end == '=');
		}
		if (ok) {
			const char *q = end + 1;
			value = strtol(q, &end, 10);
			ok = (end != q && (*end == ',' || *end == '\0'));
		}
		if (ok && ch >= 0 && ch < RS_HEADAMP_MAX_CH
		    && param >= 0 && param < RS_HEADAMP_NPARAMS
		    && value >= 0 && value <= 255) {
			out[n].ch = (int)ch;
			out[n].param = (int)param;
			out[n].value = (int)value;
			n++;
		}
		/* Advance past this cell whether it parsed or not. */
		const char *comma = strchr(p, ',');
		if (!comma)
			break;
		p = comma + 1;
	}
	return n;
}

int rs_headamp_cell_lookup(const struct rs_headamp_cell *cells, int n,
                           int ch, enum rs_headamp_param p)
{
	if (!cells)
		return -1;
	for (int i = 0; i < n; i++)
		if (cells[i].ch == ch && cells[i].param == (int)p)
			return cells[i].value;
	return -1;
}

int rs_headamp_caps_has(const char *caps, const char *token)
{
	if (!caps || !token || !*token)
		return 0;
	size_t tlen = strlen(token);
	const char *p = caps;
	while (*p) {
		const char *comma = strchr(p, ',');
		size_t len = comma ? (size_t)(comma - p) : strlen(p);
		if (len == tlen && strncmp(p, token, tlen) == 0)
			return 1;
		if (!comma)
			break;
		p = comma + 1;
	}
	return 0;
}

enum rs_avail rs_headamp_availability(int have_sink_door, const char *link_state,
                                      int channels, const char *base,
                                      const char *refused, int have_asserted)
{
	if (!have_sink_door)
		return RS_AVAIL_NO_CONTROL_DOOR;
	if (refused && strcmp(refused, RS_REFUSED_BOX_MASTER) == 0)
		return RS_AVAIL_BOX_MASTER;
	if (channels <= 0)
		return RS_AVAIL_NO_BOX;
	if (rs_headamp_base_parse(base) < 0)
		return RS_AVAIL_NO_BASE;
	if (!link_state || strcmp(link_state, RS_LINK_ESTABLISHED) != 0)
		return RS_AVAIL_NOT_ESTABLISHED;
	if (!have_asserted)
		return RS_AVAIL_NO_READBACK;
	return RS_AVAIL_OK;
}

const char *rs_avail_id(enum rs_avail a)
{
	switch (a) {
	case RS_AVAIL_OK:               return "ok";
	case RS_AVAIL_NO_CONTROL_DOOR:  return "no-control-door";
	case RS_AVAIL_BOX_MASTER:       return RS_REFUSED_BOX_MASTER;
	case RS_AVAIL_NO_BOX:           return "no-box";
	case RS_AVAIL_NO_BASE:          return "no-base";
	case RS_AVAIL_NOT_ESTABLISHED:  return "not-established";
	case RS_AVAIL_NO_READBACK:      return "no-readback";
	}
	return "unknown";
}
