#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# One release is one version. meson.build defines it; the RPM spec, the Debian
# changelog and the newest AppStream release entry each carry a copy, and a tag
# is v<version>. A copy that drifts makes the shared release build refuse the
# tag, or worse, publish a package that names another version.
set -euo pipefail
root=${1:?source root}

meson_v=$(sed -n "s/^  version : '\([^']*\)'.*/\1/p" "$root/meson.build" | head -1)
spec_v=$(sed -n 's/^Version: *//p' "$root/packaging/reac-stageboxes.spec" | head -1)
deb_v=$(sed -n '1s/^[^ ]* (\([^)]*\)).*/\1/p' "$root/debian/changelog")
# a snapshot build writes <version>~git<sha> there; the part before the tilde is the version
deb_v=${deb_v%%~*}
info_v=$(sed -n 's/.*<release version="\([^"]*\)".*/\1/p' "$root/data/org.freereac.Stageboxes.metainfo.xml" | head -1)

echo "meson $meson_v, spec $spec_v, debian $deb_v, metainfo $info_v"
[ -n "$meson_v" ] || { echo "version-agree: no version in meson.build" >&2; exit 1; }
for v in "$spec_v" "$deb_v" "$info_v"; do
	[ "$v" = "$meson_v" ] || { echo "version-agree: the versions above disagree" >&2; exit 1; }
done
