/* Unit tests for masp's number-prefix parsing.
 *
 * masp accepts two notations for non-decimal literals:
 *
 *   1. Modern (`change_base2`):  0b1010, 0q17, 0h1F, 0d10, 0aFoo
 *      A digit-zero followed by a base-character letter.  Default
 *      lexer when masp runs in its native mode.
 *
 *   2. GASP-compat (`change_base`):  B'1010', Q'17', H'1F', D'10',
 *      A'Foo'.  Base-character letter followed by a quoted body.
 *      Used when the input was written for GNU gasp.
 *
 * Both routines share two leaf helpers:
 *   - is_base(char)        -> base, or 0 if not a base-prefix char
 *   - sb_strtol(idx, sb, base, *out) -> end-index, value via out
 *
 * The functions are file-static in src/masp.c, so we include the
 * source file directly (with main renamed) to reach them — same
 * trick the existing test_masp_cli.c uses.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Avoid colliding with masp's main when masp.c is #included. */
#define main masp_program_main
#include "../../src/masp.c"
#undef main

#define CHECK(cond) do { \
    if (!(cond)) { \
      fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      return 1; \
    } \
  } while (0)

#define CHECK_EQ_INT(a, b) do { \
    long _a = (long)(a), _b = (long)(b); \
    if (_a != _b) { \
      fprintf(stderr, "  FAIL %s:%d: %s == %s  (got %ld vs %ld)\n", \
              __FILE__, __LINE__, #a, #b, _a, _b); \
      return 1; \
    } \
  } while (0)

/* Build an sb from a C-string for use as a test input. */
static void make_sb(sb *s, const char *text) {
  sb_new(s);
  sb_add_string(s, text);
}

/* Snapshot an sb as a NUL-terminated C string for easy comparison. */
static const char *sb_as_cstr(sb *s) {
  /* sb_terminate appends NUL but does not bump len, so the buffer can
     be re-read as a C string.  */
  return sb_terminate(s);
}

#define CHECK_SB_EQ(sbp, expected) do { \
    const char *_got = sb_as_cstr(sbp); \
    if (strcmp(_got, (expected)) != 0) { \
      fprintf(stderr, "  FAIL %s:%d: sb == %s  (got %s)\n", \
              __FILE__, __LINE__, (expected), _got); \
      return 1; \
    } \
  } while (0)

/* --- is_base -------------------------------------------------------- */

static int test_is_base_known_prefixes(void) {
  CHECK_EQ_INT(is_base('b'),   2);
  CHECK_EQ_INT(is_base('B'),   2);
  CHECK_EQ_INT(is_base('q'),   8);
  CHECK_EQ_INT(is_base('Q'),   8);
  CHECK_EQ_INT(is_base('h'),  16);
  CHECK_EQ_INT(is_base('H'),  16);
  CHECK_EQ_INT(is_base('d'),  10);
  CHECK_EQ_INT(is_base('D'),  10);
  /* 'a'/'A' is the "ASCII literal" prefix; documented internally as
     base 256 (placeholder sentinel).  */
  CHECK_EQ_INT(is_base('a'), 256);
  CHECK_EQ_INT(is_base('A'), 256);
  return 0;
}

static int test_is_base_unknown_returns_zero(void) {
  CHECK_EQ_INT(is_base('x'),   0);
  CHECK_EQ_INT(is_base('0'),   0);
  CHECK_EQ_INT(is_base(' '),   0);
  CHECK_EQ_INT(is_base('c'),   0);
  CHECK_EQ_INT(is_base('z'),   0);
  return 0;
}

/* --- sb_strtol ------------------------------------------------------ */

static int test_sb_strtol_decimal(void) {
  sb s; int val = -1;
  make_sb(&s, "12345");
  int end = sb_strtol(0, &s, 10, &val);
  CHECK_EQ_INT(val, 12345);
  CHECK_EQ_INT(end, 5);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_binary(void) {
  sb s; int val = -1;
  make_sb(&s, "1010");
  int end = sb_strtol(0, &s, 2, &val);
  CHECK_EQ_INT(val, 10);
  CHECK_EQ_INT(end, 4);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_octal(void) {
  sb s; int val = -1;
  make_sb(&s, "17");
  int end = sb_strtol(0, &s, 8, &val);
  CHECK_EQ_INT(val, 15);
  CHECK_EQ_INT(end, 2);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_hex_lower_and_upper(void) {
  sb s; int val;
  make_sb(&s, "ff");
  CHECK_EQ_INT(sb_strtol(0, &s, 16, &val), 2);
  CHECK_EQ_INT(val, 255);
  sb_kill(&s);

  make_sb(&s, "FF");
  CHECK_EQ_INT(sb_strtol(0, &s, 16, &val), 2);
  CHECK_EQ_INT(val, 255);
  sb_kill(&s);

  make_sb(&s, "DeAdBeEf");
  CHECK_EQ_INT(sb_strtol(0, &s, 16, &val), 8);
  CHECK_EQ_INT((unsigned)val, 0xDEADBEEFu);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_stops_at_first_non_digit(void) {
  sb s; int val;
  make_sb(&s, "12xy");
  CHECK_EQ_INT(sb_strtol(0, &s, 10, &val), 2);
  CHECK_EQ_INT(val, 12);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_stops_at_digit_outside_base(void) {
  /* '2' is invalid in base 2; strtol should stop there.  */
  sb s; int val;
  make_sb(&s, "112");
  CHECK_EQ_INT(sb_strtol(0, &s, 2, &val), 2);
  CHECK_EQ_INT(val, 3);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_skips_leading_whitespace(void) {
  sb s; int val;
  make_sb(&s, "   42rest");
  CHECK_EQ_INT(sb_strtol(0, &s, 10, &val), 5);
  CHECK_EQ_INT(val, 42);
  sb_kill(&s);
  return 0;
}

static int test_sb_strtol_starting_at_offset(void) {
  sb s; int val;
  make_sb(&s, "prefix99tail");
  /* Caller is responsible for moving past non-digit content; here we
     just pretend they did and pass idx=6.  */
  CHECK_EQ_INT(sb_strtol(6, &s, 10, &val), 8);
  CHECK_EQ_INT(val, 99);
  sb_kill(&s);
  return 0;
}

/* --- change_base2 (modern 0b/0q/0h/0d prefix form) ----------------- */

static int run_change_base2(const char *input, const char *expected) {
  sb in, out;
  make_sb(&in, input);
  sb_new(&out);
  change_base2(0, &in, &out);
  /* sb_as_cstr() mutates by NUL-terminating; use a local check. */
  const char *got = sb_terminate(&out);
  int ok = strcmp(got, expected) == 0;
  if (!ok)
    fprintf(stderr, "  change_base2(%s): expected %s, got %s\n",
            input, expected, got);
  sb_kill(&in);
  sb_kill(&out);
  return ok ? 0 : 1;
}

static int test_change_base2_binary(void) {
  return run_change_base2("0b1010", "10");
}

static int test_change_base2_octal(void) {
  return run_change_base2("0q17", "15");
}

static int test_change_base2_hex(void) {
  return run_change_base2("0hFF", "255");
}

static int test_change_base2_decimal_passthrough(void) {
  return run_change_base2("0d42", "42");
}

static int test_change_base2_uppercase_prefix(void) {
  /* 0H is the same as 0h */
  return run_change_base2("0HFF", "255");
}

static int test_change_base2_inline_in_expression(void) {
  /* Spaces and operators around the number should survive.  */
  return run_change_base2("mov 0hFF,r0", "mov 255,r0");
}

static int test_change_base2_bare_zero_not_a_prefix(void) {
  /* '0' followed by space/comma/comment must NOT trigger rewriting. */
  return run_change_base2("0 ",   "0 ")
      || run_change_base2("0,r1", "0,r1");
}

static int test_change_base2_leaves_unknown_letters_alone(void) {
  /* '0z' is not a base prefix; the leading '0' is treated as a digit
     and chewed by the default-radix path, then 'z' falls through as
     part of the surrounding identifier.  */
  return run_change_base2("0z foo", "0z foo");
}

/* --- change_base (GASP-style B'...' / H'...' quoted form) ----------
 *
 * KNOWN BUG (pinned by these tests, NOT fixed here): change_base does
 * not consume the closing apostrophe of `B'1010'`.  sb_strtol stops at
 * the closing `'`, then the outer loop falls into the quote-handler
 * branch, which echoes the `'` (and anything until the next `'`) into
 * the output.  Net effect: input `B'1010'` -> output `10'`, and
 * `H'1F',r0` -> `31',r0` (the comma gets pulled inside the unmatched
 * quote handler).  These tests pin the current behaviour so a future
 * fix shows up as a test diff.  ps2gl runs in default masp_syntax
 * mode, which dispatches to change_base2 (no apostrophes), so the bug
 * is dormant for the present pipeline.
 */

static int run_change_base(const char *input, const char *expected) {
  sb in, out;
  make_sb(&in, input);
  sb_new(&out);
  change_base(0, &in, &out);
  const char *got = sb_terminate(&out);
  int ok = strcmp(got, expected) == 0;
  if (!ok)
    fprintf(stderr, "  change_base(%s): expected %s, got %s\n",
            input, expected, got);
  sb_kill(&in);
  sb_kill(&out);
  return ok ? 0 : 1;
}

/* All of the *_gasp tests below assert the trailing-apostrophe bug as
 * documented above.  If change_base is ever fixed to consume the
 * closing quote, expected values become "10" / "31" / etc.  */

static int test_change_base_binary_gasp(void) {
  return run_change_base("B'1010'", "10'");
}

static int test_change_base_hex_gasp(void) {
  return run_change_base("H'1F'", "31'");
}

static int test_change_base_octal_gasp(void) {
  return run_change_base("Q'17'", "15'");
}

static int test_change_base_decimal_gasp(void) {
  return run_change_base("D'99'", "99'");
}

static int test_change_base_lowercase_prefix(void) {
  return run_change_base("h'10'", "16'");
}

static int test_change_base_passes_through_plain_digits(void) {
  /* In default radix 10, "42" -> "42" */
  return run_change_base("42", "42");
}

static int test_change_base_passes_through_identifiers(void) {
  return run_change_base("foo_bar", "foo_bar");
}

/* --- driver -------------------------------------------------------- */

struct test_case { const char *name; int (*fn)(void); };

static const struct test_case cases[] = {
  /* is_base */
  { "is_base_known_prefixes",                 test_is_base_known_prefixes },
  { "is_base_unknown_returns_zero",           test_is_base_unknown_returns_zero },

  /* sb_strtol */
  { "sb_strtol_decimal",                      test_sb_strtol_decimal },
  { "sb_strtol_binary",                       test_sb_strtol_binary },
  { "sb_strtol_octal",                        test_sb_strtol_octal },
  { "sb_strtol_hex_lower_and_upper",          test_sb_strtol_hex_lower_and_upper },
  { "sb_strtol_stops_at_first_non_digit",     test_sb_strtol_stops_at_first_non_digit },
  { "sb_strtol_stops_at_digit_outside_base",  test_sb_strtol_stops_at_digit_outside_base },
  { "sb_strtol_skips_leading_whitespace",     test_sb_strtol_skips_leading_whitespace },
  { "sb_strtol_starting_at_offset",           test_sb_strtol_starting_at_offset },

  /* change_base2 (modern 0b/0q/0h/0d) */
  { "change_base2_binary",                    test_change_base2_binary },
  { "change_base2_octal",                     test_change_base2_octal },
  { "change_base2_hex",                       test_change_base2_hex },
  { "change_base2_decimal_passthrough",       test_change_base2_decimal_passthrough },
  { "change_base2_uppercase_prefix",          test_change_base2_uppercase_prefix },
  { "change_base2_inline_in_expression",      test_change_base2_inline_in_expression },
  { "change_base2_bare_zero_not_a_prefix",    test_change_base2_bare_zero_not_a_prefix },
  { "change_base2_leaves_unknown_letters_alone", test_change_base2_leaves_unknown_letters_alone },

  /* change_base (GASP B'…' / H'…') */
  { "change_base_binary_gasp",                test_change_base_binary_gasp },
  { "change_base_hex_gasp",                   test_change_base_hex_gasp },
  { "change_base_octal_gasp",                 test_change_base_octal_gasp },
  { "change_base_decimal_gasp",               test_change_base_decimal_gasp },
  { "change_base_lowercase_prefix",           test_change_base_lowercase_prefix },
  { "change_base_passes_through_plain_digits", test_change_base_passes_through_plain_digits },
  { "change_base_passes_through_identifiers", test_change_base_passes_through_identifiers },
};

int main(void) {
  int n = (int)(sizeof cases / sizeof cases[0]);
  int failed = 0;
  for (int i = 0; i < n; i++) {
    int rc = cases[i].fn();
    if (rc != 0) {
      fprintf(stderr, "FAIL  %s\n", cases[i].name);
      failed++;
    } else {
      fprintf(stdout, "ok    %s\n", cases[i].name);
    }
  }
  fprintf(stdout, "\n%d/%d tests passed\n", n - failed, n);
  return failed == 0 ? 0 : 1;
}
