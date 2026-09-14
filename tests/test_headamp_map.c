// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Pau Aliagas <linuxnow@gmail.com>

/* The param-name mapping and the dB/wire-value conversion.
 *
 * THE EXPECTED STRINGS ARE WRITTEN OUT LONGHAND, never composed by calling the
 * formatter under test. A key this application misspells is not an error on a
 * live desk: reac-pw's `reac_headamp_prop_parse` SKIPS a cell whose key it does
 * not recognise and `pw_node_set_param` still returns success, so a typo here
 * would move no audio and report nothing. The literals below are the oracle,
 * copied from reac-pw's src/reac_headamp_prop.h; a test that built its
 * expectation with the same function it is checking would agree with any typo.
 *
 * The dB figures are likewise the published curve (libreac
 * <reac/reac_ctrlblk.h>, measured over all 56 steps on an S-0808), not this
 * module's own arithmetic re-run. */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "rs-headamp.h"

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

#define CHECK_STR(got, want) do { \
	const char *g_ = (got), *w_ = (want); \
	if (!g_ || strcmp(g_, w_) != 0) { \
		fprintf(stderr, "FAIL %s:%d: got \"%s\", want \"%s\"\n", \
		        __FILE__, __LINE__, g_ ? g_ : "(null)", w_); \
		failures++; \
	} \
} while (0)

#define CHECK_INT(got, want) do { \
	int g_ = (got), w_ = (want); \
	if (g_ != w_) { \
		fprintf(stderr, "FAIL %s:%d: got %d, want %d\n", \
		        __FILE__, __LINE__, g_, w_); \
		failures++; \
	} \
} while (0)

static void test_param_names(void)
{
	/* The three spellings reac-pw's param_name_to_id accepts, and nothing else. */
	CHECK_STR(rs_headamp_param_name(RS_HEADAMP_PHANTOM), "phantom");
	CHECK_STR(rs_headamp_param_name(RS_HEADAMP_PAD),     "pad");
	CHECK_STR(rs_headamp_param_name(RS_HEADAMP_SENS),    "sens");

	/* The ids are the wire's, not an enumeration of our own: the numbers that
	 * appear in a head-amp record's PARAM byte and in an asserted cell. */
	CHECK_INT(RS_HEADAMP_PHANTOM, 0x00);
	CHECK_INT(RS_HEADAMP_PAD,     0x01);
	CHECK_INT(RS_HEADAMP_SENS,    0x02);

	CHECK_INT(rs_headamp_param_from_name("phantom"), RS_HEADAMP_PHANTOM);
	CHECK_INT(rs_headamp_param_from_name("pad"),     RS_HEADAMP_PAD);
	CHECK_INT(rs_headamp_param_from_name("sens"),    RS_HEADAMP_SENS);
	CHECK_INT(rs_headamp_param_from_name("gain"),    -1);
	CHECK_INT(rs_headamp_param_from_name("Phantom"), -1);   /* case matters */
	CHECK_INT(rs_headamp_param_from_name(""),        -1);
	CHECK_INT(rs_headamp_param_from_name(NULL),      -1);
}

static void test_wire_channel(void)
{
	/* CH = base + (box_input - 1). An S-1608 straps at 32, so its input 1 is
	 * wire channel 32 and its input 16 is 47 — the case a table bounded by the
	 * 40-slot audio space silently dropped. */
	CHECK_INT(rs_headamp_wire_ch(32, 1),  32);
	CHECK_INT(rs_headamp_wire_ch(32, 16), 47);
	/* An S-0808 straps at 0. */
	CHECK_INT(rs_headamp_wire_ch(0, 1), 0);
	CHECK_INT(rs_headamp_wire_ch(0, 8), 7);

	/* No base is NOT base 0: it is no wire address at all. */
	CHECK_INT(rs_headamp_wire_ch(-1, 1), -1);
	/* Past the 0x2f ceiling, and below input 1. */
	CHECK_INT(rs_headamp_wire_ch(32, 17), -1);
	CHECK_INT(rs_headamp_wire_ch(32, 0),  -1);
}

static void test_keys(void)
{
	char key[RS_HEADAMP_KEY_CAP];

	CHECK_INT(rs_headamp_param_key(key, sizeof key, 34, RS_HEADAMP_PHANTOM), 0);
	CHECK_STR(key, "reac.headamp.34.phantom");

	CHECK_INT(rs_headamp_param_key(key, sizeof key, 34, RS_HEADAMP_PAD), 0);
	CHECK_STR(key, "reac.headamp.34.pad");

	CHECK_INT(rs_headamp_param_key(key, sizeof key, 47, RS_HEADAMP_SENS), 0);
	CHECK_STR(key, "reac.headamp.47.sens");

	/* Decimal, never hex — the daemon's parse is strtol base 10, so "0x20"
	 * would read as 0 and address a different box. */
	CHECK_INT(rs_headamp_param_key(key, sizeof key, 32, RS_HEADAMP_SENS), 0);
	CHECK_STR(key, "reac.headamp.32.sens");

	CHECK_INT(rs_headamp_param_key(key, sizeof key, 0, RS_HEADAMP_PHANTOM), 0);
	CHECK_STR(key, "reac.headamp.0.phantom");

	/* Refusals: out of the wire space, and a buffer that would truncate. A
	 * truncated key is worse than no key, so it comes back empty. */
	CHECK_INT(rs_headamp_param_key(key, sizeof key, 48, RS_HEADAMP_PHANTOM), -1);
	CHECK_INT(rs_headamp_param_key(key, sizeof key, -1, RS_HEADAMP_PHANTOM), -1);
	char small[8];
	CHECK_INT(rs_headamp_param_key(small, sizeof small, 34, RS_HEADAMP_SENS), -1);
	CHECK_STR(small, "");
}

static void test_sens_curve(void)
{
	const int smax = RS_HEADAMP_SENS_MAX_DEFAULT;
	CHECK_INT(smax, 0x37);
	CHECK_INT(smax, 55);

	/* sensitivity_dBu = -10 - value + (pad ? 20 : 0) — the endpoints the
	 * published curve names: -10 dBu at step 0, -65 dBu at step 0x37. */
	CHECK_INT(rs_headamp_sens_dbu(0,    0, smax), -10);
	CHECK_INT(rs_headamp_sens_dbu(0x37, 0, smax), -65);
	CHECK_INT(rs_headamp_sens_dbu(1,    0, smax), -11);
	CHECK_INT(rs_headamp_sens_dbu(32,   0, smax), -42);

	/* The pad shifts the WHOLE travel up by 20 dB; it does not compress it. */
	CHECK_INT(rs_headamp_sens_dbu(0,    1, smax),  10);
	CHECK_INT(rs_headamp_sens_dbu(0x37, 1, smax), -45);
	CHECK_INT(rs_headamp_sens_dbu(32,   1, smax), -22);
	CHECK_INT(rs_headamp_sens_dbu(0, 1, smax) - rs_headamp_sens_dbu(0, 0, smax),
	          RS_HEADAMP_PAD_DB);

	/* The step is a whole decibel in both directions. 32 -> 52 is the 20 dB
	 * move the rig proof uses. */
	CHECK_INT(rs_headamp_sens_dbu(32, 0, smax) - rs_headamp_sens_dbu(52, 0, smax), 20);

	/* Clamping, the way libreac clamps: saturate, never wrap. */
	CHECK_INT(rs_headamp_sens_dbu(200, 0, smax), -65);
	CHECK_INT(rs_headamp_sens_dbu(-5,  0, smax), -10);

	/* The inverse, and a full round trip over every step, pad off and on. */
	CHECK_INT(rs_headamp_sens_value(-10, 0, smax), 0);
	CHECK_INT(rs_headamp_sens_value(-65, 0, smax), 0x37);
	CHECK_INT(rs_headamp_sens_value(-42, 0, smax), 32);
	CHECK_INT(rs_headamp_sens_value(-22, 1, smax), 32);
	for (int pad = 0; pad <= 1; pad++) {
		for (int v = 0; v <= smax; v++) {
			int db = rs_headamp_sens_dbu(v, pad, smax);
			CHECK_INT(rs_headamp_sens_value(db, pad, smax), v);
		}
	}
	/* Out of travel in each direction lands on an end, never elsewhere. */
	CHECK_INT(rs_headamp_sens_value(0,    0, smax), 0);
	CHECK_INT(rs_headamp_sens_value(-999, 0, smax), 0x37);

	/* A DIFFERENT PUBLISHED CEILING IS OBEYED, so a model with another travel
	 * needs no new client. */
	CHECK_INT(rs_headamp_sens_dbu(40, 0, 40), -50);
	CHECK_INT(rs_headamp_sens_dbu(41, 0, 40), -50);     /* clamped at the ceiling */
	CHECK_INT(rs_headamp_sens_value(-60, 0, 40), 40);

	/* The ceiling comes from the node when the node publishes one; absence
	 * falls back to the model matrix's 0x37, and nothing else does. */
	CHECK_INT(rs_headamp_sens_max_parse("40"),   40);
	CHECK_INT(rs_headamp_sens_max_parse("55"),   55);
	CHECK_INT(rs_headamp_sens_max_parse(NULL),   RS_HEADAMP_SENS_MAX_DEFAULT);
	CHECK_INT(rs_headamp_sens_max_parse(""),     RS_HEADAMP_SENS_MAX_DEFAULT);
	CHECK_INT(rs_headamp_sens_max_parse("many"), RS_HEADAMP_SENS_MAX_DEFAULT);
	CHECK_INT(rs_headamp_sens_max_parse("0"),    RS_HEADAMP_SENS_MAX_DEFAULT);
}

static void test_base_parse(void)
{
	CHECK_INT(rs_headamp_base_parse("32"), 32);
	CHECK_INT(rs_headamp_base_parse("0"),  0);
	/* "none" is what reac-pw writes when no box is known. It must NOT read as
	 * base 0 — that addresses an S-1608's preamps 32 slots low and silently. */
	CHECK_INT(rs_headamp_base_parse("none"), -1);
	CHECK_INT(rs_headamp_base_parse(NULL),   -1);
	CHECK_INT(rs_headamp_base_parse(""),     -1);
	CHECK_INT(rs_headamp_base_parse("32x"),  -1);
	CHECK_INT(rs_headamp_base_parse("48"),   -1);   /* past the wire space */
}

static void test_asserted_parse(void)
{
	struct rs_headamp_cell cells[8];

	/* The published shape: ch:param=value, comma separated. */
	int n = rs_headamp_asserted_parse("34:0=1,34:2=52,35:0=0", cells, 8);
	CHECK_INT(n, 3);
	CHECK_INT(cells[0].ch, 34); CHECK_INT(cells[0].param, 0); CHECK_INT(cells[0].value, 1);
	CHECK_INT(cells[1].ch, 34); CHECK_INT(cells[1].param, 2); CHECK_INT(cells[1].value, 52);
	CHECK_INT(cells[2].ch, 35); CHECK_INT(cells[2].param, 0); CHECK_INT(cells[2].value, 0);

	CHECK_INT(rs_headamp_cell_lookup(cells, n, 34, RS_HEADAMP_PHANTOM), 1);
	CHECK_INT(rs_headamp_cell_lookup(cells, n, 34, RS_HEADAMP_SENS),    52);
	CHECK_INT(rs_headamp_cell_lookup(cells, n, 35, RS_HEADAMP_PHANTOM), 0);
	/* An UNSET cell is -1, never 0. A zero read out of an absent key is a
	 * measurement of nothing that looks like a measurement. */
	CHECK_INT(rs_headamp_cell_lookup(cells, n, 35, RS_HEADAMP_SENS), -1);
	CHECK_INT(rs_headamp_cell_lookup(cells, n, 99, RS_HEADAMP_PAD),  -1);

	/* Empty means "this daemon is asserting nothing" — a real answer. */
	CHECK_INT(rs_headamp_asserted_parse("", cells, 8), 0);
	/* NULL means the property was absent, which is a different fact. */
	CHECK_INT(rs_headamp_asserted_parse(NULL, cells, 8), -1);

	/* One malformed cell never truncates the rest. */
	n = rs_headamp_asserted_parse("34:0=1,rubbish,35:2=7", cells, 8);
	CHECK_INT(n, 2);
	CHECK_INT(cells[1].ch, 35);
	CHECK_INT(cells[1].value, 7);

	/* Out of range is dropped rather than stored. */
	CHECK_INT(rs_headamp_asserted_parse("48:0=1", cells, 8), 0);
	CHECK_INT(rs_headamp_asserted_parse("34:9=1", cells, 8), 0);
	CHECK_INT(rs_headamp_asserted_parse("34:0=999", cells, 8), 0);

	/* Never past `max`. */
	CHECK_INT(rs_headamp_asserted_parse("1:0=1,2:0=1,3:0=1", cells, 2), 2);
}

static void test_caps(void)
{
	CHECK_INT(rs_headamp_caps_has("phantom,pad,sens", "phantom"), 1);
	CHECK_INT(rs_headamp_caps_has("phantom,pad,sens", "pad"),     1);
	CHECK_INT(rs_headamp_caps_has("phantom,pad,sens", "sens"),    1);
	CHECK_INT(rs_headamp_caps_has("phantom,pad,sens", "gain"),    0);
	/* A prefix is not a token: "pad" must not match inside "padx". */
	CHECK_INT(rs_headamp_caps_has("padx", "pad"), 0);
	CHECK_INT(rs_headamp_caps_has("phantom", "phantom,pad"), 0);
	CHECK_INT(rs_headamp_caps_has("", "pad"),   0);
	CHECK_INT(rs_headamp_caps_has(NULL, "pad"), 0);
	/* A box that publishes only part of the trio gets only those controls. */
	CHECK_INT(rs_headamp_caps_has("phantom,sens", "pad"),  0);
	CHECK_INT(rs_headamp_caps_has("phantom,sens", "sens"), 1);
}

static void test_availability(void)
{
	/* Everything present and established: the controls are live. */
	CHECK_INT(rs_headamp_availability(1, "established", 16, "32", "none", 1),
	          RS_AVAIL_OK);

	/* We are the segment's SLAVE: reac-pw builds no sink node in that role, so
	 * there is no node to write to at all. */
	CHECK_INT(rs_headamp_availability(0, "established", 16, "32", "none", 1),
	          RS_AVAIL_NO_CONTROL_DOOR);

	/* The box is strapped to master mode — not an error, the contract of that
	 * mode, and it outranks everything else because it is what the operator
	 * must act on. */
	CHECK_INT(rs_headamp_availability(1, "established", 0, "none", "box-master", 1),
	          RS_AVAIL_BOX_MASTER);

	CHECK_INT(rs_headamp_availability(1, "established", 0, "32", "none", 1),
	          RS_AVAIL_NO_BOX);
	CHECK_INT(rs_headamp_availability(1, "established", 16, "none", "none", 1),
	          RS_AVAIL_NO_BASE);
	CHECK_INT(rs_headamp_availability(1, "probing", 16, "32", "none", 1),
	          RS_AVAIL_NOT_ESTABLISHED);
	CHECK_INT(rs_headamp_availability(1, "dropped", 16, "32", "none", 1),
	          RS_AVAIL_NOT_ESTABLISHED);

	/* A daemon that publishes no reac.headamp.asserted. The door is there, but
	 * a change could never be confirmed, so the rows are read-only. */
	CHECK_INT(rs_headamp_availability(1, "established", 16, "32", NULL, 0),
	          RS_AVAIL_NO_READBACK);
	/* Absent `refused` alone does not block the row — only the readback does. */
	CHECK_INT(rs_headamp_availability(1, "established", 16, "32", NULL, 1),
	          RS_AVAIL_OK);

	CHECK_STR(rs_avail_id(RS_AVAIL_BOX_MASTER), "box-master");
	CHECK_STR(rs_avail_id(RS_AVAIL_NO_READBACK), "no-readback");
	CHECK_STR(rs_avail_id(RS_AVAIL_OK), "ok");
}

int main(void)
{
	test_param_names();
	test_wire_channel();
	test_keys();
	test_sens_curve();
	test_base_parse();
	test_asserted_parse();
	test_caps();
	test_availability();

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("headamp map: ok\n");
	return 0;
}
