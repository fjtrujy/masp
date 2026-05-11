/* Unit tests for src/sb.c — the string-buffer module that masp uses
 * throughout the assembler.  sb.c only depends on libc, so we link it
 * directly into the test binary.
 *
 * Convention: each TEST_* function returns 0 on success, non-zero on
 * failure.  main() runs them in order and prints a one-line summary.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sb.h"

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

#define CHECK_EQ_MEM(a, b, n) do { \
    if (memcmp((a), (b), (n)) != 0) { \
      fprintf(stderr, "  FAIL %s:%d: memcmp(%s, %s, %d) != 0\n", \
              __FILE__, __LINE__, #a, #b, (int)(n)); \
      return 1; \
    } \
  } while (0)

/* --- basic lifecycle ------------------------------------------------ */

static int test_new_and_kill(void) {
  sb s;
  sb_new(&s);
  CHECK_EQ_INT(s.len, 0);
  CHECK(s.ptr != NULL);
  CHECK(s.item != NULL);
  /* default size from sb.c is dsize == 5 → 32-byte capacity */
  CHECK_EQ_INT(s.pot, 5);
  sb_kill(&s);
  CHECK(s.ptr == NULL);
  CHECK(s.item == NULL);
  return 0;
}

static int test_build_at_explicit_size(void) {
  sb s;
  sb_build(&s, 8);            /* 256 bytes */
  CHECK_EQ_INT(s.len, 0);
  CHECK_EQ_INT(s.pot, 8);
  sb_kill(&s);
  return 0;
}

/* --- append paths --------------------------------------------------- */

static int test_add_char(void) {
  sb s;
  sb_new(&s);
  sb_add_char(&s, 'a');
  sb_add_char(&s, 'b');
  sb_add_char(&s, 'c');
  CHECK_EQ_INT(s.len, 3);
  CHECK_EQ_MEM(s.ptr, "abc", 3);
  sb_kill(&s);
  return 0;
}

static int test_add_string(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "hello");
  CHECK_EQ_INT(s.len, 5);
  CHECK_EQ_MEM(s.ptr, "hello", 5);
  sb_add_string(&s, " world");
  CHECK_EQ_INT(s.len, 11);
  CHECK_EQ_MEM(s.ptr, "hello world", 11);
  sb_kill(&s);
  return 0;
}

static int test_add_buffer_with_embedded_null(void) {
  sb s;
  const char data[] = { 'a', 0, 'b', 0, 'c' };
  sb_new(&s);
  sb_add_buffer(&s, data, sizeof data);
  CHECK_EQ_INT(s.len, (int)sizeof data);
  CHECK_EQ_MEM(s.ptr, data, sizeof data);
  sb_kill(&s);
  return 0;
}

static int test_add_sb_concat(void) {
  sb a, b;
  sb_new(&a);
  sb_new(&b);
  sb_add_string(&a, "head:");
  sb_add_string(&b, "tail");
  sb_add_sb(&a, &b);
  CHECK_EQ_INT(a.len, 9);
  CHECK_EQ_MEM(a.ptr, "head:tail", 9);
  sb_kill(&a);
  sb_kill(&b);
  return 0;
}

/* --- growth --------------------------------------------------------- */

static int test_grow_past_initial_capacity(void) {
  sb s;
  sb_new(&s);                 /* capacity 32 */
  CHECK_EQ_INT(s.pot, 5);
  /* push 1 KiB to force at least 5 doublings */
  for (int i = 0; i < 1024; i++)
    sb_add_char(&s, (char)('A' + (i & 31)));
  CHECK_EQ_INT(s.len, 1024);
  CHECK(s.pot >= 10);         /* (1<<10) == 1024, may have rounded up */
  /* verify byte 0 and byte 1023 survived all the reallocations */
  CHECK_EQ_INT(s.ptr[0],    'A');
  CHECK_EQ_INT(s.ptr[1023], 'A' + (1023 & 31));
  sb_kill(&s);
  return 0;
}

static int test_grow_via_large_single_string(void) {
  sb s;
  enum { N = 4097 };
  char *big = (char *) malloc(N);
  CHECK(big != NULL);
  for (int i = 0; i < N; i++) big[i] = (char)(i & 0x7f);
  sb_new(&s);
  sb_add_buffer(&s, big, N);
  CHECK_EQ_INT(s.len, N);
  CHECK_EQ_MEM(s.ptr, big, N);
  sb_kill(&s);
  free(big);
  return 0;
}

/* --- reset / terminate --------------------------------------------- */

static int test_reset_keeps_capacity(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "scratch");
  int pot_before = s.pot;
  sb_reset(&s);
  CHECK_EQ_INT(s.len, 0);
  CHECK_EQ_INT(s.pot, pot_before);
  /* should be reusable */
  sb_add_string(&s, "again");
  CHECK_EQ_INT(s.len, 5);
  CHECK_EQ_MEM(s.ptr, "again", 5);
  sb_kill(&s);
  return 0;
}

static int test_sb_terminate_appends_nul_but_excludes_from_len(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "hello");
  char *cstr = sb_terminate(&s);
  /* len stays at 5, but ptr[5] is now '\0' */
  CHECK_EQ_INT(s.len, 5);
  CHECK_EQ_INT(cstr[5], 0);
  CHECK_EQ_INT(strcmp(cstr, "hello"), 0);
  sb_kill(&s);
  return 0;
}

static int test_sb_name_includes_nul_in_len(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "hi");
  char *cstr = sb_name(&s);
  /* sb_name leaves the '\0' in the buffer, len includes it */
  CHECK_EQ_INT(s.len, 3);
  CHECK_EQ_INT(cstr[2], 0);
  CHECK_EQ_INT(strcmp(cstr, "hi"), 0);
  sb_kill(&s);
  return 0;
}

/* --- skippers ------------------------------------------------------- */

static int test_skip_white(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "   \t  x");
  CHECK_EQ_INT(sb_skip_white(0, &s), 6);
  sb_kill(&s);
  return 0;
}

static int test_skip_comma_with_surrounding_ws(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "  , \tx");
  CHECK_EQ_INT(sb_skip_comma(0, &s), 5);
  sb_kill(&s);
  return 0;
}

static int test_skip_comma_without_comma_acts_like_skip_white(void) {
  sb s;
  sb_new(&s);
  sb_add_string(&s, "   x");
  CHECK_EQ_INT(sb_skip_comma(0, &s), 3);
  sb_kill(&s);
  return 0;
}

/* --- sb_eat_literal ------------------------------------------------- */

static int test_eat_literal_double_quote(void) {
  sb in, out;
  sb_new(&in);
  sb_new(&out);
  sb_add_string(&in, "\"hello\" tail");
  int idx = sb_eat_literal(0, &out, &in);
  CHECK_EQ_INT(idx, 7);                          /* past closing " */
  CHECK_EQ_INT(out.len, 7);
  CHECK_EQ_MEM(out.ptr, "\"hello\"", 7);
  sb_kill(&in);
  sb_kill(&out);
  return 0;
}

static int test_eat_literal_single_quote(void) {
  sb in, out;
  sb_new(&in);
  sb_new(&out);
  sb_add_string(&in, "'x'");
  int idx = sb_eat_literal(0, &out, &in);
  CHECK_EQ_INT(idx, 3);
  CHECK_EQ_INT(out.len, 3);
  CHECK_EQ_MEM(out.ptr, "'x'", 3);
  sb_kill(&in);
  sb_kill(&out);
  return 0;
}

static int test_eat_literal_escape_inside_string(void) {
  sb in, out;
  sb_new(&in);
  sb_new(&out);
  /* in source: "a\"b" — the backslash escapes the inner quote */
  sb_add_string(&in, "\"a\\\"b\"");
  int idx = sb_eat_literal(0, &out, &in);
  CHECK_EQ_INT(idx, 6);
  /* sb_eat_literal copies the escaped char (the inner "), dropping the
     backslash itself — result is the 5-byte sequence "a"b" */
  CHECK_EQ_INT(out.len, 5);
  CHECK_EQ_MEM(out.ptr, "\"a\"b\"", 5);
  sb_kill(&in);
  sb_kill(&out);
  return 0;
}

static int test_eat_literal_not_a_string_is_a_noop(void) {
  sb in, out;
  sb_new(&in);
  sb_new(&out);
  sb_add_string(&in, "not_a_string");
  int idx = sb_eat_literal(0, &out, &in);
  CHECK_EQ_INT(idx, 0);
  CHECK_EQ_INT(out.len, 0);
  sb_kill(&in);
  sb_kill(&out);
  return 0;
}

/* --- driver --------------------------------------------------------- */

struct test_case { const char *name; int (*fn)(void); };

static const struct test_case cases[] = {
  { "new_and_kill",                                test_new_and_kill },
  { "build_at_explicit_size",                      test_build_at_explicit_size },
  { "add_char",                                    test_add_char },
  { "add_string",                                  test_add_string },
  { "add_buffer_with_embedded_null",               test_add_buffer_with_embedded_null },
  { "add_sb_concat",                               test_add_sb_concat },
  { "grow_past_initial_capacity",                  test_grow_past_initial_capacity },
  { "grow_via_large_single_string",                test_grow_via_large_single_string },
  { "reset_keeps_capacity",                        test_reset_keeps_capacity },
  { "sb_terminate_appends_nul_but_excludes_from_len", test_sb_terminate_appends_nul_but_excludes_from_len },
  { "sb_name_includes_nul_in_len",                 test_sb_name_includes_nul_in_len },
  { "skip_white",                                  test_skip_white },
  { "skip_comma_with_surrounding_ws",              test_skip_comma_with_surrounding_ws },
  { "skip_comma_without_comma_acts_like_skip_white", test_skip_comma_without_comma_acts_like_skip_white },
  { "eat_literal_double_quote",                    test_eat_literal_double_quote },
  { "eat_literal_single_quote",                    test_eat_literal_single_quote },
  { "eat_literal_escape_inside_string",            test_eat_literal_escape_inside_string },
  { "eat_literal_not_a_string_is_a_noop",          test_eat_literal_not_a_string_is_a_noop },
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
