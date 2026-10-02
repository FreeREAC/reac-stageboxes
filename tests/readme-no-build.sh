#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# The README sells the package and says how to install it; the build is in
# BUILDING.md. This fails when a build command (make, meson, cmake, ninja,
# pnpm build, rpmbuild) appears in the README's code -- a fenced block or an
# inline `span` -- which is where a command a reader would copy lives. Prose is
# not read: "make" is also an English verb.
#
#   tests/readme-no-build.sh [file]   check a README (default: the repo's)
#   tests/readme-no-build.sh --self-test
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"

# <file>: prints "line N: <text>" for each offending line, exit 1 if any.
scan() {
	awk '
	function bad(s) {
		return s ~ /(^|[[:space:];&|(`$])(sudo[[:space:]]+)?(make|meson|cmake|ninja|rpmbuild|pnpm[[:space:]]+build)([[:space:]]|$)/
	}
	/^[[:space:]]*(```|~~~)/ { fence = !fence; next }
	fence { if (bad($0)) { printf "line %d: %s\n", NR, $0; n++ }; next }
	{
		line = $0
		while (match(line, /`[^`]+`/)) {
			span = substr(line, RSTART + 1, RLENGTH - 2)
			if (bad(span)) { printf "line %d: %s\n", NR, $0; n++; break }
			line = substr(line, RSTART + RLENGTH)
		}
	}
	END { exit n > 0 }
	' "$1"
}

self_test() {
	local d ok=1
	d=$(mktemp -d)
	trap 'rm -rf "$d"' RETURN
	printf '# x\n\nWe make it easy; `tools/make-tarball.sh` is a tool.\n\n```\nsudo dnf install ./reac-stageboxes-*.rpm\n```\n' > "$d/clean.md"
	printf '```\n$ make check\n```\n' > "$d/fenced.md"
	printf 'Run `make tests` first.\n' > "$d/inline.md"
	printf '```\ntools/make-tarball.sh && rpmbuild -ta x.tar.gz\n```\n' > "$d/rpm.md"
	scan "$d/clean.md" >/dev/null || { echo "readme-no-build self-test: prose and a make-* tool were flagged" >&2; ok=0; }
	for f in fenced inline rpm; do
		if scan "$d/$f.md" >/dev/null; then echo "readme-no-build self-test: $f.md was not flagged" >&2; ok=0; fi
	done
	[ $ok = 1 ]
}

case "${1:-}" in
--self-test) self_test && echo "readme-no-build: self-test ok"; exit ;;
esac

readme=${1:-$root/README.md}
if ! out=$(scan "$readme"); then
	echo "readme-no-build: $readme carries a build command; it belongs in BUILDING.md" >&2
	printf '  %s\n' "$out" >&2
	exit 1
fi
echo "readme-no-build: $(basename "$readme") carries no build command"
