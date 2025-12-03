#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

#ifndef SRC_DIR
#define SRC_DIR "."
#endif
#ifndef BUILD_DIR
#define BUILD_DIR "."
#endif

// Test masp stability under repeated execution
// This reproduces the crash observed in ps2gl builds

static int run_masp_once(const char *masp_path, const char *input, const char *output) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        const char *argv[] = { masp_path, "-p", "-s", "-c", ";", "-o", output, "--", input, NULL };
        execv(masp_path, (char* const*)argv);
        _exit(127);
    } else if (pid > 0) {
        // Parent process
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            fprintf(stderr, "masp terminated by signal %d (iter with input=%s)\n", sig, input);
            return 128 + sig;  // Signal death
        } else {
            fprintf(stderr, "masp terminated abnormally\n");
            return -1;
        }
    } else {
        perror("fork");
        return -1;
    }
}

int main(void) {
    char masp_path[1024];
    char output_path[1024];

    snprintf(masp_path, sizeof(masp_path), "%s/src/masp", BUILD_DIR);

    // Check if masp exists
    struct stat st;
    if (stat(masp_path, &st) != 0) {
        fprintf(stderr, "masp binary not found: %s\n", masp_path);
        return 1;
    }

    // Create output directory
    char out_dir[1024];
    snprintf(out_dir, sizeof(out_dir), "%s/test_outputs", BUILD_DIR);
    if (mkdir(out_dir, 0777) != 0 && errno != EEXIST) {
        fprintf(stderr, "Failed to create output dir: %s\n", out_dir);
        return 1;
    }

    // Test files - only use self-contained files without external dependencies
    // fast_pp1.vcl and general_nospec_tri_pp1.vcl require ps2gl includes
    const char *test_files[] = {
        "vu1Triangle.vcl"
    };
    int num_test_files = sizeof(test_files) / sizeof(test_files[0]);

    int total_iterations = 0;
    int total_crashes = 0;
    int total_failures = 0;

    for (int file_idx = 0; file_idx < num_test_files; file_idx++) {
        char input_path[1024];
        snprintf(input_path, sizeof(input_path), "%s/test/%s", SRC_DIR, test_files[file_idx]);

        // Check if file exists
        if (stat(input_path, &st) != 0) {
            printf("Skipping %s (not found)\n", test_files[file_idx]);
            continue;
        }

        printf("\nTesting with %s (50 iterations)...\n", test_files[file_idx]);
        int failures = 0;
        int crashes = 0;

        for (int i = 0; i < 50; i++) {
            snprintf(output_path, sizeof(output_path), "%s/test_outputs/stress_%s_%d.out",
                     BUILD_DIR, test_files[file_idx], i);

            int rc = run_masp_once(masp_path, input_path, output_path);

            if (rc != 0) {
                if (rc >= 128) {
                    crashes++;
                    fprintf(stderr, "  CRASH on iteration %d (signal %d)\n", i + 1, rc - 128);
                } else {
                    failures++;
                    fprintf(stderr, "  FAILURE on iteration %d (exit code %d)\n", i + 1, rc);
                }
            }

            // Clean up output file
            unlink(output_path);

            if ((i + 1) % 10 == 0) {
                printf("  %d iterations... ", i + 1);
                if (crashes > 0 || failures > 0) {
                    printf("%d crashes, %d failures\n", crashes, failures);
                } else {
                    printf("OK\n");
                }
            }

            total_iterations++;
        }

        total_crashes += crashes;
        total_failures += failures;

        printf("  File %s: %d crashes, %d failures (%.1f%% success)\n",
               test_files[file_idx], crashes, failures,
               100.0 * (50 - crashes - failures) / 50.0);
    }

    printf("\n=== OVERALL RESULTS ===\n");
    printf("Total iterations: %d\n", total_iterations);
    printf("Total crashes: %d\n", total_crashes);
    printf("Total failures: %d\n", total_failures);
    printf("Overall success rate: %.1f%%\n",
           100.0 * (total_iterations - total_crashes - total_failures) / total_iterations);

    if (total_crashes > 0) {
        fprintf(stderr, "\nERROR: Detected %d crashes during stress test\n", total_crashes);
        return 1;
    }

    if (total_failures > 5) {
        fprintf(stderr, "\nERROR: Too many failures (%d) during stress test\n", total_failures);
        return 1;
    }

    printf("\nStress test PASSED\n");
    return 0;
}
