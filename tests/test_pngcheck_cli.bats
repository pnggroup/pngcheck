#!/usr/bin/env bats
# Static CLI tests for pngcheck functionality
# These tests are manually maintained and test command-line interface behavior

load 'helpers/test_helpers'
load 'libs/bats-support/load.bash'
load 'libs/bats-assert/load.bash'

setup_file() {
    download_pngsuite_if_needed
}

setup() {
    # Ensure we have a working pngcheck executable before each test
    local pngcheck_exe
    pngcheck_exe=$(get_pngcheck_executable) || skip "pngcheck executable not found"
}

@test "pngcheck executable exists" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" -h
    assert_success
    assert_output --partial "pngcheck"
}

@test "pngcheck handles non-existent files" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" "nonexistent.png"
    assert_failure
}

@test "pngcheck handles invalid file extension" {
    local pngcheck_exe=$(get_pngcheck_executable)
    echo "not a png" > /tmp/test.txt
    run "$pngcheck_exe" "/tmp/test.txt"
    assert_failure
    rm -f /tmp/test.txt
}

@test "pngcheck quiet mode (-q)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-q" "fixtures/pngsuite/basn0g01.png"
        assert_success
        # Quiet mode should produce minimal output for valid files
        [[ ${#lines[@]} -le 2 ]]
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck verbose mode (-v)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-v" "fixtures/pngsuite/basn0g01.png"
        assert_success
        assert_output --partial "chunk"
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck very verbose mode (-vv)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-vv" "fixtures/pngsuite/basn0g01.png"
        assert_success
        # Very verbose should show filter information
        assert_output --partial "filter"
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck text chunk display (-t)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-t" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck 7-bit text display (-7)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-7" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck palette display (-p)" {
    if [[ -f "fixtures/pngsuite/basn3p01.png" ]]; then
        run_pngcheck_with_options "-p" "fixtures/pngsuite/basn3p01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck colorized output (-c)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-c" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck windowBits suppression (-w)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-w" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck multiple files" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" && -f "fixtures/pngsuite/basn0g02.png" ]]; then
        run_pngcheck_with_options "" "fixtures/pngsuite/basn0g01.png" "fixtures/pngsuite/basn0g02.png"
        assert_success
        # Should contain output for both files
        assert_output --partial "basn0g01.png"
        assert_output --partial "basn0g02.png"
    else
        skip "Test PNG files not available"
    fi
}

@test "pngcheck combined options (-vt)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-vt" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck combined options (-qp)" {
    if [[ -f "fixtures/pngsuite/basn3p01.png" ]]; then
        run_pngcheck_with_options "-qp" "fixtures/pngsuite/basn3p01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck invalid option" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" "-z"
    assert_failure
}

@test "pngcheck help option (-h)" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" -h
    assert_success
    assert_output --partial "Usage:"
}

@test "pngcheck handles directory input" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" "tests/"
    assert_failure
}

@test "pngcheck handles binary file" {
    local pngcheck_exe=$(get_pngcheck_executable)
    run "$pngcheck_exe" "$pngcheck_exe"
    assert_failure
}

@test "pngcheck search mode (-s)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-s" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}

@test "pngcheck extract mode (-x)" {
    if [[ -f "fixtures/pngsuite/basn0g01.png" ]]; then
        run_pngcheck_with_options "-x" "fixtures/pngsuite/basn0g01.png"
        assert_success
    else
        skip "Test PNG file not available"
    fi
}
