/* Unit tests for src/hash.c — the obstack-backed string-keyed hash
 * table used throughout masp for symbols, macros and formal-arg
 * dictionaries.
 *
 * Linkage: we compile hash.c + compat.c into this binary.  hash.c
 * references the global `chunksize` (defined by masp.c in the real
 * build); we provide our own here so the link is closed-form.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

/* hash.c references `chunksize` to size the obstack chunks.  In the
 * real build masp.c defines this; for the unit-test binary we mirror
 * masp.c's choice (0 → obstack picks its default).  */
int chunksize = 0;

/* as.h macro-rewrites abort() to as_abort(file, line, fn); hash.c
 * pulls that in via #include "as.h".  Provide a test-time stub so the
 * binary links closed-form.  */
__attribute__((noreturn))
void as_abort(const char *file, int line, const char *fn) {
  fprintf(stderr, "test_hash: as_abort at %s:%d in %s\n",
          file, line, fn ? fn : "?");
  abort();  /* libc abort, not the macro (we are outside as.h) */
}

#define CHECK(cond) do { \
    if (!(cond)) { \
      fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      return 1; \
    } \
  } while (0)

#define CHECK_PTR_EQ(a, b) do { \
    void *_a = (void *)(a), *_b = (void *)(b); \
    if (_a != _b) { \
      fprintf(stderr, "  FAIL %s:%d: %s == %s (got %p vs %p)\n", \
              __FILE__, __LINE__, #a, #b, _a, _b); \
      return 1; \
    } \
  } while (0)

#define CHECK_STR_EQ(a, b) do { \
    const char *_a = (a), *_b = (b); \
    if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) { \
      fprintf(stderr, "  FAIL %s:%d: %s == %s (got %s vs %s)\n", \
              __FILE__, __LINE__, #a, #b, _a ? _a : "(null)", _b ? _b : "(null)"); \
      return 1; \
    } \
  } while (0)

/* --- new / die ------------------------------------------------------ */

static int test_new_returns_empty_table(void) {
  struct hash_control *t = hash_new();
  CHECK(t != NULL);
  CHECK(hash_find(t, "missing") == NULL);
  hash_die(t);
  return 0;
}

/* --- insert / find -------------------------------------------------- */

static int test_insert_then_find(void) {
  struct hash_control *t = hash_new();
  int v1 = 42, v2 = 7;
  CHECK(hash_insert(t, "alpha", &v1) == NULL);
  CHECK(hash_insert(t, "beta",  &v2) == NULL);
  CHECK_PTR_EQ(hash_find(t, "alpha"), &v1);
  CHECK_PTR_EQ(hash_find(t, "beta"),  &v2);
  CHECK(hash_find(t, "gamma") == NULL);
  hash_die(t);
  return 0;
}

static int test_insert_duplicate_returns_exists(void) {
  struct hash_control *t = hash_new();
  int v1 = 1, v2 = 2;
  CHECK(hash_insert(t, "key", &v1) == NULL);
  const char *err = hash_insert(t, "key", &v2);
  CHECK(err != NULL);
  CHECK_STR_EQ(err, "exists");
  /* original value untouched */
  CHECK_PTR_EQ(hash_find(t, "key"), &v1);
  hash_die(t);
  return 0;
}

/* --- key ownership: hash_insert must copy ------------------------- */

static int test_insert_copies_key(void) {
  struct hash_control *t = hash_new();
  /* Use a mutable buffer for the key, then scramble it after insert
   * to make sure the table is not holding our pointer.  */
  char key[16];
  strcpy(key, "transient");
  int v = 99;
  CHECK(hash_insert(t, key, &v) == NULL);
  memset(key, 'X', sizeof key - 1);
  key[sizeof key - 1] = 0;
  CHECK_PTR_EQ(hash_find(t, "transient"), &v);
  hash_die(t);
  return 0;
}

/* --- jam: insert-or-replace ---------------------------------------- */

static int test_jam_inserts_when_absent(void) {
  struct hash_control *t = hash_new();
  int v = 5;
  CHECK(hash_jam(t, "fresh", &v) == NULL);
  CHECK_PTR_EQ(hash_find(t, "fresh"), &v);
  hash_die(t);
  return 0;
}

static int test_jam_replaces_when_present(void) {
  struct hash_control *t = hash_new();
  int v1 = 1, v2 = 2;
  CHECK(hash_insert(t, "key", &v1) == NULL);
  CHECK(hash_jam(t, "key", &v2) == NULL);
  CHECK_PTR_EQ(hash_find(t, "key"), &v2);
  hash_die(t);
  return 0;
}

/* --- replace: update only, no insert ------------------------------- */

static int test_replace_returns_old_value(void) {
  struct hash_control *t = hash_new();
  int v1 = 10, v2 = 20;
  CHECK(hash_insert(t, "k", &v1) == NULL);
  void *old = hash_replace(t, "k", &v2);
  CHECK_PTR_EQ(old, &v1);
  CHECK_PTR_EQ(hash_find(t, "k"), &v2);
  hash_die(t);
  return 0;
}

static int test_replace_returns_null_when_absent(void) {
  struct hash_control *t = hash_new();
  int v = 1;
  CHECK(hash_replace(t, "nope", &v) == NULL);
  /* Critical: did NOT insert as a side-effect */
  CHECK(hash_find(t, "nope") == NULL);
  hash_die(t);
  return 0;
}

/* --- delete -------------------------------------------------------- */

static int test_delete_returns_value_and_removes(void) {
  struct hash_control *t = hash_new();
  int v = 77;
  CHECK(hash_insert(t, "k", &v) == NULL);
  void *got = hash_delete(t, "k");
  CHECK_PTR_EQ(got, &v);
  CHECK(hash_find(t, "k") == NULL);
  hash_die(t);
  return 0;
}

static int test_delete_missing_returns_null(void) {
  struct hash_control *t = hash_new();
  CHECK(hash_delete(t, "nothing") == NULL);
  hash_die(t);
  return 0;
}

static int test_delete_does_not_disturb_siblings(void) {
  struct hash_control *t = hash_new();
  int a = 1, b = 2, c = 3;
  CHECK(hash_insert(t, "a", &a) == NULL);
  CHECK(hash_insert(t, "b", &b) == NULL);
  CHECK(hash_insert(t, "c", &c) == NULL);
  CHECK_PTR_EQ(hash_delete(t, "b"), &b);
  CHECK_PTR_EQ(hash_find(t, "a"), &a);
  CHECK(hash_find(t, "b") == NULL);
  CHECK_PTR_EQ(hash_find(t, "c"), &c);
  hash_die(t);
  return 0;
}

/* --- scale --------------------------------------------------------- */

/* Insert enough keys to populate many buckets and exercise the
 * move-to-front cache lookups.  We do not assume any particular
 * bucket layout, just that every key is retrievable.  */
static int test_many_inserts_and_lookups(void) {
  struct hash_control *t = hash_new();
  enum { N = 5000 };
  int values[N];
  char key[32];
  for (int i = 0; i < N; i++) {
    values[i] = i;
    snprintf(key, sizeof key, "sym_%d", i);
    const char *err = hash_insert(t, key, &values[i]);
    if (err != NULL) {
      fprintf(stderr, "  FAIL insert %d: %s\n", i, err);
      hash_die(t);
      return 1;
    }
  }
  /* Forward scan */
  for (int i = 0; i < N; i++) {
    snprintf(key, sizeof key, "sym_%d", i);
    void *got = hash_find(t, key);
    if (got != &values[i]) {
      fprintf(stderr, "  FAIL find sym_%d (forward)\n", i);
      hash_die(t);
      return 1;
    }
  }
  /* Reverse scan — different lookup order than the move-to-front
   * cache was warmed for, so hits the chain walk.  */
  for (int i = N - 1; i >= 0; i--) {
    snprintf(key, sizeof key, "sym_%d", i);
    void *got = hash_find(t, key);
    if (got != &values[i]) {
      fprintf(stderr, "  FAIL find sym_%d (reverse)\n", i);
      hash_die(t);
      return 1;
    }
  }
  hash_die(t);
  return 0;
}

/* --- traverse ------------------------------------------------------ */

struct visit_state { int count; int sum; };

static struct visit_state *g_visit;

static void visit_cb(const char *key, void *value) {
  (void)key;
  g_visit->count++;
  g_visit->sum += *(int *)value;
}

static int test_traverse_visits_every_entry_once(void) {
  struct hash_control *t = hash_new();
  int v[3] = { 10, 20, 30 };
  CHECK(hash_insert(t, "a", &v[0]) == NULL);
  CHECK(hash_insert(t, "b", &v[1]) == NULL);
  CHECK(hash_insert(t, "c", &v[2]) == NULL);
  struct visit_state vs = { 0, 0 };
  g_visit = &vs;
  hash_traverse(t, visit_cb);
  CHECK(vs.count == 3);
  CHECK(vs.sum   == 60);
  hash_die(t);
  return 0;
}

/* --- empty-key edge case ------------------------------------------- */

static int test_insert_empty_key(void) {
  struct hash_control *t = hash_new();
  int v = 5;
  CHECK(hash_insert(t, "", &v) == NULL);
  CHECK_PTR_EQ(hash_find(t, ""), &v);
  CHECK_PTR_EQ(hash_delete(t, ""), &v);
  CHECK(hash_find(t, "") == NULL);
  hash_die(t);
  return 0;
}

/* --- driver -------------------------------------------------------- */

struct test_case { const char *name; int (*fn)(void); };

static const struct test_case cases[] = {
  { "new_returns_empty_table",            test_new_returns_empty_table },
  { "insert_then_find",                   test_insert_then_find },
  { "insert_duplicate_returns_exists",    test_insert_duplicate_returns_exists },
  { "insert_copies_key",                  test_insert_copies_key },
  { "jam_inserts_when_absent",            test_jam_inserts_when_absent },
  { "jam_replaces_when_present",          test_jam_replaces_when_present },
  { "replace_returns_old_value",          test_replace_returns_old_value },
  { "replace_returns_null_when_absent",   test_replace_returns_null_when_absent },
  { "delete_returns_value_and_removes",   test_delete_returns_value_and_removes },
  { "delete_missing_returns_null",        test_delete_missing_returns_null },
  { "delete_does_not_disturb_siblings",   test_delete_does_not_disturb_siblings },
  { "many_inserts_and_lookups",           test_many_inserts_and_lookups },
  { "traverse_visits_every_entry_once",   test_traverse_visits_every_entry_once },
  { "insert_empty_key",                   test_insert_empty_key },
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
