// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#include "rs-box.h"

#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>

struct _RsBox {
	GObject parent_instance;

	uint32_t id;
	char    *segment;
	gboolean is_sink_door;

	char *model;
	char *width;
	char *mac;
	char *firmware;
	char *reac_version;
	char *link_state;
	char *master_state;
	char *role_state;
	char *caps;
	char *refused;          /* NULL while the key is absent */
	int   channels;
	int   base;
	int   sens_max;
	gboolean sens_max_published;

	gboolean has_readback;
	struct rs_headamp_cell cells[RS_HEADAMP_MAX_CH * RS_HEADAMP_NPARAMS];
	int n_cells;
};

G_DEFINE_FINAL_TYPE(RsBox, rs_box, G_TYPE_OBJECT)

static void rs_box_finalize(GObject *object)
{
	RsBox *self = RS_BOX(object);
	g_clear_pointer(&self->segment, g_free);
	g_clear_pointer(&self->model, g_free);
	g_clear_pointer(&self->width, g_free);
	g_clear_pointer(&self->mac, g_free);
	g_clear_pointer(&self->firmware, g_free);
	g_clear_pointer(&self->reac_version, g_free);
	g_clear_pointer(&self->link_state, g_free);
	g_clear_pointer(&self->master_state, g_free);
	g_clear_pointer(&self->role_state, g_free);
	g_clear_pointer(&self->caps, g_free);
	g_clear_pointer(&self->refused, g_free);
	G_OBJECT_CLASS(rs_box_parent_class)->finalize(object);
}

static void rs_box_class_init(RsBoxClass *klass)
{
	G_OBJECT_CLASS(klass)->finalize = rs_box_finalize;
}

static void rs_box_init(RsBox *self)
{
	/* The seeds are the daemon's own "nothing known yet" spellings, so a box
	 * that has not been heard from reads the same here as it does on the node. */
	self->model        = g_strdup("none");
	self->width        = g_strdup("0x0");
	self->mac          = g_strdup(RS_PROP_VALUE_NONE);
	self->firmware     = g_strdup("");
	self->reac_version = g_strdup("");
	self->link_state   = g_strdup("probing");
	self->master_state = g_strdup(RS_PROP_VALUE_NONE);
	self->role_state   = g_strdup("");
	self->caps         = g_strdup("");
	self->channels     = 0;
	self->base         = -1;
	self->sens_max     = RS_HEADAMP_SENS_MAX_DEFAULT;
}

RsBox *rs_box_new(uint32_t id, const char *segment, gboolean is_sink_door)
{
	RsBox *self = g_object_new(RS_TYPE_BOX, NULL);
	self->id = id;
	self->segment = g_strdup(segment ? segment : "");
	self->is_sink_door = is_sink_door;
	return self;
}

/* Replace a string field only when the key is PRESENT. Returns TRUE on a real
 * change. An absent key holds the last value, which is what makes a merge a
 * merge — reac-pw stamps a subset on every publish. */
static gboolean take_str(char **field, const struct spa_dict *d, const char *key)
{
	const char *v = spa_dict_lookup(d, key);
	if (!v)
		return FALSE;
	if (*field && strcmp(*field, v) == 0)
		return FALSE;
	g_free(*field);
	*field = g_strdup(v);
	return TRUE;
}

static gboolean take_int(int *field, const struct spa_dict *d, const char *key)
{
	const char *v = spa_dict_lookup(d, key);
	if (!v || !*v)
		return FALSE;
	char *end = NULL;
	long n = strtol(v, &end, 10);
	if (end == v || *end != '\0')
		return FALSE;
	if (*field == (int)n)
		return FALSE;
	*field = (int)n;
	return TRUE;
}

gboolean rs_box_update_from_dict(RsBox *self, const struct spa_dict *props)
{
	g_return_val_if_fail(RS_IS_BOX(self), FALSE);
	if (!props)
		return FALSE;

	gboolean changed = FALSE;
	changed |= take_str(&self->model,        props, RS_PROP_BOX_MODEL);
	changed |= take_str(&self->width,        props, RS_PROP_BOX_WIDTH);
	changed |= take_str(&self->mac,          props, RS_PROP_BOX_MAC);
	changed |= take_str(&self->firmware,     props, RS_PROP_BOX_FIRMWARE);
	changed |= take_str(&self->reac_version, props, RS_PROP_BOX_REAC_VERSION);
	changed |= take_str(&self->link_state,   props, RS_PROP_LINK_STATE);
	changed |= take_str(&self->master_state, props, RS_PROP_MASTER_STATE);
	changed |= take_str(&self->role_state,   props, RS_PROP_ROLE_STATE);
	changed |= take_str(&self->caps,         props, RS_PROP_HEADAMP_CAPS);
	changed |= take_int(&self->channels,     props, RS_PROP_HEADAMP_CHANNELS);

	/* The base is parsed, not stored raw: "none" and a non-numeric both mean
	 * "no wire address", and the one thing that must never happen is reading
	 * either as base 0 — which addresses an S-1608's preamps 32 slots low and
	 * moves a different box's inputs without saying so. */
	const char *base_s = spa_dict_lookup(props, RS_PROP_HEADAMP_BASE);
	if (base_s) {
		int b = rs_headamp_base_parse(base_s);
		if (b != self->base) {
			self->base = b;
			changed = TRUE;
		}
	}

	/* The refusal code. Its ABSENCE is a different fact from "none": absent
	 * means this daemon does not publish refusals at all, so a refused write
	 * would look exactly like an applied one. */
	const char *refused = spa_dict_lookup(props, RS_PROP_HEADAMP_REFUSED);
	if (refused && (!self->refused || strcmp(self->refused, refused) != 0)) {
		g_free(self->refused);
		self->refused = g_strdup(refused);
		changed = TRUE;
	}

	const char *smax = spa_dict_lookup(props, RS_PROP_HEADAMP_SENS_MAX);
	if (smax) {
		int m = rs_headamp_sens_max_parse(smax);
		if (m != self->sens_max || !self->sens_max_published) {
			self->sens_max = m;
			self->sens_max_published = TRUE;
			changed = TRUE;
		}
	}

	/* THE READBACK. Presence is the capability; content is the state. An empty
	 * string is a daemon asserting nothing — a real answer — and the key being
	 * missing is a daemon that cannot answer at all. */
	const char *asserted = spa_dict_lookup(props, RS_PROP_HEADAMP_ASSERTED);
	if (asserted) {
		struct rs_headamp_cell cells[G_N_ELEMENTS(self->cells)];
		int n = rs_headamp_asserted_parse(asserted, cells,
		                                  (int)G_N_ELEMENTS(cells));
		if (n < 0)
			n = 0;
		if (!self->has_readback || n != self->n_cells
		    || memcmp(cells, self->cells, (size_t)n * sizeof cells[0]) != 0) {
			memcpy(self->cells, cells, (size_t)n * sizeof cells[0]);
			self->n_cells = n;
			self->has_readback = TRUE;
			changed = TRUE;
		}
	}

	return changed;
}

uint32_t    rs_box_id(RsBox *self)            { return self->id; }

void rs_box_set_door(RsBox *self, uint32_t id, gboolean is_sink_door)
{
	g_return_if_fail(RS_IS_BOX(self));
	self->id = id;
	self->is_sink_door = is_sink_door;
}
const char *rs_box_segment(RsBox *self)       { return self->segment; }
gboolean    rs_box_is_sink_door(RsBox *self)  { return self->is_sink_door; }
const char *rs_box_model(RsBox *self)         { return self->model; }
const char *rs_box_width(RsBox *self)         { return self->width; }
const char *rs_box_mac(RsBox *self)           { return self->mac; }
const char *rs_box_firmware(RsBox *self)      { return self->firmware; }
const char *rs_box_reac_version(RsBox *self)  { return self->reac_version; }
const char *rs_box_link_state(RsBox *self)    { return self->link_state; }
const char *rs_box_master_state(RsBox *self)  { return self->master_state; }
const char *rs_box_role_state(RsBox *self)    { return self->role_state; }
const char *rs_box_caps(RsBox *self)          { return self->caps; }
const char *rs_box_refused(RsBox *self)       { return self->refused; }
int         rs_box_channels(RsBox *self)      { return self->channels; }
int         rs_box_base(RsBox *self)          { return self->base; }
int         rs_box_sens_max(RsBox *self)      { return self->sens_max; }
gboolean    rs_box_sens_max_published(RsBox *self) { return self->sens_max_published; }
gboolean    rs_box_has_readback(RsBox *self)  { return self->has_readback; }

int rs_box_wire_ch(RsBox *self, int box_input)
{
	g_return_val_if_fail(RS_IS_BOX(self), -1);
	return rs_headamp_wire_ch(self->base, box_input);
}

int rs_box_asserted(RsBox *self, int box_input, enum rs_headamp_param p)
{
	g_return_val_if_fail(RS_IS_BOX(self), -1);
	if (!self->has_readback)
		return -1;
	int ch = rs_box_wire_ch(self, box_input);
	if (ch < 0)
		return -1;
	return rs_headamp_cell_lookup(self->cells, self->n_cells, ch, p);
}

enum rs_avail rs_box_availability(RsBox *self)
{
	g_return_val_if_fail(RS_IS_BOX(self), RS_AVAIL_NO_CONTROL_DOOR);
	char base[16];
	if (self->base >= 0)
		g_snprintf(base, sizeof base, "%d", self->base);
	else
		g_strlcpy(base, RS_PROP_VALUE_NONE, sizeof base);
	return rs_headamp_availability(self->is_sink_door, self->link_state,
	                               self->channels, base, self->refused,
	                               self->has_readback);
}

char *rs_box_subtitle(RsBox *self)
{
	g_return_val_if_fail(RS_IS_BOX(self), NULL);
	GString *s = g_string_new(NULL);
	g_string_append(s, self->segment);
	if (self->width && strcmp(self->width, "0x0") != 0)
		g_string_append_printf(s, " · %s", self->width);
	/* Firmware and REAC version are DIFFERENT numbers and one is never
	 * substituted for the other; both are "" until the box answers the identity
	 * poll, and "" is rendered as absent rather than as a blank field. */
	if (self->firmware && *self->firmware)
		g_string_append_printf(s, " · %s %s", _("fw"), self->firmware);
	if (self->reac_version && *self->reac_version)
		g_string_append_printf(s, " · %s %s", _("REAC"), self->reac_version);
	return g_string_free(s, FALSE);
}
