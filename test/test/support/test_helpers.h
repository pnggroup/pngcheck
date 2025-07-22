#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdbool.h>

// Test a PNG file against expected return code and output prefix
void test_png_file(const char *png_filename, int expected_exit_code, const char *expected_prefix);

// Test pngcheck with command-line options
void test_pngcheck_with_options(const char *options, const char *png_filename, int expected_exit_code, const char *expected_content);

// Test pngcheck help option
void test_pngcheck_help(void);

// Utility functions
const char *get_pngsuite_path(void);
const char *get_expectations_path(void);

#endif // TEST_HELPERS_H
