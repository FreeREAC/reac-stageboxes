#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Build this package's RPMs: the ONE build entry point, called by hand on a
# desk and by .github/workflows/release-rpm.yml at a release tag. The workflow
# does not rebuild this logic in YAML; a second copy would drift unseen.
#
#   packaging/build-rpm.sh            # version from meson.build
#   packaging/build-rpm.sh 0.1.1      # a release: must EQUAL meson.build's
#   packaging/build-rpm.sh --check-version 0.1.1   # the refusal alone, no build
#
# THE VERSION IS NOT OVERRIDDEN, IT IS CHECKED. meson.build's project version is
# what the binary, the metainfo and the translations carry; an RPM whose
# Version: says something else would ship one number on the box and another
# inside it, and RPM has no downgrade path to take it back. So a release version
# that disagrees with meson.build is refused here, before anything is built.
#
# _topdir is the PHYSICAL path of ~/rpmbuild (reac-pw's build-rpm.sh has the
# measurement: through a symlink debugedit finds no sources and the build dies
# at the very end).
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SPEC="$ROOT/packaging/reac-stageboxes.spec"
TOP=$(readlink -f "${RPM_TOPDIR:-$HOME/rpmbuild}")
MESON_V=$(sed -n "s/^ *version *: *'\([^']*\)'.*/\1/p" "$ROOT/meson.build" | head -1)
[ -n "$MESON_V" ] || { echo "could not read version from $ROOT/meson.build" >&2; exit 1; }
CHECK_ONLY=0
if [ "${1:-}" = "--check-version" ]; then CHECK_ONLY=1; shift; fi
V="${1:-$MESON_V}"
if [ "$V" != "$MESON_V" ]; then
	echo "REFUSING: release version '$V' is not meson.build's project version '$MESON_V'." >&2
	echo "          Bump meson.build (and the spec's %changelog) in a commit, tag THAT commit." >&2
	exit 1
fi
[ "$CHECK_ONLY" = 1 ] && { echo "$V"; exit 0; }
sh "$ROOT/packaging/make-tarball.sh" "$V"
mkdir -p "$TOP/SOURCES"
mv "$ROOT/reac-stageboxes-$V.tar.gz" "$TOP/SOURCES/"
rpmbuild -ba --define "_topdir $TOP" --define "version_override $V" "$SPEC"
