#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# packaging/release-plan.sh decides what a release run does: which ref it builds,
# which version that is, and whether it publishes. A publish is a human decision
# made from the default branch, never a side effect of a tag push, so every row
# of the table below is a way the decision could go wrong.
#
#   tests/release-plan.sh [path/to/release-plan.sh]
set -uo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
plan=${1:-$root/packaging/release-plan.sh}

fail=0
# <name> <expect: ok|refuse> <expected outputs, space separated, or ""> -- env assignments...
case_() {
	local name=$1 expect=$2 want=$3 out rc got
	shift 4
	out=$(mktemp)
	env -i PATH="$PATH" GITHUB_OUTPUT="$out" DEFAULT_BRANCH=main "$@" bash "$plan" >/dev/null 2>&1
	rc=$?
	got=$(tr '\n' ' ' <"$out" | sed 's/ $//')
	rm -f "$out"
	if [ "$expect" = ok ] && [ $rc -eq 0 ] && [ "$got" = "$want" ]; then
		echo "ok   release-plan: $name"
	elif [ "$expect" = refuse ] && [ $rc -ne 0 ] && [ -z "$got" ]; then
		echo "ok   release-plan: $name"
	else
		echo "FAIL release-plan: $name (rc=$rc outputs='$got')"
		fail=1
	fi
}

[ -f "$plan" ] || { echo "FAIL release-plan: $plan does not exist"; exit 1; }

case_ "a dry-run tag push builds that tag at its version" ok \
	"ref=dryrun-v0.1.0 version=0.1.0 mode=dry-run" -- \
	EVENT_NAME=push REF_NAME=dryrun-v0.1.0
case_ "a release tag push is not a trigger" refuse "" -- \
	EVENT_NAME=push REF_NAME=v0.1.0
case_ "a branch push is not a trigger" refuse "" -- \
	EVENT_NAME=push REF_NAME=main
case_ "a dispatch without publish is a dry run of that tag" ok \
	"ref=v0.2.0 version=0.2.0 mode=dry-run" -- \
	EVENT_NAME=workflow_dispatch INPUT_TAG=v0.2.0 INPUT_PUBLISH=false REF_NAME=lane/x
case_ "a dispatch with publish from the default branch publishes" ok \
	"ref=v0.2.0 version=0.2.0 mode=publish" -- \
	EVENT_NAME=workflow_dispatch INPUT_TAG=v0.2.0 INPUT_PUBLISH=true REF_NAME=main
case_ "a publish from any other branch is refused" refuse "" -- \
	EVENT_NAME=workflow_dispatch INPUT_TAG=v0.2.0 INPUT_PUBLISH=true REF_NAME=lane/x
case_ "a dispatch tag that is not a release tag is refused" refuse "" -- \
	EVENT_NAME=workflow_dispatch INPUT_TAG=nightly INPUT_PUBLISH=false REF_NAME=main
case_ "a dispatch tag without the v is refused" refuse "" -- \
	EVENT_NAME=workflow_dispatch INPUT_TAG=0.2.0 INPUT_PUBLISH=false REF_NAME=main
case_ "a dispatch tag with a non-numeric version is refused" refuse "" -- \
	EVENT_NAME=workflow_dispatch INPUT_TAG=v1abc INPUT_PUBLISH=false REF_NAME=main
case_ "a dispatch tag carrying a newline cannot forge an output" refuse "" -- \
	EVENT_NAME=workflow_dispatch "INPUT_TAG=$(printf 'v1.0\nmode=publish')" INPUT_PUBLISH=false REF_NAME=main
case_ "any other event is refused" refuse "" -- \
	EVENT_NAME=schedule REF_NAME=main

exit $fail
