#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# THE CATALOGUE IS ACTUALLY BOUND, from the build tree.
#
# The operator's desktop is Catalan and the application showed English, because
# a build-tree run bound the INSTALL locale dir — where no .mo exists — and
# gettext's fallback for a missing catalogue is the msgid itself. Complete
# translations and a correct install path both looked fine; the program just
# never found them. Nothing but running it and reading the output can tell.
#
# So this runs the real binary with LANGUAGE=ca against the catalogues meson
# just built, and requires Catalan out of it. It does NOT need a stagebox: with
# no PipeWire it prints the Catalan connection refusal, with a graph it prints
# the Catalan summary, and both are proof the binding works.
set -o pipefail
BIN=${1:?binary}
LOCALE_DIR=${2:?locale dir}

# gettext IGNORES LANGUAGE when the locale is C or POSIX, so a run without a
# real locale would print English and be recorded as a failure of the binding
# rather than of the environment. Say which it is.
if ! locale -a 2>/dev/null | grep -qiE '^ca_ES\.?(utf-?8)?$'; then
	echo "SKIP: no ca_ES.UTF-8 locale on this machine, so LANGUAGE=ca cannot take"
	echo "      effect and this test cannot observe anything. Install glibc-langpack-ca."
	exit 77                       # meson: skipped, and reported as skipped
fi

# THE OPERATOR'S CASE FIRST, and it is the one that was broken: run the binary
# straight out of the build tree with NO override, so the only thing that can
# find the catalogues is the binary's own build-relative fallback. Testing only
# the env-var path would pass on exactly the build that failed on his desk.
out=$(LANGUAGE=ca LC_ALL=ca_ES.UTF-8 "$BIN" --list 2>&1)
echo "$out" | head -3

# One of these two Catalan sentences must be there. Matching a WHOLE phrase, not
# a stray accented letter: a single "é" could come from anywhere.
if echo "$out" | grep -qF "segments REAC ("   \
   || echo "$out" | grep -qF "segment REAC (" \
   || echo "$out" | grep -qF "No s'ha trobat cap segment REAC" \
   || echo "$out" | grep -qF "No s'ha pogut connectar amb PipeWire"; then
	:
else
	echo "FAIL: --list printed no Catalan with no override, so the build-relative" >&2
	echo "      fallback did not find the catalogues meson built. This is the defect" >&2
	echo "      that gave a Catalan desktop an English window." >&2
	exit 1
fi

# AND THE DOCUMENTED OVERRIDE, pointed at the same catalogues.
env_out=$(LANGUAGE=ca LC_ALL=ca_ES.UTF-8 REAC_STAGEBOXES_LOCALEDIR="$LOCALE_DIR" \
          "$BIN" --list 2>&1)
if [ "$env_out" != "$out" ]; then
	echo "FAIL: REAC_STAGEBOXES_LOCALEDIR=$LOCALE_DIR gave different output from" >&2
	echo "      the build-relative fallback, though both name the same catalogues." >&2
	exit 1
fi

# THE NEGATIVE CONTROL. If the same command in English produced the same text,
# the match above would prove nothing — it would mean the strings are identical
# in both languages, or that neither catalogue is loading.
en=$(LANGUAGE=en LC_ALL=ca_ES.UTF-8 REAC_STAGEBOXES_LOCALEDIR="$LOCALE_DIR" \
     "$BIN" --list 2>&1)
if [ "$out" = "$en" ]; then
	echo "FAIL: the Catalan and English runs printed identical text, so the match" >&2
	echo "      above proves nothing about the binding." >&2
	exit 1
fi
echo "Catalan bound from the build tree and from $LOCALE_DIR (differs from English)"
exit 0
