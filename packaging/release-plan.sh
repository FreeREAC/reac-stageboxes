#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# Decide what a release run does, and refuse the runs that must not happen.
# Writes ref=, version= and mode= to $GITHUB_OUTPUT.
#
#   push of a dryrun-v<version> tag    builds and signs that tag with a throwaway
#                                      key; mode=dry-run, nothing is published
#   workflow_dispatch, tag v<version>  builds that tag; publish=true makes it
#                                      mode=publish, from the default branch only
#
# A release tag push is deliberately not a trigger: a publish is a human decision.
# The inputs arrive through the environment, never spliced into a shell line.
set -euo pipefail

out=${GITHUB_OUTPUT:-/dev/stdout}
die() { echo "::error::$*" >&2; exit 1; }

event=${EVENT_NAME:-}
ref_name=${REF_NAME:-}
mode=dry-run

case "$event" in
push)
	case "$ref_name" in
	dryrun-v[0-9]*) ref=$ref_name; version=${ref_name#dryrun-v} ;;
	*) die "a tag push builds only dryrun-v[0-9]*; '$ref_name' is not one" ;;
	esac
	;;
workflow_dispatch)
	tag=${INPUT_TAG:-}
	case "$tag" in
	v[0-9]*) ;;
	*) die "'$tag' does not match the release-tag glob v[0-9]*" ;;
	esac
	ref=$tag
	version=${tag#v}
	if [ "${INPUT_PUBLISH:-}" = true ]; then
		[ "$ref_name" = "${DEFAULT_BRANCH:-}" ] \
			|| die "publish runs from the default branch (${DEFAULT_BRANCH:-unset}); this run is on '$ref_name'"
		mode=publish
	fi
	;;
*) die "event '$event' does not start a release run" ;;
esac

[[ $version =~ ^[0-9]+(\.[0-9]+)*$ ]] || die "'$version' is not an RPM version (digits and dots)"

{
	echo "ref=$ref"
	echo "version=$version"
	echo "mode=$mode"
} >>"$out"
echo "release plan: ref=$ref version=$version mode=$mode"
