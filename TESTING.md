# Testing pngcheck

## Introduction

This document describes the comprehensive testing framework for the `pngcheck`
utility, which uses a hybrid CMake/Ceedling/Unity approach to ensure robust PNG
validation across all platforms.

## Architecture

The `test/` directory contains the test suite for the `pngcheck` utility, which
is used to guard against regressions on PNG validation.

The test files are sourced from the [PngSuite](http://www.schaik.com/pngsuite/)
collection, which provides a comprehensive set of PNG test files designed to
test the robustness and compliance of PNG processing tools.

The test framework uses [Unity](https://github.com/ThrowTheSwitch/Unity) C
testing framework with [Ceedling](https://github.com/ThrowTheSwitch/Ceedling)
build system to run comprehensive tests against the `pngcheck` executable.

The CMake build infrastructure seamlessly integrates with Ceedling, allowing you
to run tests directly from CMake without needing to manually invoke Ceedling
commands.

**Note:** The CMake/Ceedling integration architecture follows best practices
adopted from the structure of the
[apm32-ceedling-example](https://github.com/LuckkMaker/apm32-ceedling-example)
repository.


```
┌────────────────────────────┬──────────────────────────────┬──────────────────────────────┐
│  CMake (build)             │  CMake Targets (bridge)      │  Ceedling (test)             │
├────────────────────────────┼──────────────────────────────┼──────────────────────────────┤
│ ┌────────────────────────┐ │ ┌──────────────────────────┐ │ ┌──────────────────────────┐ │
│ │  Cross-platform        │ │ │  Test targets            │ │ │  Unity framework         │ │
│ │  compilation           │ │ │                          │ │ │  (C testing library)     │ │
│ │                        │ │ │ • test-all               │ │ │                          │ │
│ │ • linux (gcc)          │ │ │ • test-verbose           │ │ │ • assertions             │ │
│ │ • macos (clang)        │ │ │ • test-coverage          │ │ │ • test runners           │ │
│ │ • windows (msvc/msys2) │ │ │ • test-clean             │ │ │ • mocks (unused)         │ │
│ └────────────────────────┘ │ └──────────────────────────┘ │ └──────────────────────────┘ │
│ ┌────────────────────────┐ │ ┌──────────────────────────┐ │ ┌──────────────────────────┐ │
│ │  Build dependencies    │ │ │  Integration             │ │ │  Test management         │ │
│ │                        │ │ │                          │ │ │                          │ │
│ │ • system zlib          │ │ │ • auto executable path   │ │ │ • fixtures               │ │
│ │ • automatic fallback   │ │ │ • dependency management  │ │ │ • expectation generation │ │
│ │ • vcpkg (windows)      │ │ │ • environment setup      │ │ │ • report generation      │ │
│ └────────────────────────┘ │ └──────────────────────────┘ │ └──────────────────────────┘ │
└────────────────────────────┴──────────────────────────────┴──────────────────────────────┘
```

## Overview

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


## Prerequisites

### Overview

- **CMake**: Required for building the project and running tests
- **Ruby**: Required for running the Ceedling test framework
- **Ceedling**: Ruby-based testing framework (installed via Bundler)
- **Unity**: C testing framework (included with Ceedling)
- **Python**: Required for coverage analysis (optional, for coverage targets)
- **gcovr**: Python package for coverage analysis (optional, for coverage targets)

### Installation

- **CMake**: Install from your package manager or download from
  [cmake.org](https://cmake.org/download/)

- **Ruby**: Install from your package manager or download from
  [ruby-lang.org](https://www.ruby-lang.org/en/downloads/)

- **Bundler**: Install via RubyGems:
  ```bash
  gem install bundler
  ```

- **Ceedling**: Install via Bundler:
  ```bash
  bundle install
  ```

- **Python**: Install from your package manager or download from
  [python.org](https://www.python.org/downloads/)

- **gcovr**: Install via pip:
  ```bash
  pip install gcovr
  ```


## Quick start

### Using CMake (Recommended)

```bash
# Configure pngcheck
cmake --preset Debug

# Build pngcheck
cmake --build build --target pngcheck --preset Debug

# Run all tests (PNGCHECK_EXECUTABLE is set automatically)
cmake --build build --preset Debug --target test-all

# Run tests with verbose output
cmake --build build --preset Debug --target test-verbose

# Run specific test suites
cmake --build build --preset Debug --target test-pngcheck-cli    # CLI tests
cmake --build build --preset Debug --target test-pngcheck-suite  # PNG suite tests

# Run tests with coverage analysis and generate HTML report (requires gcovr)
cmake --build build --preset Debug --target test-coverage

# Clean test artifacts
cmake --build build --preset Debug --target test-clean
```

### Using Ceedling directly

```bash
# Install dependencies
bundle install

# Run all tests (requires pngcheck in PATH or PNGCHECK_EXECUTABLE set)
bundle exec ceedling test:all

# Run specific test suites
bundle exec ceedling test:test_pngcheck_cli   # CLI tests
bundle exec ceedling test:test_pngcheck_suite # PNG suite tests
```


## Running tests in CMake

The project integrates CMake with Ceedling tests such that all test targets are
available via CMake. This allows you to run tests without needing to interact
with Ceedling.


### Available CMake targets

#### Test targets

##### `test-all`

Run all Ceedling tests (alias: `test`).

The JUnit XML test report gets generated at
`build/artifacts/test/junit_tests_report.xml`.

If Ceedling is not installed, the target provides helpful instructions
on how to install it.

Ceedling is a Ruby-based testing framework, so you need to have Ruby and
Bundler installed. You can install Ceedling using Bundler:

```bash
# Ensure you have Ruby installed
ruby --version

# Install Bundler if not already installed
gem install bundler

# Install Ceedling through Bundler
# At the root of the project, run:
bundle install
```

##### `test-verbose`

Run all tests with verbose output.

##### `test-clean`

Clean Ceedling test artifacts (alias: `clean-test`).

##### `test-coverage`

Run tests with coverage analysis and generate HTML report (requires `gcovr`).

When `gcovr` is installed, coverage targets generate both console output and
HTML reports in `build/artifacts/gcov/`. The HTML report is located at
`build/artifacts/gcov/index.html`.

**Note:** Prerequisites need to be satisfied to use this tool (see
[prerequisites](#prerequisites)).

##### `test-pngcheck-cli`

Run CLI-specific tests only.

##### `test-pngcheck-suite`

Run PNG suite tests only.

#### Targets for test case generation

##### `pngsuite-status`

Check the status of PNG suite test files. Shows which files are present and missing.

##### `generate-pngsuite-expectations`

Generate expected output files for PNG suite tests. This target runs pngcheck
against all PNG files in the suite and captures the expected outputs to
`test/expectations/pngsuite/`.

Use this target when pngcheck behavior changes and you need to update the test
expectations.

##### `generate-pngsuite-tests`

Generate the `test/test/test_pngcheck_suite.c` file based on the PNG files and
expectations. This target creates the actual C test functions that will be
executed by the test framework.



### CMake presets

The project includes the following CMake presets:

- **Debug** - Debug build with symbols
- **Release** - Optimized release build

They can be used to configure and build the project easily:

```bash
# Configure with presets
cmake --preset Debug          # Debug build with symbols
cmake --preset Release        # Optimized release build

# Build with presets
cmake --build --preset Debug
cmake --build --preset Release
```

### Integration with Ceedling

A two-file approach is used to bridge the CMake build system with Ceedling's
test framework:

- **`CMakeLists.txt`** - Main CMake configuration file that includes the test subdirectory
- **`test/CMakeLists.txt`** - CMake configuration for the test suite

This integration provides the following features:

- Instead of needing to manually set the `PNGCHECK_EXECUTABLE` environment
variable, the CMake integration automatically locates the built `pngcheck`
executable and sets the environment variable for you.

- CMake automatically manages dependencies, ensuring that the `pngcheck`
executable is built before running tests.

- The CMake targets provide a consistent interface for running tests across
different platforms, without needing to manually invoke Ceedling commands.

- The CMake integration provides clear error messages if dependencies are missing,
such as `ceedling` or `gcovr`, guiding users to install them.


## Test structure

### Test files

- `test/support/test_helpers.c` - Common test functions
- `test/support/test_helpers.h` - Header for common test functions
- `test/test/test_pngcheck_cli.c` - CLI functionality tests (manual)
- `test/test/test_pngcheck_suite.c` - PNG validation tests (auto-generated)

### Directory layout

```
pngcheck/
├── CMakeLists.txt                  # Production build configuration
├── project.yml                     # Ceedling test configuration
├── TESTING.md                      # This documentation
└── test/
    ├── CMakeLists.txt              # CMake test configuration
    ├── bin/                        # Ruby CLI tool for managing tests
    │   └── pngcheck-test           # Ruby script to manage tests and expectations
    ├── lib/                        # Ruby libraries for `pngcheck-test`
    ├── test/                       # Test files
    │   ├── test_pngcheck_cli.c     # CLI tests (manual)
    │   ├── test_pngcheck_suite.c   # PNG suite tests (generated by test/bin/pngcheck-test)
    │   └── support/                # Test helpers
    ├── fixtures/pngsuite/          # PNG test files (176 files)
    └── expectations/pngsuite/      # Expected outputs
```

## Test approach

Tests validate pngcheck behavior using **simple prefix matching and exit codes**:

- **Exit code**: 0 for valid PNGs, non-zero for errors
- **Output prefix**: "OK:" for valid files, "ERROR:" for invalid files

This approach focuses on core functionality rather than exact output matching.

### CLI tests

Static tests for command-line options:
- Help option (`-h`)
- File handling (valid, invalid, nonexistent)
- Output options (`-v`, `-q`, `-t`, `-p`, `-c`, `-7`)

### PNG suite tests

Auto-generated tests using the [PngSuite](http://www.schaik.com/pngsuite/) collection:
- **Valid PNGs**: Various formats, bit depths, interlacing
- **Invalid PNGs**: Corrupted headers, bad checksums, truncated files

**Note:** The PngSuite files are committed to the repository at
`test/fixtures/pngsuite/`, where its LICENSE also resides.


## Test management

The test suite includes a Ruby-based management tool.

**Note:** Prerequisites need to be satisfied to use this tool (see
[prerequisites](#prerequisites)).

To manage tests, run the Ruby CLI tool:

```bash
# Check status
bundle exec test/bin/pngcheck-test status

# Regenerate expectations (when pngcheck behavior changes) (add --force to overwrite)
bundle exec test/bin/pngcheck-test expectations

# Generate test/test/test_pngcheck_suite.c
bundle exec test/bin/pngcheck-test generate
```

**Note:**: On Windows the command would be `bundle exec ruby
test/bin/pngcheck-test` instead of `bundle exec test/bin/pngcheck-test` as the
Ruby script is not executable on Windows.

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


## CI troubles

### Windows CI (windows-2022, windows-11-arm)

For some reason, the CMake custom target (in `test/CMakeList.txt`) fails to find
the `test/bin/pngcheck-test` script on Windows. Yet it works on Ubuntu and macOS.

In addition, Windows builds the `pngcheck` executable under the
`build/{preset-name}/{preset-name}/` directory, while on Linux and macOS it is
under `build/{preset-name}/`.

As a result, the `build.yml` workflow does not use CMake to run tests:

- directly runs the `test/bin/pngcheck-test` script and uses the
`PNGCHECK_EXECUTABLE` environment variable to point to the built `pngcheck`
executable.

- directly uses the `ceedling` command to run tests instead of the CMake targets:
`cmake --build build --preset Debug --target test-all`.



## Troubleshooting

**pngcheck not found**: Build with `make` or `cmake`
**Ruby issues**: Run `bundle install`


## Acknowledgements

The PngSuite test images are created by
[Willem van Schaik](http://www.schaik.com/pngsuite/), and distributed under its
original license reproduced at `test/fixtures/pngsuite/PngSuite.LICENSE`.

We thank Willem van Schaik for providing these test images that are essential
in validating the behavior of `pngcheck`.
