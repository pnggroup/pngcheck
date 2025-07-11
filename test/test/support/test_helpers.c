#include "test_helpers.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    // Windows doesn't have unistd.h or POSIX wait macros
    #define WIFEXITED(status) 1
    #define WEXITSTATUS(status) (status)
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

#define MAX_PATH 1024
#define MAX_OUTPUT 4096

// Get path to PngSuite fixtures
const char *get_pngsuite_path(void) {
    static char path[MAX_PATH];
    static bool initialized = false;

    if (!initialized) {
        const char *env_path = getenv("PNGSUITE_PATH");
        if (env_path) {
            strncpy(path, env_path, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        } else {
            fprintf(stderr, "ERROR: PNGSUITE_PATH environment variable not set\n");
            exit(1);
        }
        initialized = true;
    }

    return path;
}

// Get path to expectations directory
const char *get_expectations_path(void) {
    static char path[MAX_PATH];
    static bool initialized = false;

    if (!initialized) {
        const char *env_path = getenv("EXPECTATIONS_PATH");
        if (env_path) {
            strncpy(path, env_path, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
        } else {
            fprintf(stderr, "ERROR: EXPECTATIONS_PATH environment variable not set\n");
            exit(1);
        }
        initialized = true;
    }

    return path;
}

// Run pngcheck and capture output and exit code
static int run_pngcheck(const char *png_path, char *output, size_t output_size) {
    char command[MAX_PATH * 2];
    FILE *fp;

    const char *pngcheck_exe = getenv("PNGCHECK_EXECUTABLE");
    if (!pngcheck_exe) {
        strcpy(output, "ERROR: PNGCHECK_EXE environment variable not set");
        return -1;
    }

    snprintf(command, sizeof(command), "%s \"%s\" 2>&1", pngcheck_exe, png_path);

    fp = popen(command, "r");
    if (!fp) {
        strcpy(output, "ERROR: Failed to run pngcheck");
        return -1;
    }

    // Read output
    size_t total_read = 0;
    while (total_read < output_size - 1 &&
           fgets(output + total_read, output_size - total_read, fp)) {
        total_read = strlen(output);
    }

    int exit_code = pclose(fp);
    if (WIFEXITED(exit_code)) {
        exit_code = WEXITSTATUS(exit_code);
    }

    return exit_code;
}

// Test a PNG file against expected return code and output prefix
void test_png_file(const char *png_filename, int expected_exit_code, const char *expected_prefix) {
    char png_path[MAX_PATH];
    char output[MAX_OUTPUT];

    snprintf(png_path, sizeof(png_path), "%s/%s", get_pngsuite_path(), png_filename);

    int actual_exit_code = run_pngcheck(png_path, output, sizeof(output));

    if (expected_exit_code >= 0 && actual_exit_code != expected_exit_code) {
        char message[MAX_OUTPUT];
        snprintf(message, sizeof(message),
            "Exit code mismatch for %s: expected %d, got %d\nOutput: %s",
            png_filename, expected_exit_code, actual_exit_code, output);
        TEST_FAIL_MESSAGE(message);
    }

    if (expected_prefix) {
        bool prefix_ok = false;
        if (strcmp(expected_prefix, "OK") == 0) {
            prefix_ok = strncmp(output, "OK:", 3) == 0;
        } else if (strcmp(expected_prefix, "ERROR") == 0) {
            prefix_ok = strstr(output, "ERROR:") != NULL;
        } else {
            prefix_ok = strncmp(output, expected_prefix, strlen(expected_prefix)) == 0;
        }

        if (!prefix_ok) {
            char message[MAX_OUTPUT];
            snprintf(message, sizeof(message),
                "Output prefix mismatch for %s: expected '%s'\nActual output: %s",
                png_filename, expected_prefix, output);
            TEST_FAIL_MESSAGE(message);
        }
    }
}

// Test pngcheck with command-line options
void test_pngcheck_with_options(const char *options, const char *png_filename, int expected_exit_code, const char *expected_content) {
    char command[MAX_PATH * 2];
    char output[MAX_OUTPUT];
    FILE *fp;

    const char *pngcheck_exe = getenv("PNGCHECK_EXECUTABLE");
    if (!pngcheck_exe) {
        TEST_FAIL_MESSAGE("PNGCHECK_EXE environment variable not set");
        return;
    }

    if (png_filename) {
        char png_path[MAX_PATH];
        snprintf(png_path, sizeof(png_path), "%s/%s", get_pngsuite_path(), png_filename);
        snprintf(command, sizeof(command), "%s %s \"%s\" 2>&1", pngcheck_exe, options ? options : "", png_path);
    } else {
        snprintf(command, sizeof(command), "%s %s 2>&1", pngcheck_exe, options ? options : "");
    }

    fp = popen(command, "r");
    if (!fp) {
        TEST_FAIL_MESSAGE("Failed to run pngcheck");
        return;
    }

    size_t total_read = 0;
    while (total_read < sizeof(output) - 1 &&
           fgets(output + total_read, sizeof(output) - total_read, fp)) {
        total_read = strlen(output);
    }

    int exit_code = pclose(fp);
    if (WIFEXITED(exit_code)) {
        exit_code = WEXITSTATUS(exit_code);
    }

    if (expected_exit_code >= 0 && exit_code != expected_exit_code) {
        char message[MAX_OUTPUT];
        snprintf(message, sizeof(message),
            "Exit code mismatch for options '%s': expected %d, got %d",
            options ? options : "", expected_exit_code, exit_code);
        TEST_FAIL_MESSAGE(message);
    }

    if (expected_content && strlen(expected_content) > 0 && !strstr(output, expected_content)) {
        char message[MAX_OUTPUT];
        snprintf(message, sizeof(message),
            "Expected content '%s' not found in output for options '%s'",
            expected_content, options ? options : "");
        TEST_FAIL_MESSAGE(message);
    }
}

// Test pngcheck help option
void test_pngcheck_help(void) {
    test_pngcheck_with_options("-h", NULL, 0, "Usage:");
}
