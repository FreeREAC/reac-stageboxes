#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Guards on .github/workflows/release-rpm.yml. Each is a way a release run could
# publish what nobody asked it to, or sign with a key nobody means to keep:
#
#   trigger   a branch push or a release-tag push starts a run (only a dryrun-v
#             tag push and a dispatch may)
#   secrets   a secret other than the packages key, its passphrase or the pages
#             token is read -- above all the retired personal RPM_GPG_KEY
#   gate      a step that reads a secret, pushes, or uploads a release asset is
#             not gated on env.MODE == 'publish', so a dry run could reach it
#   expr      a ${{ }} expression sits inside a run: block, where an input (a tag
#             name) would be spliced into shell
#   unsigned  publish-repo.sh is called with --no-sign
#
# Only the workflow's text is read: awk, no YAML parser, because an rpm build
# runs this and has none. Steps start at six spaces and a dash.
#
#   tests/release-workflow.sh [file]   check a workflow (default: the repo's)
#   tests/release-workflow.sh --self-test
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"

# <file>: prints one "<guard>: <what>" line per violation; exit 1 if any.
scan() {
	local found
	found=$(awk '
	function flush() {
		if (!inside) return
		if ((secret || push || release) && gate != "publish")
			printf "gate: step \"%s\" reads a secret, pushes or uploads a release asset without if: env.MODE == '\''publish'\''\n", name
		inside = 0
	}
	/^on:/ { section = "on" }
	/^[a-z]/ && !/^on:/ { if (section == "on") section = "" }
	section == "on" && /^    branches/ { printf "trigger: a branch push starts a run\n" }
	section == "on" && /^      - / && prev_tags && $0 !~ /dryrun-v\[0-9\]\*/ { printf "trigger: tag pattern %s is not dryrun-v[0-9]*\n", $2 }
	section == "on" { prev_tags = ($0 ~ /^    tags:/) || (prev_tags && /^      - /) }
	/^      - / && section != "on" { flush(); inside = 1; name = $0; sub(/^      - (name: )?/, "", name); gate = ""; secret = 0; push = 0; release = 0; inrun = 0 }
	inside && /^        if: env\.MODE == .publish.[[:space:]]*$/ { gate = "publish" }
	inside && /secrets\./ {
		s = $0
		while (match(s, /secrets\.[A-Za-z0-9_]+/)) {
			n = substr(s, RSTART + 8, RLENGTH - 8)
			if (n != "PACKAGES_GPG_KEY" && n != "PACKAGES_GPG_PASSPHRASE" && n != "FREEREAC_PAGES_TOKEN")
				printf "secrets: secrets.%s is not one of the packages key, its passphrase or the pages token\n", n
			s = substr(s, RSTART + RLENGTH)
		}
		secret = 1
	}
	inside && /git push/ { push = 1 }
	inside && /gh release (create|upload)/ { release = 1 }
	/publish-repo\.sh.*--no-sign/ { printf "unsigned: publish-repo.sh is called with --no-sign\n" }
	/^        run:/ { inrun = 1; if ($0 ~ /\$\{\{/) printf "expr: a ${{ }} expression in the run: of step \"%s\"\n", name; next }
	inrun && /^          / { if ($0 ~ /\$\{\{/) printf "expr: a ${{ }} expression in the run: of step \"%s\"\n", name; next }
	inrun && !/^[[:space:]]*$/ { inrun = 0 }
	END { flush() }
	' "$1" | sort -u)
	[ -z "$found" ] || { printf '%s\n' "$found"; return 1; }
}

# shellcheck disable=SC2016  # a literal ${{ }} is the point
fixture() { # <name> -> a workflow with one violation of the named guard (or none)
	local trig='    tags:\n      - '"'dryrun-v[0-9]*'"'' sec='${{ secrets.PACKAGES_GPG_KEY }}' gate="        if: env.MODE == 'publish'" run='echo "$X"' push='git push origin main'
	case $1 in
	trigger-branch) trig='    branches:\n      - main' ;;
	trigger-tag) trig='    tags:\n      - '"'v[0-9]*'" ;;
	secret-retired) sec='${{ secrets.RPM_GPG_KEY }}' ;;
	secret-other) sec='${{ secrets.SOMETHING_ELSE }}' ;;
	ungated-secret) gate="" ;;
	expr-in-run) run='echo "${{ inputs.tag }}"' ;;
	ungated-push) push='git push origin main'; gate="" ; sec='nothing' ;;
	ungated-release) push='gh release upload v1 x.rpm'; gate=""; sec='nothing' ;;
	unsigned) run='packaging/publish-repo.sh --rpm-dir c --no-sign' ;;
	esac
	printf 'name: x\non:\n  push:\n%b\npermissions:\n  contents: read\njobs:\n  j:\n    steps:\n      - name: dry\n        run: |\n          %s\n      - name: pub\n%s\n        env:\n          K: %s\n        run: |\n          %s\n' \
		"$trig" "$run" "${gate:+$gate}" "$sec" "$push" | sed '/^$/d'
}

self_test() {
	local d g ok=1
	d=$(mktemp -d)
	trap 'rm -rf "$d"' RETURN
	fixture clean >"$d/clean.yml"
	scan "$d/clean.yml" >/dev/null || { echo "release-workflow self-test: the clean fixture was flagged:" >&2; scan "$d/clean.yml" >&2; ok=0; }
	for g in trigger-branch trigger-tag secret-retired secret-other ungated-secret expr-in-run ungated-push ungated-release unsigned; do
		fixture "$g" >"$d/$g.yml"
		scan "$d/$g.yml" >/dev/null && { echo "release-workflow self-test: $g was not flagged" >&2; ok=0; }
	done
	[ $ok = 1 ]
}

case "${1:-}" in
--self-test) self_test && echo "release-workflow: self-test ok"; exit ;;
esac

wf=${1:-$root/.github/workflows/release-rpm.yml}
[ -f "$wf" ] || { echo "release-workflow: $wf does not exist" >&2; exit 1; }
if ! out=$(scan "$wf"); then
	echo "release-workflow: $wf breaks a guard" >&2
	printf '  %s\n' "$out" >&2
	exit 1
fi
echo "release-workflow: $(basename "$wf") passes every guard"
