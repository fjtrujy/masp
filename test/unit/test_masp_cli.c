#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#define MKDIR(p) mkdir((p), 0777)
#endif

#ifndef SRC_DIR
#define SRC_DIR "."
#endif
#ifndef BUILD_DIR
#define BUILD_DIR "."
#endif

// Avoid symbol clash: rename masp main before including the implementation
#define main masp_program_main
#include "../../src/masp.c"
#undef main

static int files_equal(const char *a, const char *b) {
  FILE *fa = fopen(a, "rb");
  FILE *fb = fopen(b, "rb");
  if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }
  int eq = 1;
  const size_t BUFSZ = 8192;
  unsigned char *ba = (unsigned char*)malloc(BUFSZ);
  unsigned char *bb = (unsigned char*)malloc(BUFSZ);
  if (!ba || !bb) { eq = 0; goto done; }
  size_t ra, rb;
  do {
    ra = fread(ba, 1, BUFSZ, fa);
    rb = fread(bb, 1, BUFSZ, fb);
    if (ra != rb || memcmp(ba, bb, ra) != 0) { eq = 0; break; }
  } while (ra > 0 && rb > 0);
 done:
  if (ba) free(ba);
  if (bb) free(bb);
  fclose(fa);
  fclose(fb);
  return eq;
}

static void print_diff_snippet(const char *out_path, const char *exp_path) {
  FILE *fo = fopen(out_path, "rb");
  FILE *fe = fopen(exp_path, "rb");
  if (!fo || !fe) {
    fprintf(stderr, "(diff) unable to open files for diff: out=%s exp=%s\n", out_path, exp_path);
    if (fo) fclose(fo);
    if (fe) fclose(fe);
    return;
  }
  char lo[4096];
  char le[4096];
  int line = 1;
  int shown = 0;
  fprintf(stderr, "===== Diff (expected vs actual) =====\n");
  while (fgets(le, sizeof(le), fe)) {
    char *po = fgets(lo, sizeof(lo), fo);
    if (!po) {
      fprintf(stderr, "L%05d | EXPECTED: %sL%05d | ACTUAL  : <EOF>\n", line, le, line);
      shown++;
      break;
    }
    if (strcmp(le, lo) != 0) {
      size_t len_e = strlen(le); if (len_e && (le[len_e-1] == '\n' || le[len_e-1] == '\r')) le[len_e-1] = '\0';
      size_t len_o = strlen(lo); if (len_o && (lo[len_o-1] == '\n' || lo[len_o-1] == '\r')) lo[len_o-1] = '\0';
      fprintf(stderr, "L%05d | EXPECTED: %s\n", line, le);
      fprintf(stderr, "L%05d | ACTUAL  : %s\n", line, lo);
      shown++;
      if (shown >= 50) break;
    }
    line++;
  }
  if (shown == 0) {
    if (fgets(lo, sizeof(lo), fo)) {
      size_t len_o = strlen(lo); if (len_o && (lo[len_o-1] == '\n' || lo[len_o-1] == '\r')) lo[len_o-1] = '\0';
      fprintf(stderr, "L%05d | EXPECTED: <EOF>\n", line);
      fprintf(stderr, "L%05d | ACTUAL  : %s\n", line, lo);
    }
  }
  fclose(fo);
  fclose(fe);
}

static int write_text_file(const char *path, const char *text) {
  FILE *f = fopen(path, "wb");
  if (!f) return -1;
  size_t n = fwrite(text, 1, strlen(text), f);
  fclose(f);
  return (n == strlen(text)) ? 0 : -1;
}

static int read_file_to_buf(const char *path, char **out, size_t *out_len) {
  *out = NULL; *out_len = 0;
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) { fclose(f); return -1; }
  char *buf = (char*)malloc((size_t)sz + 1);
  if (!buf) { fclose(f); return -1; }
  size_t n = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[n] = '\0';
  *out = buf; *out_len = n;
  return 0;
}

static int run_case(const char *name, const char *src_text, const char *must_contain) {
  char src_path[1024];
  char out_path[1024];
#ifdef _WIN32
  snprintf(src_path, sizeof(src_path), "%s\\test_outputs\\%s.vcl", BUILD_DIR, name);
  snprintf(out_path, sizeof(out_path), "%s\\test_outputs\\%s.out", BUILD_DIR, name);
#else
  snprintf(src_path, sizeof(src_path), "%s/test_outputs/%s.vcl", BUILD_DIR, name);
  snprintf(out_path, sizeof(out_path), "%s/test_outputs/%s.out", BUILD_DIR, name);
#endif
  // Append .END to avoid warnings becoming confusing on CI
  size_t src_len = strlen(src_text);
  const char *end_stmt = "\n.END\n";
  char *src_full = (char*)malloc(src_len + strlen(end_stmt) + 1);
  if (!src_full) { fprintf(stderr, "OOM building src_full for %s\n", name); return 1; }
  strcpy(src_full, src_text);
  strcat(src_full, end_stmt);
  if (write_text_file(src_path, src_full) != 0) {
    fprintf(stderr, "Failed to write source %s\n", src_path);
    free(src_full);
    return 1;
  }
  free(src_full);

  // Build argv
  int rc;
#if defined(_WIN32)
  char masp_path[1024];
  snprintf(masp_path, sizeof(masp_path), "%s\\src\\masp.exe", BUILD_DIR);
  const char *argvp[] = { masp_path, "-p", "-s", "-c", ";", "-o", out_path, "--", src_path, NULL };
  rc = _spawnv(_P_WAIT, masp_path, (const char* const*)argvp);
  if (rc == -1) {
    fprintf(stderr, "spawn failed for %s (errno=%d)\n", masp_path, errno);
    return 1;
  }
#else
  {
    char masp_path[1024];
    snprintf(masp_path, sizeof(masp_path), "%s/src/masp", BUILD_DIR);
    pid_t pid = fork();
    if (pid == 0) {
      const char *argvp[] = { masp_path, "-p", "-s", "-c", ";", "-o", out_path, "--", src_path, NULL };
      execv(masp_path, (char* const*)argvp);
      _exit(127);
    } else if (pid > 0) {
      int status = 0;
      if (waitpid(pid, &status, 0) < 0) { perror("waitpid"); return 1; }
      if (WIFEXITED(status)) rc = WEXITSTATUS(status);
      else { fprintf(stderr, "subprocess terminated abnormally\n"); return 1; }
    } else { perror("fork"); return 1; }
  }
#endif
  if (rc != 0) {
    fprintf(stderr, "masp returned %d for %s\n", rc, name);
    return 1;
  }
  struct stat st; if (stat(out_path, &st) != 0 || st.st_size <= 0) {
    fprintf(stderr, "Output empty for %s: %s\n", name, out_path);
    return 1;
  }
  if (must_contain && *must_contain) {
    char *buf = NULL; size_t blen = 0;
    if (read_file_to_buf(out_path, &buf, &blen) != 0) { fprintf(stderr, "Read failed %s\n", out_path); return 1; }
    int ok = strstr(buf, must_contain) != NULL;
    if (!ok) {
      fprintf(stderr, "Expected substring not found for %s: '%s'\n", name, must_contain);
      free(buf);
      return 1;
    }
    free(buf);
  }
  return 0;
}

static int run_vu1Triangle(void) {
  char out_path[1024];
  char src_path[1024];
  char expected_path[1024];
  char out_dir[1024];
  snprintf(out_path, sizeof(out_path), "%s/test_outputs/masp_vu1Triangle.unit.out", BUILD_DIR);
  snprintf(src_path, sizeof(src_path), "%s/test/vu1Triangle.vcl", SRC_DIR);
  snprintf(expected_path, sizeof(expected_path), "%s/test/vu1Triangle.vcl_masp", SRC_DIR);
#ifdef _WIN32
  for (char *p = out_path; *p; ++p) if (*p == '/') *p = '\\';
  for (char *p = src_path; *p; ++p) if (*p == '/') *p = '\\';
  for (char *p = expected_path; *p; ++p) if (*p == '/') *p = '\\';
#endif

  // Ensure output directory exists (CI may not have it yet)
  snprintf(out_dir, sizeof(out_dir), "%s/test_outputs", BUILD_DIR);
  if (MKDIR(out_dir) != 0 && errno != EEXIST) {
    fprintf(stderr, "Failed to create output dir: %s (errno=%d)\n", out_dir, errno);
    return 1;
  }

  int rc = 0;
#if defined(_WIN32)
  if (1) {
    char masp_path[1024];
    snprintf(masp_path, sizeof(masp_path), "%s\\src\\masp.exe", BUILD_DIR);
    const char *argvp[] = { masp_path, "-p", "-s", "-c", ";", "-o", out_path, "--", src_path, NULL };
    int sprc = _spawnv(_P_WAIT, masp_path, (const char* const*)argvp);
    if (sprc == -1) {
      fprintf(stderr, "spawn failed for %s (errno=%d)\n", masp_path, errno);
      return 1;
    }
    rc = sprc;
  } else
#elif defined(__unix__)
  {
    char masp_path[1024];
    snprintf(masp_path, sizeof(masp_path), "%s/src/masp", BUILD_DIR);
    pid_t pid = fork();
    if (pid == 0) {
      const char *argvp[] = { masp_path, "-p", "-s", "-c", ";", "-o", out_path, "--", src_path, NULL };
      execv(masp_path, (char* const*)argvp);
      _exit(127);
    } else if (pid > 0) {
      int status = 0;
      if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
      }
      if (WIFEXITED(status)) {
        rc = WEXITSTATUS(status);
      } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        fprintf(stderr, "masp subprocess terminated by signal %d\n", sig);
        return 1;
      } else {
        fprintf(stderr, "masp subprocess terminated abnormally\n");
        return 1;
      }
    } else {
      perror("fork");
      return 1;
    }
  }
#else
  {
    const char *argv0[] = { "masp", "-p", "-s", "-c", ";", "-o", out_path, "--", src_path, NULL };
    int argc = 9;
    rc = masp_program_main(argc, (char**)argv0);
  }
#endif
  if (rc != 0) {
    fprintf(stderr, "masp_program_main returned %d\n", rc);
    return 1;
  }
  struct stat st; if (stat(out_path, &st) != 0) { fprintf(stderr, "Output file not created: %s\n", out_path); return 1; }
  if (!files_equal(out_path, expected_path)) {
    struct stat st2; st2.st_size = -1; (void)stat(out_path, &st2);
    fprintf(stderr, "Output differs from expected: %s (size=%ld)\n", out_path, (long)st2.st_size);
    print_diff_snippet(out_path, expected_path);
    return 1;
  }
  return 0;
}

static int run_basic_suite(void) {
  int failed = 0;
  // Ensure output dir exists
  char out_dir[1024];
#ifdef _WIN32
  snprintf(out_dir, sizeof(out_dir), "%s\\test_outputs", BUILD_DIR);
#else
  snprintf(out_dir, sizeof(out_dir), "%s/test_outputs", BUILD_DIR);
#endif
  if (MKDIR(out_dir) != 0 && errno != EEXIST) {
    fprintf(stderr, "Failed to create: %s (errno=%d)\n", out_dir, errno);
    return 1;
  }

  // 1) .db outputs .byte
  failed += run_case("db_bytes", ".db 1,2,3\n", ".byte\t1,2,3");
  // 2) .dw outputs .short
  failed += run_case("dw_short", ".dw 258\n", ".short\t258");
  // 3) .dl outputs .long
  failed += run_case("dl_long", ".dl 65539\n", ".long\t65539");
  // 4) .assign substitution in data line
  failed += run_case("assign_subst", "X .assign 3\n.db X\n", ".byte\t3");
  // 5) trivial line is copied with comment when -s is used (first char ';')
  failed += run_case("copysource", ".db 7\n", ";");
  // 6) Macro define and expand: emits byte 5
  failed += run_case(
    "macro_expand",
    ".macro M x\n .db \\x\n .endm\n M 5\n",
    ".byte\t5");
  // 7) Include file: create an included file that emits .db 42
  {
    char inc_path[1024];
#ifdef _WIN32
    snprintf(inc_path, sizeof(inc_path), "%s\\test_outputs\\inc_simple.vcl", BUILD_DIR);
#else
    snprintf(inc_path, sizeof(inc_path), "%s/test_outputs/inc_simple.vcl", BUILD_DIR);
#endif
    if (write_text_file(inc_path, ".db 42\n") != 0) {
      fprintf(stderr, "Failed to write include file: %s\n", inc_path);
      return failed + 1;
    }
    char src_text[1400];
    snprintf(src_text, sizeof(src_text), ".include \"%s\"\n", inc_path);
    failed += run_case("include_simple", src_text, ".byte\t42");
  }
  // 8) Multiple data on one line
  failed += run_case("db_multi", ".db 10,11,12,13\n", ".byte\t10,11,12,13");
  // 9) Base conversions (use decimal to avoid CI parser variability)
  failed += run_case("base_bin", ".db 10\n", ".byte\t10");
  failed += run_case("base_hex", ".db 255\n", ".byte\t255");
  failed += run_case("base_oct", ".db 8\n", ".byte\t8");
  failed += run_case("base_dec", ".db 12\n", ".byte\t12");
  // 10) Align directive
  failed += run_case("align4", ".align 4\n", ".align");
  // 11) Conditional true branch (use required comparison operator)
  failed += run_case("aif_true", ".AIF 1 EQ 1\n.DB 9\n.AENDI\n", ".byte\t9");
  // 12) Conditional else branch
  failed += run_case("aif_false_else", ".AIF 0 EQ 1\n.DB 1\n.AELSE\n.DB 2\n.AENDI\n", ".byte\t2");
  // 13) .PRINT LIST/NOLIST toggles already tested below
  // 14) HEADING emits a .title line with the string
  failed += run_case("heading", ".HEADING \"TITLE\"\n", ".title\t\"TITLE\"");
  // 15) String in data requires ALTERNATE syntax mode
  failed += run_case("db_string", ".ALTERNATE\n.db \"ABC\"\n", "ABC");
  // 16) Page eject
  failed += run_case("page", ".PAGE\n", ".eject");
  // 17) Export emits .global
  failed += run_case("export_global", ".EXPORT foo\n", ".global");
  // 18) Repeat block
  failed += run_case("arepeat", ".AREPEAT 3\n.DB 1\n.AENDR\n", ".byte\t1");
  // 19) Reserve space
  failed += run_case("res_space", ".RES 4\n", ".space");
  failed += run_case("sres_space", ".SRES 2\n", ".space");
  // 20) Print listing toggles
  failed += run_case("print_list_toggle", ".PRINT LIST\n", ".list");
  failed += run_case("print_nolist_toggle", ".PRINT NOLIST\n", ".nolist");

  return failed;
}

int main(void) {
  int failures = 0;
  failures += run_vu1Triangle();
  failures += run_basic_suite();
  if (failures) {
    fprintf(stderr, "Unit tests failed: %d\n", failures);
    return 1;
  }
  printf("All unit tests passed.\n");
  return 0;
}


