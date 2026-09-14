// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* rs-list — `reac-stageboxes --list`: the walk, printed, with no display.
 *
 * It exists so the registry walk can be PROVEN against a live graph over a
 * terminal, which is the one part of this application no offline test can
 * reach. It only ever reads: it binds nodes and prints what they publish, and
 * writes no param.
 *
 * Exit status: 0 when at least one segment was found, 1 when none was. The
 * count of BOUND NODES is printed either way, because "no stageboxes" and "no
 * graph" are different facts that look identical in an empty list. */
#ifndef RS_LIST_H
#define RS_LIST_H

#include <glib.h>

/* Runs its own main loop, prints, returns the process exit status. */
int rs_list_run(void);

#endif /* RS_LIST_H */
