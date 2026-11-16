#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# THE TWO PACKAGES THIS DEPENDS ON ARE BOUNDED, in the spec and in debian/control alike: libreac (its header,
# pkgconfig(libreac) / libreac-dev) and reac-pw (the daemon whose nodes this reads), each with a floor and a ceiling,
# the ceiling the next MAJOR (the rule from 1.0 on: a minor keeps the soname and the nodes). Without a ceiling a
# release of either leaves this installable against a version it was never built for; without a floor at the
# versions this release pairs with, the channel resolves it against an older libreac through an older reac-pw.
set -euo pipefail
root=${1:?source root}
spec=$root/packaging/reac-stageboxes.spec ctl=$root/debian/control
LIBREAC_FLOOR=1.12.0 REACPW_FLOOR=1.0.44 CEIL=2
bad=0
say() { echo "deps-bounded: $*" >&2; bad=1; }
want() { # <file> <label> <regex that must match one line>
	grep -qE "$3" "$1" || say "${1##*/}: $2"
}
want "$spec" "pkgconfig(libreac) >= $LIBREAC_FLOOR" "^BuildRequires: +pkgconfig\(libreac\) >= ${LIBREAC_FLOOR//./\\.}$"
want "$spec" "pkgconfig(libreac) < $CEIL" "^BuildRequires: +pkgconfig\(libreac\) < $CEIL$"
want "$spec" "reac-pw >= $REACPW_FLOOR" "^Requires: +reac-pw >= ${REACPW_FLOOR//./\\.}$"
want "$spec" "reac-pw < $CEIL" "^Requires: +reac-pw < $CEIL$"
want "$ctl" "libreac-dev (>= $LIBREAC_FLOOR~), (<< $CEIL~)" "libreac-dev \(>= ${LIBREAC_FLOOR//./\\.}~\), libreac-dev \(<< $CEIL~\)"
want "$ctl" "reac-pw (>= $REACPW_FLOOR~), (<< $CEIL~)" "reac-pw \(>= ${REACPW_FLOOR//./\\.}~\), reac-pw \(<< $CEIL~\)"
[ "$bad" = 0 ] && echo "OK: libreac >= $LIBREAC_FLOOR < $CEIL and reac-pw >= $REACPW_FLOOR < $CEIL in the spec and debian/control"
exit "$bad"
