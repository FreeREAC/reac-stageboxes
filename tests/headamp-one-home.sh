#!/bin/sh
# The head-amp vocabulary has one home: rs-headamp.h includes <reac/reac_ctrlblk.h>
# and defines no RS_HEADAMP_* id, channel bound or curve figure as a literal number.
# Usage: headamp-one-home.sh <path to rs-headamp.h>
set -eu
hdr=${1:?rs-headamp.h}

check() {
	f=$1
	rc=0
	grep -q '^#include <reac/reac_ctrlblk.h>' "$f" || {
		echo "$f: does not include <reac/reac_ctrlblk.h>, the one home of the vocabulary"; rc=1; }
	# a literal number on the right of an RS_HEADAMP_ define (0x37, -10, 48, 20...), the
	# key-buffer capacity excepted: that is this module's own
	if grep -nE '^#define RS_HEADAMP_(PHANTOM|PAD|SENS|NPARAMS|MAX_CH|SENS_MAX_DEFAULT|SENS_REF_DBU|SENS_STEP_DB|PAD_DB)[[:space:]]+\(?-?(0x[0-9a-fA-F]+|[0-9]+)\)?([[:space:]]|$)' "$f"; then
		echo "$f: spells a head-amp figure as a number; libreac's <reac/reac_ctrlblk.h> is where it lives"; rc=1; fi
	# an enumerator assigned a number instead of libreac's name
	if grep -nE '(^|[[:space:]{,])RS_HEADAMP_(PHANTOM|PAD|SENS)[[:space:]]*=[[:space:]]*-?(0x[0-9a-fA-F]+|[0-9]+)' "$f"; then
		echo "$f: numbers a parameter id itself; the ids are REAC_HEADAMP_*"; rc=1; fi
	return $rc
}

# the detector must refuse each planted copy, or a clean tree proves nothing
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
printf '#include <reac/reac_ctrlblk.h>\n#define RS_HEADAMP_SENS_MAX_DEFAULT 0x37\n' > "$tmp/a.h"
printf '#include <reac/reac_ctrlblk.h>\nenum x { RS_HEADAMP_PAD = 0x01 };\n' > "$tmp/b.h"
printf '#define RS_HEADAMP_MAX_CH REAC_HEADAMP_MAX_CH\n' > "$tmp/c.h"
printf '#include <reac/reac_ctrlblk.h>\n#define RS_HEADAMP_MAX_CH REAC_HEADAMP_MAX_CH\n#define RS_HEADAMP_KEY_CAP 48\n' > "$tmp/good.h"
for p in a b c; do
	if check "$tmp/$p.h" >/dev/null 2>&1; then echo "SELF-CHECK: the planted copy $p.h passed; the detector is broken"; exit 1; fi
done
check "$tmp/good.h" >/dev/null 2>&1 || { echo "SELF-CHECK: the planted good header was refused; the detector is broken"; exit 1; }

check "$hdr" && echo "OK: $hdr reads the head-amp vocabulary from libreac and copies none of it"
