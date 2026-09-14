#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
#
# EVERY USER-VISIBLE STRING IS IN THE CATALOGUE, and every catalogue translates
# all of them. The failure this catches has no symptom: a string added without
# `_()` never reaches the .pot, a msgid added without a translation falls back
# to English, and in both cases the program runs and prints something. Nothing
# logs, nothing errors — a Catalan desktop just quietly gets English.
#
# So: re-extract the template from POTFILES and require (a) that it matches the
# committed one, which is what fails when a source grows a string, and (b) that
# msgcmp finds no untranslated or missing entry in any catalogue.
set -o pipefail
SRC=${1:?source dir}
cd "$SRC" || exit 1

tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

pot=$tmp/re-extracted.pot
xgettext --from-code=UTF-8 --add-comments --keyword=_ --keyword=N_ \
         --keyword=ngettext:1,2 \
         --package-name=reac-stageboxes --package-version=0.1.0 \
         --msgid-bugs-address=linuxnow@gmail.com --copyright-holder="Pau Aliagas" \
         -o "$pot" --directory=. $(grep -v desktop po/POTFILES) || exit 1
xgettext --from-code=UTF-8 -L Desktop -j -o "$pot" \
         --directory=. data/org.freereac.Stageboxes.desktop.in || exit 1

# A POSITIVE CONTROL FIRST. An extraction that silently produced nothing would
# make every comparison below pass, and an empty search looks exactly like a
# clean one. Require the template to carry a string we know is there.
if ! grep -q 'Phantom power' "$pot"; then
	echo "FAIL: the extraction produced no known string — the probe is broken," >&2
	echo "      not the catalogues. Check POTFILES and the xgettext keywords." >&2
	exit 1
fi
n=$(grep -c '^msgid ' "$pot")
if [ "$n" -lt 40 ]; then
	echo "FAIL: only $n msgids extracted; the sources carry far more." >&2
	exit 1
fi
echo "extracted $n msgids (control string present)"

# (a) Did a source grow a string the committed template has not got?
missing=0
while IFS= read -r line; do
	case "$line" in
		'msgid ""') continue ;;
		'msgid '*) ;;
		*) continue ;;
	esac
	if ! grep -Fqx "$line" po/reac-stageboxes.pot; then
		echo "FAIL: not in po/reac-stageboxes.pot: $line" >&2
		missing=1
	fi
done < "$pot"
if [ "$missing" -ne 0 ]; then
	echo "      Regenerate the template and translate the new strings." >&2
	exit 1
fi

# (b) Is every msgid translated, in every catalogue we ship?
rc=0
while read -r lang; do
	[ -n "$lang" ] || continue
	if ! msgcmp "po/$lang.po" po/reac-stageboxes.pot 2>&1; then
		echo "FAIL: po/$lang.po does not translate every msgid." >&2
		rc=1
	fi
	if ! msgfmt --check -o /dev/null "po/$lang.po"; then
		echo "FAIL: po/$lang.po does not compile." >&2
		rc=1
	fi
done < po/LINGUAS
[ "$rc" -eq 0 ] && echo "catalogues complete: $(tr '\n' ' ' < po/LINGUAS)"
exit $rc
