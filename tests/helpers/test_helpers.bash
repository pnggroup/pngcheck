#!/bin/bash

# Test helper functions for pngcheck testing

# Get the correct pngcheck executable path
get_pngcheck_executable() {
    # Use environment variable if set
    if [[ -n "$PNGCHECK_EXECUTABLE" ]]; then
        # Check if the path exists and is executable
        if [[ -f "$PNGCHECK_EXECUTABLE" && -x "$PNGCHECK_EXECUTABLE" ]]; then
            echo "$PNGCHECK_EXECUTABLE"
            return 0
        elif [[ -f "$PNGCHECK_EXECUTABLE" ]]; then
            echo "Error: PNGCHECK_EXECUTABLE '$PNGCHECK_EXECUTABLE' exists but is not executable" >&2
            return 1
        else
            echo "Error: PNGCHECK_EXECUTABLE '$PNGCHECK_EXECUTABLE' does not exist" >&2
            echo "Current directory: $(pwd)" >&2
            echo "Directory contents:" >&2
            ls -la "$(dirname "$PNGCHECK_EXECUTABLE")" 2>/dev/null || echo "Directory does not exist" >&2
            return 1
        fi
    fi

    # Try common build locations FIRST (prioritize local builds over system PATH)
    local build_paths=(
        "../pngcheck"
        "./pngcheck"
        "./build/pngcheck"
        "./build/pngcheck.exe"
        "./build/Release/pngcheck.exe"
        "./build-cmake/pngcheck"
    )

    # Get the absolute path to the project root from the test directory
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local project_root="$(cd "$script_dir/../.." && pwd)"
    local absolute_pngcheck="$project_root/pngcheck"

    # Check the absolute path first
    if [[ -f "$absolute_pngcheck" && -x "$absolute_pngcheck" ]]; then
        echo "$absolute_pngcheck"
        return 0
    fi

    for path in "${build_paths[@]}"; do
        if [[ -f "$path" && -x "$path" ]]; then
            echo "$path"
            return 0
        fi
    done

    # Only try PATH as a fallback if no local builds are found
    if command -v pngcheck >/dev/null 2>&1; then
        echo "pngcheck"
        return 0
    fi

    echo "Error: pngcheck executable not found. Please either:" >&2
    echo "  1. Build pngcheck in the current directory, or" >&2
    echo "  2. Set PNGCHECK_EXECUTABLE environment variable to the path of pngcheck binary, or" >&2
    echo "  3. Install pngcheck in your PATH" >&2
    echo "Current directory: $(pwd)" >&2
    echo "Searched paths:" >&2
    for path in "${build_paths[@]}"; do
        if [[ -f "$path" ]]; then
            echo "  $path: exists but not executable" >&2
        else
            echo "  $path: does not exist" >&2
        fi
    done
    return 1
}

# Run pngcheck on a file and capture output
run_pngcheck() {
    local png_file="$1"
    local pngcheck_exe
    pngcheck_exe=$(get_pngcheck_executable) || return 1

    run "$pngcheck_exe" "$png_file"
}

# Download PngSuite fixtures if not present
download_pngsuite_if_needed() {
    # Get the directory where this script is located
    local script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    local fixtures_dir="$script_dir/../fixtures/pngsuite"
    local pngsuite_url="http://www.schaik.com/pngsuite2011/PngSuite-2017jul19.zip"

    echo "Debug: script_dir=$script_dir"
    echo "Debug: fixtures_dir=$fixtures_dir"

    # Check if fixtures already exist and have content
    if [[ -d "$fixtures_dir" ]] && [[ -n "$(ls -A "$fixtures_dir"/*.png 2>/dev/null)" ]]; then
        local existing_count=$(ls -1 "$fixtures_dir"/*.png 2>/dev/null | wc -l)
        echo "PngSuite fixtures already present ($existing_count PNG files)"
        return 0
    fi

    echo "Downloading PngSuite test fixtures..."
    mkdir -p "$fixtures_dir"

    # Change to a temporary directory for download
    local temp_dir=$(mktemp -d)
    local original_dir=$(pwd)
    echo "Debug: temp_dir=$temp_dir"
    echo "Debug: original_dir=$original_dir"
    cd "$temp_dir"

    # Download with platform-appropriate tool
    echo "Debug: Downloading from $pngsuite_url"
    if command -v curl >/dev/null; then
        echo "Debug: Using curl for download"
        if ! curl -L "$pngsuite_url" -o pngsuite.zip; then
            echo "Error: curl download failed" >&2
            cd "$original_dir"
            rm -rf "$temp_dir"
            return 1
        fi
    elif command -v wget >/dev/null; then
        echo "Debug: Using wget for download"
        if ! wget "$pngsuite_url" -O pngsuite.zip; then
            echo "Error: wget download failed" >&2
            cd "$original_dir"
            rm -rf "$temp_dir"
            return 1
        fi
    else
        echo "Error: Neither curl nor wget available" >&2
        cd "$original_dir"
        rm -rf "$temp_dir"
        return 1
    fi

    # Check if download was successful
    if [[ ! -f pngsuite.zip ]]; then
        echo "Error: Failed to download PngSuite" >&2
        echo "Debug: Contents of temp directory:" >&2
        ls -la >&2
        cd "$original_dir"
        rm -rf "$temp_dir"
        return 1
    fi

    local zip_size=$(stat -c%s pngsuite.zip 2>/dev/null || stat -f%z pngsuite.zip 2>/dev/null || echo "unknown")
    echo "Debug: Downloaded zip file size: $zip_size bytes"

    # Extract to temporary location first
    if command -v unzip >/dev/null; then
        echo "Debug: Extracting zip file"
        if ! unzip -q pngsuite.zip; then
            echo "Error: Failed to extract zip file" >&2
            echo "Debug: Zip file contents:" >&2
            unzip -l pngsuite.zip >&2 || echo "Cannot list zip contents" >&2
            cd "$original_dir"
            rm -rf "$temp_dir"
            return 1
        fi
    else
        echo "Error: unzip not available" >&2
        cd "$original_dir"
        rm -rf "$temp_dir"
        return 1
    fi

    echo "Debug: Contents after extraction:"
    find . -type f -name "*.png" | head -10

    # Find PNG files and copy them to fixtures directory
    # The zip might contain subdirectories, so we need to find all PNG files
    local png_files_found=$(find . -name "*.png" | wc -l)
    echo "Debug: Found $png_files_found PNG files in archive"

    if [[ $png_files_found -eq 0 ]]; then
        echo "Error: No PNG files found in extracted archive" >&2
        echo "Debug: All extracted contents:" >&2
        find . -type f | head -20 >&2
        cd "$original_dir"
        rm -rf "$temp_dir"
        return 1
    fi

    find . -name "*.png" -exec cp {} "$fixtures_dir/" \;

    # Verify we got some PNG files
    local png_count=$(ls -1 "$fixtures_dir"/*.png 2>/dev/null | wc -l)
    if [[ $png_count -eq 0 ]]; then
        echo "Error: No PNG files copied to fixtures directory" >&2
        echo "Debug: Fixtures directory contents:" >&2
        ls -la "$fixtures_dir" >&2
        cd "$original_dir"
        rm -rf "$temp_dir"
        return 1
    fi

    # Clean up
    cd "$original_dir"
    rm -rf "$temp_dir"
    echo "PngSuite fixtures downloaded successfully ($png_count PNG files)"
}

# Compare actual output with expected output file
assert_output_matches_file() {
    local expected_file="$1"
    local expected_content
    local filtered_output
    local filtered_expected

    if [[ ! -f "$expected_file" ]]; then
        echo "Expected output file not found: $expected_file" >&2
        return 1
    fi

    expected_content=$(cat "$expected_file")

    # Filter out zlib version warnings and subsequent empty lines from both actual and expected output
    filtered_output=$(echo "$output" | sed '/^zlib warning:/d' | sed '/^$/d')
    filtered_expected=$(echo "$expected_content" | sed '/^zlib warning:/d' | sed '/^$/d')

    # Normalize "static, " differences - remove "static, " from both outputs to handle
    # compilation differences where animated PNG support may or may not be included
    filtered_output=$(echo "$filtered_output" | sed 's/, static,/,/g')
    filtered_expected=$(echo "$filtered_expected" | sed 's/, static,/,/g')

    # Normalize trailing periods - remove trailing periods from both outputs to handle
    # version differences where newer pngcheck versions may include trailing periods
    filtered_output=$(echo "$filtered_output" | sed 's/\.$//')
    filtered_expected=$(echo "$filtered_expected" | sed 's/\.$//')

    if [[ "$filtered_output" != "$filtered_expected" ]]; then
        echo "Output mismatch for $(basename "$expected_file" .out):" >&2
        echo "Expected: $filtered_expected" >&2
        echo "Actual: $filtered_output" >&2
        return 1
    fi
}

# Test a PNG file against its expected output (helper for generated tests)
test_png_file() {
    local png_file="$1"
    local expected_file="$2"

    if [[ -f "$png_file" && -f "$expected_file" ]]; then
        run_pngcheck "$png_file"
        assert_output_matches_file "$expected_file"
    else
        skip "Test files not available"
    fi
}

# Run pngcheck with specific options and capture output
run_pngcheck_with_options() {
    local options="$1"
    shift
    local files=("$@")
    local pngcheck_exe
    pngcheck_exe=$(get_pngcheck_executable) || return 1

    if [[ -n "$options" ]]; then
        run "$pngcheck_exe" $options "${files[@]}"
    else
        run "$pngcheck_exe" "${files[@]}"
    fi
}
