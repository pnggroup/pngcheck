#!/bin/bash
# Main test runner script for pngcheck

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=== pngcheck Test Suite ==="
echo ""

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Change to the tests directory
cd "$SCRIPT_DIR"

# Check if bats is available
if ! command -v bats &> /dev/null; then
    echo -e "${RED}Error: bats is not installed${NC}"
    echo "Please install bats-core:"
    echo "  macOS: brew install bats-core"
    echo "  Ubuntu/Debian: apt-get install bats"
    echo "  Or see: https://github.com/bats-core/bats-core"
    exit 1
fi

# Generate test cases if needed
if [[ ! -f "test_pngcheck_suite.bats" ]] || [[ "generate_test_cases.sh" -nt "test_pngcheck_suite.bats" ]]; then
    echo "Generating PNG suite test cases..."
    ./generate_test_cases.sh
    echo ""
fi

# Parse command line arguments
run_cli_tests=true
run_suite_tests=true
verbose=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --cli-only)
            run_suite_tests=false
            shift
            ;;
        --suite-only)
            run_cli_tests=false
            shift
            ;;
        --verbose|-v)
            verbose=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --cli-only     Run only CLI tests"
            echo "  --suite-only   Run only PNG suite tests"
            echo "  --verbose, -v  Verbose output"
            echo "  --help, -h     Show this help"
            echo ""
            echo "Test files:"
            echo "  tests/test_pngcheck_cli.bats   - Static CLI functionality tests"
            echo "  tests/test_pngcheck_suite.bats - Auto-generated PNG suite tests"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Set bats options
bats_opts=""
if [[ "$verbose" == "true" ]]; then
    bats_opts="--verbose-run"
fi

# Run tests
exit_code=0

if [[ "$run_cli_tests" == "true" ]]; then
    echo -e "${YELLOW}Running CLI tests...${NC}"
    if bats $bats_opts test_pngcheck_cli.bats; then
        echo -e "${GREEN}✓ CLI tests passed${NC}"
    else
        echo -e "${RED}✗ CLI tests failed${NC}"
        exit_code=1
    fi
    echo ""
fi

if [[ "$run_suite_tests" == "true" ]]; then
    echo -e "${YELLOW}Running PNG suite tests...${NC}"
    if bats $bats_opts test_pngcheck_suite.bats; then
        echo -e "${GREEN}✓ PNG suite tests passed${NC}"
    else
        echo -e "${RED}✗ PNG suite tests failed${NC}"
        exit_code=1
    fi
    echo ""
fi

# Summary
if [[ $exit_code -eq 0 ]]; then
    echo -e "${GREEN}=== All tests passed! ===${NC}"
else
    echo -e "${RED}=== Some tests failed ===${NC}"
fi

exit $exit_code
