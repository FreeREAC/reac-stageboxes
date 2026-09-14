// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

#ifndef RS_WINDOW_H
#define RS_WINDOW_H

#include <adwaita.h>

#include "rs-pw.h"

G_BEGIN_DECLS

#define RS_TYPE_WINDOW (rs_window_get_type())
G_DECLARE_FINAL_TYPE(RsWindow, rs_window, RS, WINDOW, AdwApplicationWindow)

/* `pw` may be NULL: a session with no PipeWire is a state the window RENDERS,
 * with the reason on the page, rather than an empty list an operator would read
 * as "no stageboxes". */
RsWindow *rs_window_new(AdwApplication *app, RsPw *pw, const char *pw_error);

G_END_DECLS

#endif /* RS_WINDOW_H */
