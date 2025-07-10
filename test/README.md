# pngcheck test suite

This directory contains the test suite for the `pngcheck` utility, which is used
to guard against regressions on PNG validation.

The test files are sourced from the [PngSuite](http://www.schaik.com/pngsuite/)
collection, which provides a comprehensive set of PNG test files designed to
test the robustness and compliance of PNG processing tools.

The test framework uses [Unity](https://github.com/ThrowTheSwitch/Unity) C
testing framework with [Ceedling](https://github.com/ThrowTheSwitch/Ceedling)
build system to run comprehensive tests against the `pngcheck` executable.

## Overview

The pngcheck test suite is organized into a modern C testing structure following
Ceedling 1.0+ best practices adopted from the structure of the
[apm32-ceedling-example](https://github.com/LuckkMaker/apm32-ceedling-example)
repository.

It includes:
- **Test files**: C test files for CLI and PNG suite tests
- **Fixtures**: PNG files from the PngSuite collection
- **Expectations**: Expected outputs for each test case
- **CLI tests**: Static tests for command-line interface functionality
- **PNG suite tests**: Auto-generated tests using the PngSuite image test suite
- **Helper functions**: Reusable test utilities and assertions
- **Configuration**: Ceedling project configuration file (`project.yml`)
- **Management tool**: Ruby CLI to manage test files and expectations

As the Ceedling framework is written in Ruby, the test suite uses a Ruby
command-line tool (`test/bin/pngcheck-test`) to manage the test files,
expectations, and to run the tests.


## Quick start

```bash
# Build pngcheck first
make

# Run all tests (186 tests)
cd test && ceedling test:all

# Run specific test suites
ceedling test:test_pngcheck_cli   # CLI tests (10 tests)
ceedling test:test_pngcheck_suite # PNG suite tests (176 tests)
```

## Test structure

### Test files

- `test/test_pngcheck_cli.c` - CLI functionality tests (manual)
- `test/test_pngcheck_suite.c` - PNG validation tests (auto-generated)
- `test/support/test_helpers.c` - Common test functions

### Directory layout

```
test/
├── project.yml                 # Ceedling configuration
├── bin/                        # Ruby CLI tool for managing tests
│   └── pngcheck-test           # Ruby script to manage tests and expectations
├── lib/                        # Ruby libraries for `pngcheck-test`
├── (build/)                    # Build artifacts from Ceedling
├── test/                       # Test files
│   ├── test_pngcheck_cli.c     # CLI tests
│   ├── test_pngcheck_suite.c   # PNG suite tests (auto-generated)
│   └── support/                # Test helpers
├── fixtures/pngsuite/          # PNG test files (176 files)
├── expectations/pngsuite/      # Expected outputs
└── (build/)                    # Build artifacts from Ceedling
```

## Test approach

Tests validate pngcheck behavior using **simple prefix matching and exit codes**:

- **Exit code**: 0 for valid PNGs, non-zero for errors
- **Output prefix**: "OK:" for valid files, "ERROR:" for invalid files

This approach focuses on core functionality rather than exact output matching.

### CLI tests (10 tests)

Static tests for command-line options:
- Help option (`-h`)
- File handling (valid, invalid, nonexistent)
- Output options (`-v`, `-q`, `-t`, `-p`, `-c`, `-7`)

### PNG suite tests (176 tests)

Auto-generated tests using the [PngSuite](http://www.schaik.com/pngsuite/) collection:
- **Valid PNGs**: Various formats, bit depths, interlacing
- **Invalid PNGs**: Corrupted headers, bad checksums, truncated files

## Test management

The test suite includes a Ruby-based management tool:

```bash
# Check status
bundle exec test/bin/pngcheck-test status

# Download PNG files (if missing) (add --force to redownload)
bundle exec test/bin/pngcheck-test download

# Regenerate expectations (when pngcheck behavior changes) (add --force to overwrite)
bundle exec test/bin/pngcheck-test expectations
```

## Configuration

Key settings in `project.yml`:
- Environment variables for paths
- Modern Ceedling 1.0+ configuration
- Google Test-like output format
- JSON/JUnit/CppUnit reports for CI

## Adding tests

### CLI tests
Edit `test/test_pngcheck_cli.c` and use helper functions:

```c
void test_my_feature(void) {
    test_pngcheck_with_options("-option", "file.png", 0, "expected_content");
}
```

### PNG tests
1. Add PNG files to `fixtures/pngsuite/`
2. Run `bundle exec test/bin/pngcheck-test expectations`
3. Run `bundle exec test/bin/pngcheck-test generate`

## Troubleshooting

**pngcheck not found**: Build with `make` or `cmake`
**Missing PNG files**: Run `bundle exec test/bin/pngcheck-test download`
**Ruby issues**: Run `bundle install`
