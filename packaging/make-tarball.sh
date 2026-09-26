#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Assemble the reac-stageboxes source tarball for rpmbuild: Source0 of
# packaging/reac-stageboxes.spec, `%{name}-%{version}.tar.gz` with a
# `%{name}-%{version}/` top directory (the spec's %autosetup reads that). Writes
# reac-stageboxes-<version>.tar.gz to the repo root, where .gitignore keeps it
# out of the tree.
#
# THE TREE IS GIT'S, NOT THE CHECKOUT'S. `git archive HEAD` ships exactly what is
# committed, so a stray build directory, a local po/*.mo or an uncommitted edit
# can never ride into a package that claims to be a tag.
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
# Version single-source: meson.build's project version. A release build passes
# the tag's version as $1 and build-rpm.sh refuses a tag that disagrees with it.
MESON_V=$(sed -n "s/^ *version *: *'\([^']*\)'.*/\1/p" "$ROOT/meson.build" | head -1)
V="${1:-$MESON_V}"
[ -n "$V" ] || { echo "could not read version from $ROOT/meson.build" >&2; exit 1; }
git -C "$ROOT" archive --format=tar.gz --prefix="reac-stageboxes-$V/" \
	-o "$ROOT/reac-stageboxes-$V.tar.gz" HEAD
echo "wrote $ROOT/reac-stageboxes-$V.tar.gz"
