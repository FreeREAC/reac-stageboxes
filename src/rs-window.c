// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-window — the one window.
 *
 * Sidebar: every segment in the graph. Content: one row per input of the
 * selected box, with the controls its published caps say it has.
 *
 * THE RULE THIS FILE KEEPS. No widget holds a value. Every control renders
 * `reac.headamp.asserted` — the daemon's own shadow table, read back — and a
 * gesture writes a param and then waits to be told. If the daemon refuses, its
 * code arrives on the next info event, the reason goes on the row, and the
 * control snaps back to what is actually asserted. A switch that moved because
 * it was clicked, and stayed moved because nothing contradicted it, is the
 * failure this application exists to avoid: the write is silent when it is
 * dropped.
 *
 * And where the capability is not there at all — no control door, no box, no
 * base, a box strapped to master, or a daemon with no readback — the page says
 * which, and the controls are insensitive with their last known values held.
 * A dimmed control that does nothing, with no reason beside it, is worse than
 * no control. */

#include "rs-window.h"

#include <glib/gi18n.h>

#include "rs-headamp.h"

typedef struct {
	int              input;        /* 1-based box input */
	AdwExpanderRow  *row;
	AdwSwitchRow    *phantom;
	AdwSwitchRow    *pad;
	AdwActionRow    *sens_row;
	GtkScale        *sens;
	GtkLabel        *note;         /* the inline refusal / readback sentence */
} InputRow;

struct _RsWindow {
	AdwApplicationWindow parent_instance;

	RsPw *pw;
	char *pw_error;

	AdwNavigationSplitView *split;
	GtkListBox             *sidebar;
	AdwPreferencesPage     *page;
	AdwPreferencesGroup    *inputs_group;
	AdwBanner              *banner;
	AdwStatusPage          *empty;
	GtkStack               *content_stack;
	AdwWindowTitle         *content_title;

	RsBox    *selected;            /* borrowed */
	GPtrArray *rows;               /* InputRow*, rebuilt when the shape changes */
	int        rows_for_channels;  /* the channel count `rows` was built for */

	gboolean   syncing;            /* set while widgets are written from the model,
	                                * so a programmatic change never writes back */
	/* The one write we are waiting on, so a refusal lands on the row that
	 * caused it rather than on all of them. -1 = nothing pending. */
	int        pending_input;
	int        pending_param;
};

G_DEFINE_FINAL_TYPE(RsWindow, rs_window, ADW_TYPE_APPLICATION_WINDOW)

static void sync_content(RsWindow *self);

/* ---- the reason sentences ---------------------------------------------- */

/* One sentence per unavailability, in the operator's terms, each naming what is
 * true rather than what failed. `box-master` is deliberately not phrased as an
 * error: it is the contract of that mode. */
static const char *avail_sentence(enum rs_avail a)
{
	switch (a) {
	case RS_AVAIL_OK:
		return NULL;
	case RS_AVAIL_NO_CONTROL_DOOR:
		return _("Another master owns this segment, so there is no preamp control here.");
	case RS_AVAIL_BOX_MASTER:
		return _("This box is strapped to REAC master mode. Its preamps are set on the box itself, over its serial port — REAC carries no preamp control to a box in master mode.");
	case RS_AVAIL_NO_BOX:
		return _("No stagebox recognised on this segment yet.");
	case RS_AVAIL_NO_BASE:
		return _("The box has announced no head-amp base, so its inputs have no wire address.");
	case RS_AVAIL_NOT_ESTABLISHED:
		return _("The link to this box is not established.");
	case RS_AVAIL_NO_READBACK:
		return _("No readback from this daemon: it publishes no head-amp state, so a change could not be confirmed. Shown read-only.");
	}
	return NULL;
}

/* The daemon's refusal codes, said plainly. An unknown code is shown verbatim
 * rather than swallowed — a code this application has not been taught is still
 * the daemon's answer and the operator should see it. */
static char *refusal_sentence(const char *code)
{
	if (!code || !*code || g_str_equal(code, RS_REFUSED_NONE))
		return NULL;
	if (g_str_equal(code, RS_REFUSED_BOX_MASTER))
		return g_strdup(_("Refused: the box is in master mode."));
	if (g_str_equal(code, "no-box"))
		return g_strdup(_("Refused: no box on this segment."));
	if (g_str_equal(code, "no-base"))
		return g_strdup(_("Refused: no wire address for this box."));
	if (g_str_equal(code, "bad-key"))
		return g_strdup(_("Refused: the daemon did not recognise that control."));
	if (g_str_equal(code, "out-of-range"))
		return g_strdup(_("Refused: that value is out of range."));
	return g_strdup_printf(_("Refused by the daemon: %s"), code);
}

/* ---- the write path ----------------------------------------------------- */

static void note_set(InputRow *r, const char *text)
{
	gtk_label_set_text(r->note, text ? text : "");
	gtk_widget_set_visible(GTK_WIDGET(r->note), text && *text);
}

static void write_cell(RsWindow *self, InputRow *r, enum rs_headamp_param p, int value)
{
	if (self->syncing || !self->selected || !self->pw)
		return;

	g_autoptr(GError) error = NULL;
	self->pending_input = r->input;
	self->pending_param = (int)p;
	if (!rs_pw_write_headamp(self->pw, self->selected, r->input, p, value, &error)) {
		/* Refused before it left: say so on the row and put the control back
		 * where the daemon's readback says it is. */
		note_set(r, error->message);
		self->pending_input = -1;
		self->pending_param = -1;
		sync_content(self);
		return;
	}
	note_set(r, _("Writing…"));
}

static void on_phantom_toggled(GObject *obj, GParamSpec *pspec G_GNUC_UNUSED,
                               gpointer user_data)
{
	InputRow *r = user_data;
	RsWindow *self = g_object_get_data(G_OBJECT(r->row), "rs-window");
	write_cell(self, r, RS_HEADAMP_PHANTOM,
	           adw_switch_row_get_active(ADW_SWITCH_ROW(obj)) ? 1 : 0);
}

static void on_pad_toggled(GObject *obj, GParamSpec *pspec G_GNUC_UNUSED,
                           gpointer user_data)
{
	InputRow *r = user_data;
	RsWindow *self = g_object_get_data(G_OBJECT(r->row), "rs-window");
	write_cell(self, r, RS_HEADAMP_PAD,
	           adw_switch_row_get_active(ADW_SWITCH_ROW(obj)) ? 1 : 0);
}

/* The slider carries dBu — the unit libreac publishes and the unit this
 * application says out loud. It is NOT the gain figure a mixing desk shows:
 * sensitivity runs the other way (the hottest setting is the most negative
 * number) and the two differ by a constant that is not settled, so nothing here
 * quietly adds ten to make them agree. */
static void on_sens_changed(GtkRange *range, gpointer user_data)
{
	InputRow *r = user_data;
	RsWindow *self = g_object_get_data(G_OBJECT(r->row), "rs-window");
	if (!self || self->syncing || !self->selected)
		return;
	int pad_on = adw_switch_row_get_active(r->pad) ? 1 : 0;
	int dbu = (int)gtk_range_get_value(range);
	int step = rs_headamp_sens_value(dbu, pad_on, rs_box_sens_max(self->selected));
	write_cell(self, r, RS_HEADAMP_SENS, step);
}

static char *on_sens_format(GtkScale *scale G_GNUC_UNUSED, double value,
                            gpointer user_data G_GNUC_UNUSED)
{
	/* dBu, named on the widget, because a bare number in a preamp row is the
	 * kind of thing two surfaces read ten decibels apart. */
	return g_strdup_printf(_("%d dBu"), (int)value);
}

/* ---- building the input rows -------------------------------------------- */

static void rows_clear(RsWindow *self)
{
	for (guint i = 0; i < self->rows->len; i++) {
		InputRow *r = g_ptr_array_index(self->rows, i);
		adw_preferences_group_remove(self->inputs_group, GTK_WIDGET(r->row));
	}
	g_ptr_array_set_size(self->rows, 0);
	self->rows_for_channels = -1;
}

static InputRow *input_row_new(RsWindow *self, RsBox *box, int input)
{
	InputRow *r = g_new0(InputRow, 1);
	r->input = input;

	r->row = ADW_EXPANDER_ROW(adw_expander_row_new());
	g_object_set_data(G_OBJECT(r->row), "rs-window", self);

	g_autofree char *title = g_strdup_printf(_("Input %d"), input);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r->row), title);

	/* The inline sentence: a refusal, or why this row cannot move. It lives in
	 * a label that is always allocated and only toggled visible, so a message
	 * appearing never reflows the list under the operator's finger. */
	r->note = GTK_LABEL(gtk_label_new(""));
	gtk_label_set_wrap(r->note, TRUE);
	gtk_label_set_xalign(r->note, 0.0f);
	gtk_widget_add_css_class(GTK_WIDGET(r->note), "caption");
	gtk_widget_add_css_class(GTK_WIDGET(r->note), "dim-label");
	gtk_widget_set_margin_start(GTK_WIDGET(r->note), 12);
	gtk_widget_set_margin_end(GTK_WIDGET(r->note), 12);
	gtk_widget_set_visible(GTK_WIDGET(r->note), FALSE);
	adw_expander_row_add_suffix(r->row, GTK_WIDGET(r->note));

	const char *caps = rs_box_caps(box);

	if (rs_headamp_caps_has(caps, "phantom")) {
		r->phantom = ADW_SWITCH_ROW(adw_switch_row_new());
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r->phantom),
		                              _("Phantom power (+48 V)"));
		g_signal_connect(r->phantom, "notify::active",
		                 G_CALLBACK(on_phantom_toggled), r);
		adw_expander_row_add_row(r->row, GTK_WIDGET(r->phantom));
	}

	if (rs_headamp_caps_has(caps, "pad")) {
		r->pad = ADW_SWITCH_ROW(adw_switch_row_new());
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r->pad),
		                              _("Pad (−20 dB)"));
		g_signal_connect(r->pad, "notify::active",
		                 G_CALLBACK(on_pad_toggled), r);
		adw_expander_row_add_row(r->row, GTK_WIDGET(r->pad));
	}

	if (rs_headamp_caps_has(caps, "sens")) {
		r->sens_row = ADW_ACTION_ROW(adw_action_row_new());
		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(r->sens_row),
		                              _("Sensitivity"));
		r->sens = GTK_SCALE(gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL,
		                                             -65, -10, 1));
		gtk_widget_set_hexpand(GTK_WIDGET(r->sens), TRUE);
		gtk_widget_set_size_request(GTK_WIDGET(r->sens), 260, -1);
		gtk_scale_set_draw_value(r->sens, TRUE);
		gtk_scale_set_value_pos(r->sens, GTK_POS_RIGHT);
		gtk_scale_set_format_value_func(r->sens, on_sens_format, NULL, NULL);
		g_signal_connect(r->sens, "value-changed", G_CALLBACK(on_sens_changed), r);
		adw_action_row_add_suffix(r->sens_row, GTK_WIDGET(r->sens));
		adw_expander_row_add_row(r->row, GTK_WIDGET(r->sens_row));
	}

	return r;
}

static void rows_build(RsWindow *self, RsBox *box)
{
	rows_clear(self);
	int channels = rs_box_channels(box);
	for (int i = 1; i <= channels; i++) {
		InputRow *r = input_row_new(self, box, i);
		g_ptr_array_add(self->rows, r);
		adw_preferences_group_add(self->inputs_group, GTK_WIDGET(r->row));
	}
	self->rows_for_channels = channels;
}

/* ---- rendering the model ------------------------------------------------ */

static void sync_row(RsWindow *self, InputRow *r, RsBox *box, enum rs_avail avail)
{
	gboolean live = (avail == RS_AVAIL_OK);
	gboolean readonly = (avail == RS_AVAIL_NO_READBACK);

	/* Insensitive, LAST VALUE HELD. Blanking the controls on a box that dropped
	 * would tell the operator the preamps had been zeroed, which is not what
	 * happened. */
	gtk_widget_set_sensitive(GTK_WIDGET(r->row), live);

	int wire_ch = rs_box_wire_ch(box, r->input);
	if (wire_ch >= 0) {
		g_autofree char *sub = g_strdup_printf(_("wire channel %d"), wire_ch);
		adw_expander_row_set_subtitle(r->row, sub);
	} else {
		adw_expander_row_set_subtitle(r->row, "");
	}

	int phantom = rs_box_asserted(box, r->input, RS_HEADAMP_PHANTOM);
	int pad     = rs_box_asserted(box, r->input, RS_HEADAMP_PAD);
	int sens    = rs_box_asserted(box, r->input, RS_HEADAMP_SENS);

	if (r->phantom && phantom >= 0)
		adw_switch_row_set_active(r->phantom, phantom != 0);
	if (r->pad && pad >= 0)
		adw_switch_row_set_active(r->pad, pad != 0);

	if (r->sens) {
		int pad_on = r->pad ? (adw_switch_row_get_active(r->pad) ? 1 : 0) : 0;
		int smax = rs_box_sens_max(box);
		/* The travel comes from the node when the node publishes one. dBu runs
		 * backwards against the step, so the range is [step max, step 0]. */
		gtk_range_set_range(GTK_RANGE(r->sens),
		                    rs_headamp_sens_dbu(smax, pad_on, smax),
		                    rs_headamp_sens_dbu(0, pad_on, smax));
		if (sens >= 0)
			gtk_range_set_value(GTK_RANGE(r->sens),
			                    rs_headamp_sens_dbu(sens, pad_on, smax));
	}

	/* The inline sentence. A pending write that came back refused names itself
	 * here; otherwise a row with no readback says so once, on the row. */
	const char *refused = rs_box_refused(box);
	g_autofree char *refusal = refusal_sentence(refused);
	if (refusal && self->pending_input == r->input)
		note_set(r, refusal);
	else if (readonly)
		note_set(r, _("No readback from this daemon."));
	else if (live && (phantom < 0 && pad < 0 && sens < 0))
		note_set(r, _("Nothing asserted for this input yet."));
	else
		note_set(r, NULL);
}

static void sync_content(RsWindow *self)
{
	RsBox *box = self->selected;

	if (!box) {
		gtk_stack_set_visible_child_name(self->content_stack, "empty");
		adw_window_title_set_title(self->content_title, _("REAC Stageboxes"));
		adw_window_title_set_subtitle(self->content_title, "");
		return;
	}

	gtk_stack_set_visible_child_name(self->content_stack, "page");
	adw_window_title_set_title(self->content_title, rs_box_model(box));
	g_autofree char *sub = rs_box_subtitle(box);
	adw_window_title_set_subtitle(self->content_title, sub);

	enum rs_avail avail = rs_box_availability(box);
	const char *sentence = avail_sentence(avail);
	adw_banner_set_title(self->banner, sentence ? sentence : "");
	adw_banner_set_revealed(self->banner, sentence != NULL);

	/* The row SHAPE follows the published channel count, so a box swapped for a
	 * wider one grows the page instead of leaving eight dead rows behind. */
	if (self->rows_for_channels != rs_box_channels(box))
		rows_build(self, box);

	self->syncing = TRUE;
	for (guint i = 0; i < self->rows->len; i++)
		sync_row(self, g_ptr_array_index(self->rows, i), box, avail);
	self->syncing = FALSE;

	/* A pending write is settled the moment the daemon re-published: either its
	 * refusal is now on the row, or the asserted value above is the answer. */
	self->pending_input = -1;
	self->pending_param = -1;
}

/* ---- the sidebar -------------------------------------------------------- */

static void sidebar_row_sync(GtkListBoxRow *lrow, RsBox *box)
{
	AdwActionRow *row = ADW_ACTION_ROW(lrow);
	const char *model = rs_box_model(box);
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),
	                              (model && *model && !g_str_equal(model, "none"))
	                                      ? model : _("No box"));
	g_autofree char *sub = rs_box_subtitle(box);
	adw_action_row_set_subtitle(row, sub);

	GtkWidget *badge = g_object_get_data(G_OBJECT(row), "rs-badge");
	if (GTK_IS_LABEL(badge)) {
		/* The link state, and — when we are not this segment's master — who is.
		 * An operator needs to see the difference between "no box" and "someone
		 * else's box" without opening the page. */
		const char *link = rs_box_link_state(box);
		if (!rs_box_is_sink_door(box))
			gtk_label_set_text(GTK_LABEL(badge), _("slave"));
		else
			gtk_label_set_text(GTK_LABEL(badge), link ? link : "");
	}
}

static GtkWidget *sidebar_row_new(RsBox *box)
{
	GtkWidget *row = adw_action_row_new();
	GtkWidget *badge = gtk_label_new("");
	gtk_widget_add_css_class(badge, "caption");
	gtk_widget_add_css_class(badge, "dim-label");
	g_object_set_data(G_OBJECT(row), "rs-badge", badge);
	g_object_set_data_full(G_OBJECT(row), "rs-box", g_object_ref(box),
	                       g_object_unref);
	adw_action_row_add_suffix(ADW_ACTION_ROW(row), badge);
	sidebar_row_sync(GTK_LIST_BOX_ROW(row), box);
	return row;
}

static GtkListBoxRow *sidebar_find(RsWindow *self, RsBox *box)
{
	for (GtkWidget *c = gtk_widget_get_first_child(GTK_WIDGET(self->sidebar));
	     c != NULL; c = gtk_widget_get_next_sibling(c)) {
		if (!GTK_IS_LIST_BOX_ROW(c))
			continue;
		if (g_object_get_data(G_OBJECT(c), "rs-box") == (gpointer)box)
			return GTK_LIST_BOX_ROW(c);
	}
	return NULL;
}

static void on_sidebar_selected(GtkListBox *listbox G_GNUC_UNUSED,
                                GtkListBoxRow *row, gpointer user_data)
{
	RsWindow *self = user_data;
	self->selected = row ? g_object_get_data(G_OBJECT(row), "rs-box") : NULL;
	self->rows_for_channels = -1;      /* a different box is a different shape */
	sync_content(self);
	adw_navigation_split_view_set_show_content(self->split, row != NULL);
}

static void on_box_added(RsPw *pw G_GNUC_UNUSED, RsBox *box, gpointer user_data)
{
	RsWindow *self = user_data;
	GtkWidget *row = sidebar_row_new(box);
	gtk_list_box_append(self->sidebar, row);
	if (!self->selected)
		gtk_list_box_select_row(self->sidebar, GTK_LIST_BOX_ROW(row));
}

static void on_box_changed(RsPw *pw G_GNUC_UNUSED, RsBox *box, gpointer user_data)
{
	RsWindow *self = user_data;
	GtkListBoxRow *row = sidebar_find(self, box);
	if (row)
		sidebar_row_sync(row, box);
	if (self->selected == box)
		sync_content(self);
}

static void on_box_removed(RsPw *pw G_GNUC_UNUSED, RsBox *box, gpointer user_data)
{
	RsWindow *self = user_data;
	GtkListBoxRow *row = sidebar_find(self, box);
	if (!row)
		return;
	if (self->selected == box) {
		self->selected = NULL;
		rows_clear(self);
	}
	gtk_list_box_remove(self->sidebar, GTK_WIDGET(row));
	sync_content(self);
}

/* ---- construction ------------------------------------------------------- */

static void rs_window_dispose(GObject *object)
{
	RsWindow *self = RS_WINDOW(object);
	g_clear_object(&self->pw);
	g_clear_pointer(&self->pw_error, g_free);
	g_clear_pointer(&self->rows, g_ptr_array_unref);
	G_OBJECT_CLASS(rs_window_parent_class)->dispose(object);
}

static void rs_window_class_init(RsWindowClass *klass)
{
	G_OBJECT_CLASS(klass)->dispose = rs_window_dispose;
}

static void rs_window_init(RsWindow *self)
{
	self->rows = g_ptr_array_new_with_free_func(g_free);
	self->rows_for_channels = -1;
	self->pending_input = -1;
	self->pending_param = -1;
}

RsWindow *rs_window_new(AdwApplication *app, RsPw *pw, const char *pw_error)
{
	RsWindow *self = g_object_new(RS_TYPE_WINDOW,
	                              "application", app,
	                              "default-width", 900,
	                              "default-height", 640,
	                              "title", _("REAC Stageboxes"),
	                              NULL);
	self->pw = pw ? g_object_ref(pw) : NULL;
	self->pw_error = g_strdup(pw_error);

	/* --- sidebar --- */
	self->sidebar = GTK_LIST_BOX(gtk_list_box_new());
	gtk_list_box_set_selection_mode(self->sidebar, GTK_SELECTION_SINGLE);
	gtk_widget_add_css_class(GTK_WIDGET(self->sidebar), "navigation-sidebar");
	g_signal_connect(self->sidebar, "row-selected",
	                 G_CALLBACK(on_sidebar_selected), self);

	GtkWidget *sidebar_scroll = gtk_scrolled_window_new();
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sidebar_scroll),
	                              GTK_WIDGET(self->sidebar));
	gtk_widget_set_vexpand(sidebar_scroll, TRUE);

	GtkWidget *sidebar_toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(sidebar_toolbar),
	                             adw_header_bar_new());
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(sidebar_toolbar), sidebar_scroll);

	AdwNavigationPage *sidebar_page = adw_navigation_page_new(sidebar_toolbar,
	                                                          _("Stageboxes"));

	/* --- content --- */
	self->banner = ADW_BANNER(adw_banner_new(""));
	self->inputs_group = ADW_PREFERENCES_GROUP(adw_preferences_group_new());
	adw_preferences_group_set_title(self->inputs_group, _("Inputs"));

	self->page = ADW_PREFERENCES_PAGE(adw_preferences_page_new());
	adw_preferences_page_add(self->page, self->inputs_group);

	/* The "nothing selected / nothing found" face. It says which of the two it
	 * is: a PipeWire that would not connect is a different fact from a graph
	 * with no REAC segments in it, and they look the same if one is not named. */
	self->empty = ADW_STATUS_PAGE(adw_status_page_new());
	adw_status_page_set_icon_name(self->empty, "audio-input-microphone-symbolic");
	if (self->pw_error) {
		adw_status_page_set_title(self->empty, _("No PipeWire session"));
		adw_status_page_set_description(self->empty, self->pw_error);
	} else {
		adw_status_page_set_title(self->empty, _("No stageboxes"));
		adw_status_page_set_description(self->empty,
		        _("No REAC segment is present in the PipeWire graph. reac-pw publishes one node per segment; a segment appears here as soon as it does."));
	}

	self->content_stack = GTK_STACK(gtk_stack_new());
	gtk_stack_add_named(self->content_stack, GTK_WIDGET(self->empty), "empty");
	gtk_stack_add_named(self->content_stack, GTK_WIDGET(self->page), "page");
	gtk_widget_set_vexpand(GTK_WIDGET(self->content_stack), TRUE);

	GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_append(GTK_BOX(content_box), GTK_WIDGET(self->banner));
	gtk_box_append(GTK_BOX(content_box), GTK_WIDGET(self->content_stack));

	self->content_title = ADW_WINDOW_TITLE(adw_window_title_new(_("REAC Stageboxes"), ""));
	GtkWidget *content_header = adw_header_bar_new();
	adw_header_bar_set_title_widget(ADW_HEADER_BAR(content_header),
	                                GTK_WIDGET(self->content_title));

	GtkWidget *content_toolbar = adw_toolbar_view_new();
	adw_toolbar_view_add_top_bar(ADW_TOOLBAR_VIEW(content_toolbar), content_header);
	adw_toolbar_view_set_content(ADW_TOOLBAR_VIEW(content_toolbar), content_box);

	AdwNavigationPage *content_page = adw_navigation_page_new(content_toolbar,
	                                                          _("Preamps"));

	self->split = ADW_NAVIGATION_SPLIT_VIEW(adw_navigation_split_view_new());
	adw_navigation_split_view_set_sidebar(self->split, sidebar_page);
	adw_navigation_split_view_set_content(self->split, content_page);
	adw_navigation_split_view_set_min_sidebar_width(self->split, 240);

	adw_application_window_set_content(ADW_APPLICATION_WINDOW(self),
	                                   GTK_WIDGET(self->split));

	if (self->pw) {
		g_signal_connect(self->pw, "box-added", G_CALLBACK(on_box_added), self);
		g_signal_connect(self->pw, "box-changed", G_CALLBACK(on_box_changed), self);
		g_signal_connect(self->pw, "box-removed", G_CALLBACK(on_box_removed), self);

		/* Whatever the registry already delivered before the window existed. */
		GPtrArray *boxes = rs_pw_boxes(self->pw);
		for (guint i = 0; boxes && i < boxes->len; i++)
			on_box_added(self->pw, g_ptr_array_index(boxes, i), self);
	}

	sync_content(self);
	return self;
}
