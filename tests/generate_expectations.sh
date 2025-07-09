#!/bin/bash
# Script to generate pngcheck expectations for all PNG files in the test suite
# These expectations will be committed to git as the "golden standard"

set -e

echo "Generating pngcheck expectations..."

# Source the helper functions
source helpers/test_helpers.bash

# Ensure we have the fixtures
echo "Ensuring PngSuite fixtures are available..."
download_pngsuite_if_needed

# Check if pngcheck executable exists, if not try to build it
pngcheck_exe=$(get_pngcheck_executable 2>/dev/null) || {
    echo "pngcheck executable not found. Attempting to build..."

    if [[ -f "../CMakeLists.txt" ]]; then
        echo "Building with CMake..."
        cd .. && cmake -B build && cmake --build build && cd tests
    elif [[ -f "../Makefile" ]]; then
        echo "Building with Make..."
        cd .. && make && cd tests
    else
        echo "Error: No build system found (CMakeLists.txt or Makefile)"
        exit 1
    fi

    # Try to get executable again
    pngcheck_exe=$(get_pngcheck_executable) || {
        echo "Error: Failed to build or find pngcheck executable"
        exit 1
    }
}

echo "Using pngcheck executable: $pngcheck_exe"

# Create expectations directory
mkdir -p expectations/pngsuite

# Generate expectations for all PNG files
total_files=0
generated_files=0

echo "Generating expectations for PNG files..."

for png_file in fixtures/pngsuite/*.png; do
    if [[ -f "$png_file" ]]; then
        total_files=$((total_files + 1))
        basename=$(basename "$png_file")
        output_file="expectations/pngsuite/${basename}.out"

        echo "Processing: $basename"

        # Run pngcheck and capture output (both stdout and stderr)
        # We use || true to continue even if pngcheck returns non-zero (expected for corrupted files)
        "$pngcheck_exe" "$png_file" > "$output_file" 2>&1 || true

        generated_files=$((generated_files + 1))
    fi
done

echo ""
echo "Expectation generation complete!"
echo "Generated expectations for $generated_files/$total_files PNG files"
echo "Expectations saved in: tests/expectations/pngsuite/"
echo ""
echo "Next steps:"
echo "1. Review the generated expectations in tests/expectations/pngsuite/"
echo "2. Commit the expectations to git:"
echo "   git add tests/expectations/"
echo "   git commit -m 'Add pngcheck test expectations'"
echo "3. Run tests with: bats tests/test_pngcheck.bats"
