#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Build reac-stageboxes' RPMs from the checked-out tree.
#
#   packaging/build-rpm.sh <version>
#
# The version is the release tag without its v. It must be the one meson.build
# and the spec both carry: the version lives in meson.build, rpm cannot read it,
# so the spec holds a copy and this refuses a build where any of the three
# disagree -- the only moment a copy can be caught, and RPM has no downgrade path.
#
# _topdir is the PHYSICAL path of ~/rpmbuild (or $RPM_TOPDIR): where ~/rpmbuild
# is a symlink, debugedit looks under the logical path and finds no sources.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
V=${1:?usage: build-rpm.sh <version>}

MESON_V=$(sed -n "s/^  version : '\([^']*\)'.*/\1/p" "$ROOT/meson.build" | head -1)
[ -n "$MESON_V" ] || { echo "build-rpm.sh: could not read version from meson.build" >&2; exit 1; }
SPEC_V=$(sed -n 's/^Version:.*%{!?version_override:\([^}]*\)}.*/\1/p' "$ROOT/packaging/reac-stageboxes.spec" | head -1)
[ -n "$SPEC_V" ] || { echo "build-rpm.sh: could not read the version default from packaging/reac-stageboxes.spec" >&2; exit 1; }

if [ "$V" != "$MESON_V" ] || [ "$SPEC_V" != "$MESON_V" ]; then
	echo "build-rpm.sh: version drift: tag $V, meson.build $MESON_V, packaging/reac-stageboxes.spec $SPEC_V" >&2
	echo "  meson.build is the definition. Bring the spec to it, and tag that version." >&2
	exit 1
fi

TOP=$(readlink -f "${RPM_TOPDIR:-$HOME/rpmbuild}")
mkdir -p "$TOP/SOURCES"
git -C "$ROOT" archive --format=tar.gz --prefix="reac-stageboxes-$V/" \
	-o "$TOP/SOURCES/reac-stageboxes-$V.tar.gz" HEAD
rpmbuild -ba --define "_topdir $TOP" "$ROOT/packaging/reac-stageboxes.spec"
