# pngcheck test suite

This directory contains the test suite for the `pngcheck` utility, which is used
to guard against regressions on PNG validation.

The test files are sourced from the [PngSuite](http://www.schaik.com/pngsuite/)
collection, which provides a comprehensive set of PNG test files designed to
test the robustness and compliance of PNG processing tools.

The test framework uses [Bats](https://github.com/bats-core/bats-core) (Bash
Automated Testing System) to run tests against the `pngcheck` executable using the
PngSuite test collection.

## Overview

The pngcheck test suite is organized into a modular structure with
separate test files for different purposes.

- **CLI tests**: Static tests for command-line interface functionality
- **PNG suite tests**: Auto-generated tests using the PngSuite image test suite
- **Helper functions**: Reusable test utilities and assertions
- **Test runners**: Scripts to execute tests with various options

## Test structure

### Test files

| File | Purpose |  Maintenance |
|------|---------|-------------|
| `test_pngcheck_cli.bats` | CLI functionality  | Manual |
| `test_pngcheck_suite.bats` | PNG validation | Auto-generated |

### Support files

| File | Purpose |
|------|---------|
| `helpers/test_helpers.bash` | Common test functions |
| `libs/bats-assert/` | Assertion library |
| `generate_test_cases.sh` | PNG suite test generator |
| `generate_expectations.sh` | Expected output generator |
| `run_tests.sh` | Main test runner |

### Directory structure

```
tests/
├── README.md                   # This documentation
├── test_pngcheck_cli.bats      # CLI functionality tests
├── test_pngcheck_suite.bats    # PNG suite tests (auto-generated)
├── generate_test_cases.sh      # Generate PNG suite tests
├── generate_expectations.sh    # Regenerate expected outputs
├── run_tests.sh                # Main test runner with options
├── helpers/
│   └── test_helpers.bash       # Test helper functions
├── libs/
│   └── bats-assert/            # Bats assertion library (git submodule)
├── fixtures/                   # Test PNG files
│   └── pngsuite/               # PngSuite PNG files (auto-downloaded)
└── expectations/
    └── pngsuite/               # Expected pngcheck outputs
        ├── basi0g01.png.out
        ├── basi0g02.png.out
        └── ...
```

## Quick start

### Prerequisites

**Install Bats:**
```bash
# macOS
brew install bats-core

# Ubuntu/Debian
sudo apt install bats

# Manual installation
git clone https://github.com/bats-core/bats-core.git
cd bats-core && ./install.sh /usr/local
```

**Initialize submodules:**
```bash
git submodule update --init --recursive
```

**Configure pngcheck executable:**

The test suite needs to know where to find the pngcheck executable. You can either:

1. **Install pngcheck in your PATH** (recommended for local development):
   ```bash
   # Build and install pngcheck
   make && sudo make install
   ```

2. **Set the PNGCHECK_EXECUTABLE environment variable**:
   ```bash
   # For Makefile builds
   export PNGCHECK_EXECUTABLE=./pngcheck

   # For CMake builds
   export PNGCHECK_EXECUTABLE=./build/pngcheck

   # For Windows builds
   export PNGCHECK_EXECUTABLE=./pngcheck.exe
   ```

### Running tests

```bash
# Run all tests (CLI + PNG suite, 198 tests total)
./tests/run_tests.sh

# Run only CLI tests (fast, 22 tests)
./tests/run_tests.sh --cli-only

# Run only PNG suite tests (comprehensive, 176 tests)
./tests/run_tests.sh --suite-only

# Verbose output for debugging
./tests/run_tests.sh --verbose

# Help and options
./tests/run_tests.sh --help
```

### Direct Bats usage

```bash
# Run specific test files
bats tests/test_pngcheck_cli.bats
bats tests/test_pngcheck_suite.bats

# Run all test files
bats tests/test_pngcheck_*.bats

# Verbose mode
bats --verbose-run tests/test_pngcheck_cli.bats
```

## Test categories

### CLI tests

Static tests for command-line interface behavior (manually maintained):

- **Basic functionality**: executable exists, version info, help
- **Error handling**: non-existent files, invalid options, empty arguments
- **Command-line options**: `-q`, `-v`, `-vv`, `-t`, `-7`, `-p`, `-c`, `-w`, `-s`, `-x`
- **Multiple files**: testing with multiple PNG files
- **Combined options**: testing option combinations like `-vt`, `-qp`
- **Edge cases**: directory input, binary files, invalid extensions

### PNG suite tests

Auto-generated tests for the PNG test suite:

**Valid PNGs (162 images)**: Files that should pass validation

- Basic formats: grayscale, RGB, palette, alpha
- Bit depths: 1, 2, 4, 8, 16 bits
- Interlacing: Adam7 and non-interlaced
- Compression levels, filter types, gamma correction

**Invalid PNGs (14 images)**: Files with intentional errors

- Corrupted headers, invalid chunk data, bad CRC checksums
- Truncated files, invalid color types, malformed critical chunks

Each test is categorized as `[VALID]`, `[INVALID]`, or `[WARNING]` based on
expected output.

## Test data

### PNG test suite

The tests use the official PngSuite images from Willem van Schaik.


### Expected outputs

Pre-generated expected outputs for each PNG file:

- **Location**: `tests/expectations/pngsuite/`
- **Format**: `.out` files containing pngcheck output
- **Generation**: Created by `generate_expectations.sh`

## Test generation

### Auto-generated PNG suite tests

The PNG suite tests are automatically generated by scanning expected output files:

```bash
# Regenerate PNG suite tests
./tests/generate_test_cases.sh
```

This process:
1. Scans `tests/expectations/pngsuite/` for `.out` files
2. Determines test category (VALID/INVALID) from expected output
3. Generates individual test cases for each PNG file
4. Creates `tests/test_pngcheck_suite.bats` with all tests

### Expected output generation

Expected outputs can be regenerated when pngcheck behavior changes:

```bash
# Regenerate expected outputs
./tests/generate_expectations.sh
```

This creates `.out` files containing the current pngcheck output for each PNG file.

**Important**: Only run this script when you intentionally want to update the
expected test outputs. Always review the changes with
`git diff tests/expectations/` before committing.

## Test output examples

```
=== pngcheck Test Suite ===

Running CLI tests...
✓ CLI tests passed

Running PNG suite tests...
✓ PNG suite tests passed

=== All tests passed! ===
```

Valid PNG example:

```
OK: tests/fixtures/pngsuite/basn0g01.png (32x32, 1-bit grayscale, non-interlaced, -28.1%).
```

Invalid PNG example:

```
tests/fixtures/pngsuite/xc1n0g08.png  invalid IHDR image type (1)
ERROR: tests/fixtures/pngsuite/xc1n0g08.png
```


## Adding new tests

### Adding CLI tests

Edit `tests/test_pngcheck_cli.bats`:

```bash
@test "my new test" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" --new-option
    assert_success
    assert_output --partial "expected text"
}
```

### Adding PNG files

1. Add PNG files to `tests/fixtures/pngsuite/`
2. Generate expected outputs: `./tests/generate_expectations.sh`
3. Regenerate test cases: `./tests/generate_test_cases.sh`



## Debug commands

```bash
# Verbose test output
./tests/run_tests.sh --verbose

# Individual test debugging
bats --verbose-run --print-output-on-failure tests/test_pngcheck_cli.bats

# Manual PNG testing
./pngcheck tests/fixtures/pngsuite/basn0g01.png

# Compare outputs
./pngcheck tests/fixtures/pngsuite/basn0g01.png > actual.out
diff actual.out tests/expectations/pngsuite/basn0g01.png.out
```


## References

- [Bats documentation](https://bats-core.readthedocs.io/)
- [PngSuite test collection](http://www.schaik.com/pngsuite/)
- [PNG specification](https://www.w3.org/TR/PNG/)
