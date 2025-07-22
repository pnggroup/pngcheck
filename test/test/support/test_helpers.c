#include "test_helpers.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
    #define WIFEXITED(status) 1
    #define WEXITSTATUS(status) (status)
    #define IS_WINDOWS 1
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #define IS_WINDOWS 0
#endif

#define MAX_PATH 1024
#define MAX_OUTPUT 4096

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

static int run_command(const char *command, char *output, size_t output_size) {
    FILE *fp = popen(command, "r");
    if (!fp) {
        strcpy(output, "ERROR: Failed to run command");
        return -1;
    }

    size_t total_read = 0;
    while (total_read < output_size - 1 &&
           fgets(output + total_read, output_size - total_read, fp)) {
        total_read = strlen(output);
    }

    int exit_code = pclose(fp);
    return WIFEXITED(exit_code) ? WEXITSTATUS(exit_code) : exit_code;
}

static const char *clean_output(const char *output) {
    static char cleaned[MAX_OUTPUT];
    char *dest = cleaned;
    const char *line = output;

    while (*line) {
        const char *line_start = line;
        const char *line_end = strchr(line, '\n');
        if (!line_end) line_end = line + strlen(line);

        // Skip warning lines and empty lines
        bool skip = false;
        if (strstr(line_start, "warning:") || strstr(line_start, "zlib warning:")) {
            skip = true;
        } else {
            // Check if empty (only whitespace)
            skip = true;
            for (const char *p = line_start; p < line_end; p++) {
                if (!isspace(*p)) {
                    skip = false;
                    break;
                }
            }
        }

        if (!skip) {
            size_t line_len = line_end - line_start;
            if (dest + line_len + 1 < cleaned + sizeof(cleaned)) {
                memcpy(dest, line_start, line_len);
                dest += line_len;
                if (*line_end == '\n') *dest++ = '\n';
            }
        }

        line = (*line_end == '\n') ? line_end + 1 : line_end;
        if (*line_end == '\0') break;
    }

    *dest = '\0';
    return cleaned;
}

static void escape_shell_arg(const char *input, char *output, size_t output_size) {
    if (!input || !output || output_size < 3) {
        if (output && output_size > 0) output[0] = '\0';
        return;
    }

    size_t pos = 0;
    output[pos++] = '"';

    for (const char *p = input; *p && pos < output_size - 2; p++) {
        if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
            if (pos < output_size - 3) {
                output[pos++] = '\\';
                output[pos++] = *p;
            }
        } else {
            output[pos++] = *p;
        }
    }

    output[pos++] = '"';
    output[pos] = '\0';
}

static int build_command(char *command, size_t command_size, const char *pngcheck_exe,
                        const char *options, const char *png_filename) {
    char escaped_filename[MAX_PATH * 2];
    char escaped_options[MAX_PATH * 2];

    if (!command || !pngcheck_exe) {
        return -1;
    }

    // Escape the filename if provided
    if (png_filename) {
        char full_path[MAX_PATH];
        snprintf(full_path, sizeof(full_path), "%s/%s", get_pngsuite_path(), png_filename);
        escape_shell_arg(full_path, escaped_filename, sizeof(escaped_filename));
    }

    // Escape options if provided
    if (options) {
        escape_shell_arg(options, escaped_options, sizeof(escaped_options));
    }

    // Build the command safely
    if (png_filename && options) {
        snprintf(command, command_size, "%s %s %s 2>&1",
                pngcheck_exe, escaped_options, escaped_filename);
    } else if (png_filename) {
        snprintf(command, command_size, "%s %s 2>&1",
                pngcheck_exe, escaped_filename);
    } else if (options) {
        snprintf(command, command_size, "%s %s 2>&1",
                pngcheck_exe, escaped_options);
    } else {
        snprintf(command, command_size, "%s 2>&1", pngcheck_exe);
    }

    return 0;
}

void test_png_file(const char *png_filename, int expected_exit_code, const char *expected_prefix) {
    char command[MAX_PATH * 2];
    char output[MAX_OUTPUT];

    const char *pngcheck_exe = getenv("PNGCHECK_EXECUTABLE");
    if (!pngcheck_exe) {
        TEST_FAIL_MESSAGE("PNGCHECK_EXECUTABLE environment variable not set");
        return;
    }

    if (build_command(command, sizeof(command), pngcheck_exe, NULL, png_filename) != 0) {
        TEST_FAIL_MESSAGE("Failed to build command");
        return;
    }

    int actual_exit_code = run_command(command, output, sizeof(output));

    // Check exit code (skip on Windows)
    if (!IS_WINDOWS && expected_exit_code >= 0 && actual_exit_code != expected_exit_code) {
        char message[MAX_OUTPUT];
        snprintf(message, sizeof(message),
            "Exit code mismatch for %s: expected %d, got %d\nOutput: %s",
            png_filename, expected_exit_code, actual_exit_code, output);
        TEST_FAIL_MESSAGE(message);
    }

    // Check status output
    if (expected_prefix) {
        const char *cleaned = clean_output(output);
        bool ok = false;

        if (strcmp(expected_prefix, "OK") == 0) {
            ok = strncmp(cleaned, "OK:", 3) == 0;
        } else if (strcmp(expected_prefix, "ERROR") == 0) {
            ok = strstr(cleaned, "ERROR:") != NULL;
        } else {
            ok = strncmp(cleaned, expected_prefix, strlen(expected_prefix)) == 0;
        }

        if (!ok) {
            char message[MAX_OUTPUT];
            snprintf(message, sizeof(message),
                "Output mismatch for %s: expected '%s'\nActual: %s\nCleaned: %s",
                png_filename, expected_prefix, output, cleaned);
            TEST_FAIL_MESSAGE(message);
        }
    }
}

void test_pngcheck_with_options(const char *options, const char *png_filename, int expected_exit_code, const char *expected_content) {
    char command[MAX_PATH * 2];
    char output[MAX_OUTPUT];

    const char *pngcheck_exe = getenv("PNGCHECK_EXECUTABLE");
    if (!pngcheck_exe) {
        TEST_FAIL_MESSAGE("PNGCHECK_EXECUTABLE environment variable not set");
        return;
    }

    if (build_command(command, sizeof(command), pngcheck_exe, options, png_filename) != 0) {
        TEST_FAIL_MESSAGE("Failed to build command");
        return;
    }

    int exit_code = run_command(command, output, sizeof(output));

    // Check exit code (skip on Windows)
    if (!IS_WINDOWS && expected_exit_code >= 0 && exit_code != expected_exit_code) {
        char message[MAX_OUTPUT];
        snprintf(message, sizeof(message),
            "Exit code mismatch for options '%s': expected %d, got %d\nOutput: %s",
            options ? options : "", expected_exit_code, exit_code, output);
        TEST_FAIL_MESSAGE(message);
    }

    // Check content
    if (expected_content && strlen(expected_content) > 0) {
        const char *cleaned = clean_output(output);
        if (!strstr(cleaned, expected_content)) {
            char message[MAX_OUTPUT];
            snprintf(message, sizeof(message),
                "Expected content '%s' not found for options '%s'\nActual: %s\nCleaned: %s",
                expected_content, options ? options : "", output, cleaned);
            TEST_FAIL_MESSAGE(message);
        }
    }
}

void test_pngcheck_help(void) {
    test_pngcheck_with_options("-h", NULL, 0, "Usage:");
}
