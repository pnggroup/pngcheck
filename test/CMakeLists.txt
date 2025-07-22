cmake_minimum_required(VERSION 3.14)

project(pngcheck-tests)

# Find Ruby executable
find_program(RUBY_EXECUTABLE NAMES ruby)

if(NOT RUBY_EXECUTABLE)
    message(WARNING "Ruby executable not found. Test targets will be disabled.")
    set(TESTING_ENABLED FALSE)
else()
    # Check if bundler is available
    execute_process(COMMAND ${RUBY_EXECUTABLE} -e "require 'bundler'"
                    RESULT_VARIABLE BUNDLER_CHECK_RESULT
                    OUTPUT_QUIET
                    ERROR_QUIET)

    if(BUNDLER_CHECK_RESULT EQUAL 0)
        message(STATUS "Found Ruby with Bundler support")
        message(STATUS "Using bundler to manage Ceedling dependency")
        set(TESTING_ENABLED TRUE)
    else()
        message(STATUS "Bundler not available, trying to find Ceedling directly")

        # Find ceedling
        find_program(CEEDLING_COMMAND ceedling)

        if(NOT CEEDLING_COMMAND)
            message(WARNING "Ceedling not found and Bundler not available. Test targets will be disabled.")
            message(STATUS "To enable testing, install Ceedling or use bundler.")
            set(TESTING_ENABLED FALSE)
        else()
            message(STATUS "Found Ceedling at: ${CEEDLING_COMMAND}")
            set(TESTING_ENABLED TRUE)
        endif()
    endif()
endif()

if(TESTING_ENABLED)
    # Determine the correct ceedling command to use
    if(BUNDLER_CHECK_RESULT EQUAL 0)
        # Use bundler to run ceedling and test utilities
        set(CEEDLING_CMD bundle exec ceedling)
        set(TESTUTIL_CMD bundle exec ruby test/bin/pngcheck-test)
    else()
        set(CEEDLING_CMD ceedling)
        set(TESTUTIL_CMD ruby test/bin/pngcheck-test)
    endif()

    # ceedling clean
    add_custom_target(test-clean
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning Ceedling artifacts..."
        COMMAND ${CEEDLING_CMD} clobber || true
        COMMAND ${CMAKE_COMMAND} -E echo "Cleaning Ceedling build artifacts completed."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Removing Ceedling build artifacts"
    )

    # ceedling test:all
    add_custom_target(test-all
        COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${CEEDLING_CMD} test:all
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck generate-pngsuite-tests
        COMMENT "Running all unit tests"
    )

    # ceedling test:all with verbose output
    add_custom_target(test-verbose
        COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${CEEDLING_CMD} test:all --verbosity=obnoxious
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck generate-pngsuite-tests
        COMMENT "Running all unit tests with verbose output"
    )

    # Check for gcovr availability
    find_program(GCOVR_EXECUTABLE gcovr)

    # ceedling gcov:all for coverage report
    if(GCOVR_EXECUTABLE)
        add_custom_target(test-coverage
            COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${CEEDLING_CMD} gcov:all
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_SOURCE_DIR}/build/artifacts/gcov
            COMMAND ${GCOVR_EXECUTABLE} -r ${CMAKE_SOURCE_DIR} --html --html-details -o ${CMAKE_SOURCE_DIR}/build/artifacts/gcov/index.html
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS pngcheck generate-pngsuite-tests
            COMMENT "Running tests with coverage analysis and generating HTML report"
        )
    else()
        # Fallback target that provides helpful error message
        add_custom_target(test-coverage
            COMMAND ${CMAKE_COMMAND} -E echo "ERROR: gcovr not found. Install with: pip install gcovr"
            COMMAND ${CMAKE_COMMAND} -E echo "Alternative: Run '${CEEDLING_CMD} gcov:all' directly for basic coverage"
            COMMAND ${CMAKE_COMMAND} -E false
            COMMENT "Coverage analysis requires gcovr"
        )
    endif()

    # Individual test targets
    add_custom_target(test-pngcheck-suite
        COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${CEEDLING_CMD} test:test_pngcheck_suite
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck generate-pngsuite-tests
        COMMENT "Running pngcheck suite tests"
    )

    # PNG Suite utility targets
    add_custom_target(pngsuite-status
        COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${TESTUTIL_CMD} status
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck
        COMMENT "Check PNG suite status"
    )

    add_custom_target(generate-pngsuite-expectations
        COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${TESTUTIL_CMD} expectations
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck
        COMMENT "Generate PNG suite expectations"
    )

    add_custom_target(generate-pngsuite-tests
        COMMAND ${CMAKE_COMMAND} -E env "PNGCHECK_EXECUTABLE=$<TARGET_FILE:pngcheck>" ${TESTUTIL_CMD} generate
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS pngcheck
        COMMENT "Generate PNG suite test files"
    )
else()
    # Create dummy targets when testing is disabled
    add_custom_target(test-all
        COMMAND ${CMAKE_COMMAND} -E echo "Testing is disabled. Ruby/Ceedling not available."
        COMMENT "Testing disabled - Ruby/Ceedling not found"
    )

    add_custom_target(test-clean
        COMMAND ${CMAKE_COMMAND} -E echo "Testing is disabled. Ruby/Ceedling not available."
        COMMENT "Testing disabled - Ruby/Ceedling not found"
    )
endif()

# Print test information
message(STATUS "")
message(STATUS "=== Ceedling Test Configuration ===")
if(TESTING_ENABLED)
    if(CEEDLING_COMMAND)
        message(STATUS "Ceedling found at: ${CEEDLING_COMMAND}")
    else()
        message(STATUS "Ceedling will be installed automatically via bundler")
    endif()
    message(STATUS "Testing: ENABLED")
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
    message(STATUS "  generate-pngsuite-expectations - Generate PNG suite expectations")
    message(STATUS "  generate-pngsuite-tests   - Generate PNG suite test files")
else()
    message(STATUS "Testing: DISABLED (Ruby/Ceedling not available)")
    message(STATUS "")
    message(STATUS "Available targets (disabled):")
    message(STATUS "  test-all                  - Shows testing disabled message")
    message(STATUS "  test-clean                - Shows testing disabled message")
    message(STATUS "")
    message(STATUS "To enable testing, install Ruby and run 'bundle install' or install Ceedling directly.")
endif()
message(STATUS "")
