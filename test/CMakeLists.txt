cmake_minimum_required(VERSION 3.14)

project(pngcheck-tests)

# Find Ruby executable
find_program(RUBY_EXECUTABLE NAMES ruby)

if(NOT RUBY_EXECUTABLE)
    message(FATAL_ERROR "Ruby executable not found.")
endif()

# Check if bundler is available
execute_process(COMMAND ${RUBY_EXECUTABLE} -e "require 'bundler'"
                RESULT_VARIABLE BUNDLER_CHECK_RESULT
                OUTPUT_QUIET
                ERROR_QUIET)

if(BUNDLER_CHECK_RESULT EQUAL 0)
    message(STATUS "Found Ruby with Bundler support")
    message(STATUS "Using bundler to manage Ceedling dependency")
else()
    message(STATUS "Bundler not available, trying to find Ceedling directly")

    # Find ceedling
    find_program(CEEDLING_COMMAND ceedling)

    if(NOT CEEDLING_COMMAND)
        message(FATAL_ERROR "Ceedling not found and Bundler not available. Please install Ceedling or use bundler.")
    else()
        message(STATUS "Found Ceedling at: ${CEEDLING_COMMAND}")
    endif()
endif()

# ceedling clean
add_custom_target(test-clean
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning Ceedling artifacts..."
    COMMAND ceedling clobber || true
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning Ceedling build artifacts completed."
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Removing Ceedling build artifacts"
)

# Get the path to the pngcheck executable built by CMake
get_target_property(PNGCHECK_EXECUTABLE_PATH pngcheck RUNTIME_OUTPUT_DIRECTORY)
if(NOT PNGCHECK_EXECUTABLE_PATH)
    set(PNGCHECK_EXECUTABLE_PATH ${CMAKE_BINARY_DIR})
endif()
set(PNGCHECK_EXECUTABLE_FULL_PATH ${PNGCHECK_EXECUTABLE_PATH}/pngcheck)

# ceedling test:all
add_custom_target(test-all
    COMMAND ${CMAKE_COMMAND} -E env PNGCHECK_EXECUTABLE=${PNGCHECK_EXECUTABLE_FULL_PATH} ceedling test:all
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS pngcheck pngsuite-download
    COMMENT "Running all unit tests"
)

# ceedling test:all with verbose output
add_custom_target(test-verbose
    COMMAND ${CMAKE_COMMAND} -E env PNGCHECK_EXECUTABLE=${PNGCHECK_EXECUTABLE_FULL_PATH} ceedling test:all --verbosity=obnoxious
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS pngcheck
    COMMENT "Running all unit tests with verbose output"
)

# Check for gcovr availability
find_program(GCOVR_EXECUTABLE gcovr)

# ceedling gcov:all for coverage report
if(GCOVR_EXECUTABLE)
    add_custom_target(test-coverage
        COMMAND ${CMAKE_COMMAND} -E env PNGCHECK_EXECUTABLE=${PNGCHECK_EXECUTABLE_FULL_PATH} ceedling gcov:all
        COMMAND ${GCOVR_EXECUTABLE} -r ${CMAKE_SOURCE_DIR} --html --html-details -o ${CMAKE_SOURCE_DIR}/build/artifacts/gcov/index.html
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck
        COMMENT "Running tests with coverage analysis and generating HTML report"
    )
else()
    # Fallback target that provides helpful error message
    add_custom_target(test-coverage
        COMMAND ${CMAKE_COMMAND} -E echo "ERROR: gcovr not found. Install with: pip install gcovr"
        COMMAND ${CMAKE_COMMAND} -E echo "Alternative: Run 'ceedling gcov:all' directly for basic coverage"
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "Coverage analysis requires gcovr"
    )
endif()


# Individual test targets (examples - add more as needed)
add_custom_target(test-pngcheck-cli
    COMMAND ${CMAKE_COMMAND} -E env PNGCHECK_EXECUTABLE=${PNGCHECK_EXECUTABLE_FULL_PATH} ceedling test:test_pngcheck_cli
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS pngcheck
    COMMENT "Running pngcheck CLI tests"
)

add_custom_target(test-pngcheck-suite
    COMMAND ${CMAKE_COMMAND} -E env PNGCHECK_EXECUTABLE=${PNGCHECK_EXECUTABLE_FULL_PATH} ceedling test:test_pngcheck_suite
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS pngcheck
    COMMENT "Running pngcheck suite tests"
)

# PNG Suite utility targets
add_custom_target(pngsuite-status
    COMMAND bundle exec test/bin/pngcheck-test status
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Check PNG suite status"
)

add_custom_target(pngsuite-download
    COMMAND bundle exec test/bin/pngcheck-test download
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Download PNG test files"
)

add_custom_target(generate-pngsuite-expectations
    COMMAND ${CMAKE_COMMAND} -E env PNGCHECK_EXECUTABLE=${PNGCHECK_EXECUTABLE_FULL_PATH} bundle exec test/bin/pngcheck-test expectations
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS pngcheck
    COMMENT "Generate PNG suite expectations"
)

add_custom_target(generate-pngsuite-tests
    COMMAND bundle exec test/bin/pngcheck-test generate
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generate PNG suite test files"
)

# Print test information
message(STATUS "")
message(STATUS "=== Ceedling Test Configuration ===")
if(CEEDLING_COMMAND)
    message(STATUS "Ceedling found at: ${CEEDLING_COMMAND}")
else()
    message(STATUS "Ceedling will be installed automatically")
endif()
message(STATUS "")
message(STATUS "Available test targets:")
message(STATUS "  test-all                  - Run all Ceedling tests")
message(STATUS "  test-verbose              - Run all tests with verbose output")
message(STATUS "  test-clean                - Clean all test build artifacts")
message(STATUS "  test-coverage             - Run tests with coverage analysis and generate HTML report")
message(STATUS "  test-pngcheck-cli         - Run CLI-specific tests")
message(STATUS "  test-pngcheck-suite       - Run suite-specific tests")
message(STATUS "")
message(STATUS "Available PNG suite utility targets:")
message(STATUS "  pngsuite-status           - Check PNG suite status")
message(STATUS "  pngsuite-download         - Download PNG test files")
message(STATUS "  generate-pngsuite-expectations - Generate PNG suite expectations")
message(STATUS "  generate-pngsuite-tests   - Generate PNG suite test files")
message(STATUS "")
