cmake_minimum_required(VERSION 3.14)

project(pngcheck-tests)

# Find ceedling
find_program(CEEDLING_COMMAND ceedling)

if(NOT CEEDLING_COMMAND)
    # Ceedling not found
    message(STATUS "Ceedling not found. Attempting to install...")

    # Install ceedling
    execute_process(COMMAND gem install ceedling
                    RESULT_VARIABLE CEEDLING_INSTALL_RESULT
                    OUTPUT_VARIABLE CEEDLING_INSTALL_OUTPUT
                    ERROR_VARIABLE CEEDLING_INSTALL_ERROR)

    # Check if installation was successful
    if(CEEDLING_INSTALL_RESULT EQUAL 0)
        message(STATUS "Ceedling installed successfully.")
    else()
        message(FATAL_ERROR "Failed to install Ceedling: ${CEEDLING_INSTALL_ERROR}")
    endif()
else()
    # Get Ruby executable
    find_program(RUBY_EXECUTABLE NAMES ruby)

    if(NOT RUBY_EXECUTABLE)
        message(FATAL_ERROR "Ruby executable not found.")
    endif()

    # Build full ceedling command
    set(CEEDLING_FULL_COMMAND "${RUBY_EXECUTABLE}" "${CEEDLING_COMMAND}")

    # Get Ceedling version
    execute_process(COMMAND ${CEEDLING_COMMAND} version
                    RESULT_VARIABLE CEEDLING_VERSION_RESULT
                    OUTPUT_VARIABLE CEEDLING_VERSION_OUTPUT
                    ERROR_VARIABLE CEEDLING_VERSION_ERROR)

    if(CEEDLING_VERSION_RESULT EQUAL 0)
        message(STATUS "Found Ceedling: ${CEEDLING_VERSION_OUTPUT}")
    else()
        message(STATUS "Ceedling found at: ${CEEDLING_COMMAND}")
        message(STATUS "Note: Version check failed, but Ceedling should work")
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
    DEPENDS pngcheck
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
