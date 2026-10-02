#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# packaging/build-rpm.sh turns a release tag into an RPM. It must refuse a tag
# whose version is not the one meson.build and the spec both carry (RPM has no
# downgrade path, and a tarball named for one version holding another is a
# package that lies), and the package it names must carry the fmx suffix.
# rpmbuild is a stub here: the proof is what build-rpm.sh hands it.
#
#   tests/build-rpm.sh
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
# an rpm build unpacks a tarball: no repository of its own, so no `git archive`
# to prove (a build dir inside some other work tree does not count)
if [ "$(git -C "$root" rev-parse --show-toplevel 2>/dev/null)" != "$root" ]; then
	echo "SKIP build-rpm: $root is not the top of a git work tree"
	exit 77
fi
fail=0
ok() { echo "ok   build-rpm: $1"; }
bad() { echo "FAIL build-rpm: $1"; [ -n "${2:-}" ] && printf '     | %s\n' "$2"; fail=1; }

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# a throwaway git repo holding a copy of the working tree, so a drift can be
# made without touching the real one
git -C "$root" ls-files -z --cached --others --exclude-standard | (cd "$root" && xargs -0 cp --parents -t "$work") || exit 1
git -C "$work" init -q -b main
git -C "$work" add -A
git -C "$work" -c user.email=t@example.com -c user.name=T -c commit.gpgsign=false commit -qm copy

mkdir -p "$work/bin"
cat >"$work/bin/rpmbuild" <<'EOF'
#!/bin/sh
echo "$@" >>"$STUB_LOG"
EOF
chmod +x "$work/bin/rpmbuild"

run() { # <version> -> output in $out, status in $rc
	rm -rf "$work/top" "$work/stub.log"
	out=$(cd "$work" && PATH="$work/bin:$PATH" STUB_LOG="$work/stub.log" RPM_TOPDIR="$work/top" \
		sh packaging/build-rpm.sh "$1" 2>&1)
	rc=$?
}

meson_v=$(sed -n "s/^  version : '\\([^']*\\)'.*/\\1/p" "$work/meson.build")
[ -n "$meson_v" ] || { bad "meson.build carries a version"; exit 1; }

run "$meson_v"
if [ $rc -eq 0 ] && grep -q -- "-ba" "$work/stub.log" && grep -q -- "packaging/reac-stageboxes.spec" "$work/stub.log"; then
	ok "the version meson.build and the spec carry reaches rpmbuild"
else bad "the matching version did not reach rpmbuild (rc=$rc)" "$out"; fi

tarball="$work/top/SOURCES/reac-stageboxes-$meson_v.tar.gz"
if [ -f "$tarball" ] && tar -tzf "$tarball" | grep -x "reac-stageboxes-$meson_v/meson.build" >/dev/null; then
	ok "the source tarball is named for the version and holds the tree"
else bad "no tarball reac-stageboxes-$meson_v.tar.gz with the tree under its own prefix"; fi

run "9.9.9"
if [ $rc -ne 0 ] && echo "$out" | grep -q "9.9.9" && [ ! -e "$work/stub.log" ]; then
	ok "a tag version that is not meson.build's is refused before rpmbuild"
else bad "a drifted tag version was not refused (rc=$rc)" "$out"; fi

sed -i "s/%{!?version_override:$meson_v}/%{!?version_override:0.0.1}/" "$work/packaging/reac-stageboxes.spec"
run "$meson_v"
if [ $rc -ne 0 ] && echo "$out" | grep -qi "spec" && [ ! -e "$work/stub.log" ]; then
	ok "a spec whose version is not meson.build's is refused before rpmbuild"
else bad "a drifted spec version was not refused (rc=$rc)" "$out"; fi
cp "$root/packaging/reac-stageboxes.spec" "$work/packaging/reac-stageboxes.spec"

if grep -Eq '^Release:[[:space:]]+[^[:space:]]*%\{\?dist\}\.fmx[[:space:]]*$' "$root/packaging/reac-stageboxes.spec"; then
	ok "the spec's Release ends in the fmx suffix, after the dist tag"
else bad "the spec's Release is not <n>%{?dist}.fmx"; fi

exit $fail
