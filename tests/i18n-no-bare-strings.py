#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>
"""No user-visible string literal escapes the catalogue.

The companion test compares the re-extracted template against the committed one,
and it CANNOT SEE THIS: a string that was never wrapped in `_()` is absent from
both, so the two agree perfectly and the check passes while the operator reads
English. That blind spot was found by sabotage — a `printf("...")` added to the
list output went green through the whole i18n suite.

So this one reads the sources instead of the catalogues. For every call that
puts text in front of a person, it requires the argument to be `_(`, `N_(`,
`ngettext(` or a variable. A literal with no letters in it (`""`, `"\\n"`, an em
dash) is text in no language and is allowed; anything else must be marked
`/* untranslated: why */` on the same line, so an exception is a decision
someone wrote down rather than an oversight.
"""
import re
import sys
from pathlib import Path

# The calls whose first text argument a person reads. Icon names, CSS classes
# and property names are deliberately NOT here: they are identifiers, and
# translating one would break the lookup.
CALLS = [
    "printf", "fprintf", "g_print", "g_printerr",
    "g_set_error_literal", "g_set_error",
    "g_strdup_printf", "g_string_append_printf",
    "adw_preferences_row_set_title",
    "adw_action_row_set_subtitle",
    "adw_expander_row_set_subtitle",
    "adw_preferences_group_set_title",
    "adw_status_page_set_title",
    "adw_status_page_set_description",
    "adw_window_title_set_title",
    "adw_window_title_set_subtitle",
    "adw_banner_set_title",
    "gtk_label_set_text",
]

# A literal in a text position: the call, then arguments that are not literals,
# then a literal. `fprintf(stderr, "…")` and `printf("…")` both land here.
PATTERN = re.compile(
    r"\b(" + "|".join(CALLS) + r")\s*\(\s*"
    r"(?:[A-Za-z_][\w>.\-\[\]]*\s*,\s*)*"        # stderr, self->banner, r->note, …
    r"(\"(?:[^\"\\]|\\.)*\")"
)

HAS_LETTER = re.compile(r"[A-Za-z]")

# printf conversions, stripped before the letter test: the `s` of "%s" is not a
# word. A format string that is nothing BUT placeholders and punctuation —
# " · %s", "%s\n" — carries no language and needs no catalogue entry.
CONVERSION = re.compile(r"%[-+ #0']*[0-9*]*(?:\.[0-9*]+)?[hlLqjzt]*"
                        r"[diouxXeEfFgGaAcspn%]")
ESCAPE = re.compile(r"\\.")


def is_wordless(body):
    """True when the literal is punctuation, escapes and format placeholders."""
    stripped = ESCAPE.sub("", CONVERSION.sub("", body))
    return not HAS_LETTER.search(stripped)


def check(path):
    bad = []
    for n, line in enumerate(path.read_text(encoding="utf8").splitlines(), 1):
        if "/* untranslated:" in line:
            continue
        for call, literal in PATTERN.findall(line):
            body = literal[1:-1]
            if is_wordless(body):
                continue
            bad.append((n, call, literal, line.strip()))
    return bad


def main(argv):
    src = Path(argv[1]) if len(argv) > 1 else Path("src")
    files = sorted(src.glob("*.c"))
    if not files:
        print(f"FAIL: no C sources under {src} — the scan is broken, not clean.",
              file=sys.stderr)
        return 1

    # A POSITIVE CONTROL. An expression that matches nothing reports a clean
    # codebase and a broken one identically, so prove the pattern can still fire
    # before believing it found nothing.
    probe = 'printf("Phantom power is on\\n");'
    hits = PATTERN.findall(probe)
    if not hits or is_wordless(hits[0][1][1:-1]):
        print("FAIL: the scan does not flag a known bare string — it is broken, "
              "not the sources.", file=sys.stderr)
        return 1
    # And the other half of the control: a wordless format must NOT be flagged,
    # or every printf in the tree would be a false alarm and the test would be
    # turned off within a day.
    if not is_wordless(" \u00b7 %s") or not is_wordless("%s\\n"):
        print("FAIL: the scan flags a pure format string — it would cry wolf.",
              file=sys.stderr)
        return 1

    failures = 0
    for f in files:
        for n, call, literal, line in check(f):
            print(f"FAIL {f}:{n}: {call}() is given an untranslated literal "
                  f"{literal}\n     {line}", file=sys.stderr)
            failures += 1

    if failures:
        print(f"\n{failures} user-visible string(s) outside the catalogue. Wrap each "
              f"in _(), or mark the line /* untranslated: why */.", file=sys.stderr)
        return 1

    print(f"no bare user-visible strings in {len(files)} source file(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
