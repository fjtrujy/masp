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
      // Trim trailing newlines for nicer printing
      size_t len_e = strlen(le); if (len_e && (le[len_e-1] == '\n' || le[len_e-1] == '\r')) le[len_e-1] = '\0';
      size_t len_o = strlen(lo); if (len_o && (lo[len_o-1] == '\n' || lo[len_o-1] == '\r')) lo[len_o-1] = '\0';
      fprintf(stderr, "L%05d | EXPECTED: %s\n", line, le);
      fprintf(stderr, "L%05d | ACTUAL  : %s\n", line, lo);
      shown++;
      if (shown >= 50) break;
    }
    line++;
  }
  // If actual has extra lines
  if (shown == 0) {
    // Check for extra lines when content so far matched
    if (fgets(lo, sizeof(lo), fo)) {
      size_t len_o = strlen(lo); if (len_o && (lo[len_o-1] == '\n' || lo[len_o-1] == '\r')) lo[len_o-1] = '\0';
      fprintf(stderr, "L%05d | EXPECTED: <EOF>\n", line);
      fprintf(stderr, "L%05d | ACTUAL  : %s\n", line, lo);
    }
  }
  fclose(fo);
  fclose(fe);
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
  // Normalize to backslashes for Windows APIs
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
    // Prefer a clean subprocess exec on UNIX to avoid sanitizer/locale clashes on CI
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
        // Fallback to in-process to surface better diagnostics
        const char *argv0[] = { "masp", "-p", "-s", "-c", ";", "-o", out_path, "--", src_path, NULL };
        int argc = 9;
        rc = masp_program_main(argc, (char**)argv0);
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
  struct stat st;
  if (stat(out_path, &st) != 0) {
    fprintf(stderr, "Output file not created: %s\n", out_path);
    return 1;
  }
  if (!files_equal(out_path, expected_path)) {
    struct stat st2; st2.st_size = -1; (void)stat(out_path, &st2);
    fprintf(stderr, "Output differs from expected: %s (size=%ld)\n", out_path, (long)st2.st_size);
    print_diff_snippet(out_path, expected_path);
    return 1;
  }
  return 0;
}

int main(void) {
  int failures = 0;
  failures += run_vu1Triangle();
  if (failures) {
    fprintf(stderr, "Unit tests failed: %d\n", failures);
    return 1;
  }
  printf("All unit tests passed.\n");
  return 0;
}


