#!/bin/bash

# RPC++ Multithreaded Stress Test Script
# Comprehensive testing for race conditions, deadlocks, and service lifecycle issues
# Usage: ./stress_test_multithreaded.sh [OPTIONS]

set -e

# Configuration
DEFAULT_ITERATIONS=50
DEFAULT_TIMEOUT=30
DEFAULT_REPEAT=1
TEST_BINARY="./build/output/debug/rpc_test"
LOG_DIR="/tmp/rpc_stress_tests"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
ITERATIONS=$DEFAULT_ITERATIONS
TIMEOUT=$DEFAULT_TIMEOUT
REPEAT=$DEFAULT_REPEAT
VERBOSE=false
CONTINUE_ON_FAILURE=false
SPECIFIC_TEST=""
PARALLEL_RUNS=1

# Usage function
usage() {
    cat << EOF
RPC++ Multithreaded Stress Test Script

Usage: $0 [OPTIONS]

OPTIONS:
    -i, --iterations NUM     Number of test iterations (default: $DEFAULT_ITERATIONS)
    -t, --timeout SEC        Timeout per test run in seconds (default: $DEFAULT_TIMEOUT)
    -r, --repeat NUM         GTest repeat count per run (default: $DEFAULT_REPEAT)
    -v, --verbose            Enable verbose output
    -c, --continue           Continue testing after failures
    -s, --specific TEST      Run specific test pattern (e.g., "*two_zones*")
    -p, --parallel NUM       Run multiple test instances in parallel (default: 1)
    -h, --help              Show this help message

EXAMPLES:
    $0                                          # Run default stress test
    $0 -i 100 -t 45 -v                        # 100 iterations, 45s timeout, verbose
    $0 -s "*two_zones*" -i 20                 # Test specific pattern 20 times
    $0 -r 3 -p 2                              # Run with repeat=3, 2 parallel instances
    $0 -c -v                                   # Continue on failure with verbose output

MULTITHREADED TESTS COVERED:
    - multithreaded_standard_tests
    - multithreaded_standard_tests_with_and_foos
    - multithreaded_remote_tests
    - multithreaded_create_new_zone
    - multithreaded_create_new_zone_releasing_host_then_running_on_other_enclave
    - multithreaded_bounce_baz_between_two_interfaces
    - multithreaded_check_sub_subordinate
    - multithreaded_two_zones_get_one_to_lookup_other

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -i|--iterations)
            ITERATIONS="$2"
            shift 2
            ;;
        -t|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        -r|--repeat)
            REPEAT="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -c|--continue)
            CONTINUE_ON_FAILURE=true
            shift
            ;;
        -s|--specific)
            SPECIFIC_TEST="$2"
            shift 2
            ;;
        -p|--parallel)
            PARALLEL_RUNS="$2"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Validate inputs
if ! [[ "$ITERATIONS" =~ ^[0-9]+$ ]] || [ "$ITERATIONS" -lt 1 ]; then
    echo -e "${RED}Error: Iterations must be a positive integer${NC}"
    exit 1
fi

if ! [[ "$TIMEOUT" =~ ^[0-9]+$ ]] || [ "$TIMEOUT" -lt 1 ]; then
    echo -e "${RED}Error: Timeout must be a positive integer${NC}"
    exit 1
fi

if ! [[ "$REPEAT" =~ ^[0-9]+$ ]] || [ "$REPEAT" -lt 1 ]; then
    echo -e "${RED}Error: Repeat must be a positive integer${NC}"
    exit 1
fi

if ! [[ "$PARALLEL_RUNS" =~ ^[0-9]+$ ]] || [ "$PARALLEL_RUNS" -lt 1 ]; then
    echo -e "${RED}Error: Parallel runs must be a positive integer${NC}"
    exit 1
fi

# Check if test binary exists
if [ ! -f "$TEST_BINARY" ]; then
    echo -e "${RED}Error: Test binary not found at $TEST_BINARY${NC}"
    echo "Please build the project first:"
    echo "  cmake --build build --target rpc_test"
    exit 1
fi

# Create log directory
mkdir -p "$LOG_DIR"

# Test filter
if [ -n "$SPECIFIC_TEST" ]; then
    TEST_FILTER="$SPECIFIC_TEST"
else
    TEST_FILTER="*multithreaded*"
fi

# Summary tracking
TOTAL_RUNS=0
PASSED_RUNS=0
FAILED_RUNS=0
TIMEOUT_RUNS=0
RACE_CONDITIONS_DETECTED=0
ERRORS_DETECTED=0

# Function to log with timestamp
log() {
    echo -e "$(date '+%H:%M:%S') $1"
}

# Function to run a single test iteration
run_test_iteration() {
    local iteration=$1
    local process_id=$2
    local log_file="$LOG_DIR/stress_test_${TIMESTAMP}_p${process_id}_${iteration}.log"
    
    if [ "$VERBOSE" = true ]; then
        log "${BLUE}[P$process_id] Starting iteration $iteration${NC}"
    fi
    
    # Run the test with timeout
    local start_time=$(date +%s)
    timeout $TIMEOUT "$TEST_BINARY" --gtest_filter="$TEST_FILTER" --gtest_repeat=$REPEAT > "$log_file" 2>&1
    local exit_code=$?
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Analyze results
    local status=""
    local details=""
    
    if [ $exit_code -eq 124 ]; then
        status="TIMEOUT"
        details="Test exceeded ${TIMEOUT}s timeout"
        ((TIMEOUT_RUNS++))
    elif [ $exit_code -ne 0 ]; then
        status="FAILED"
        if grep -q "Segmentation fault\|CRASH\|Signal:" "$log_file"; then
            details="Crash detected (exit code: $exit_code)"
        elif grep -q "FAILED" "$log_file"; then
            details="Test assertion failed (exit code: $exit_code)"
        elif grep -q "check_is_empty\|RPC_ASSERT" "$log_file"; then
            details="Service cleanup failed (exit code: $exit_code)"
        else
            details="Unknown failure (exit code: $exit_code)"
        fi
        ((FAILED_RUNS++))
    elif grep -q "PASSED" "$log_file"; then
        status="PASSED"
        details="${duration}s"
        ((PASSED_RUNS++))
    else
        status="UNKNOWN"
        details="No clear pass/fail indication"
        ((FAILED_RUNS++))
    fi
    
    # Check for race conditions and errors
    if grep -q "RACE CONDITION DETECTED" "$log_file"; then
        local race_count=$(grep -c "RACE CONDITION DETECTED" "$log_file")
        ((RACE_CONDITIONS_DETECTED += race_count))
        details="$details, ${race_count} race conditions handled"
    fi
    
    if grep -q "ERROR:" "$log_file"; then
        local error_count=$(grep -c "ERROR:" "$log_file")
        ((ERRORS_DETECTED += error_count))
        details="$details, ${error_count} errors logged"
    fi
    
    ((TOTAL_RUNS++))
    
    # Output results
    case $status in
        "PASSED")
            if [ "$VERBOSE" = true ]; then
                log "${GREEN}[P$process_id] Iteration $iteration: $status ($details)${NC}"
            else
                echo -n "."
            fi
            ;;
        "FAILED")
            log "${RED}[P$process_id] Iteration $iteration: $status - $details${NC}"
            if [ "$VERBOSE" = true ]; then
                echo "    Log: $log_file"
                echo "    Last 5 lines:"
                tail -5 "$log_file" | sed 's/^/      /'
            fi
            ;;
        "TIMEOUT")
            log "${YELLOW}[P$process_id] Iteration $iteration: $status - $details${NC}"
            ;;
        *)
            log "${YELLOW}[P$process_id] Iteration $iteration: $status - $details${NC}"
            ;;
    esac
    
    # Return failure status for early termination logic
    if [ "$status" != "PASSED" ]; then
        return 1
    fi
    return 0
}

# Function to run stress test for a single process
run_stress_test_process() {
    local process_id=$1
    local iterations_per_process=$2
    
    for ((i=1; i<=iterations_per_process; i++)); do
        if ! run_test_iteration $i $process_id; then
            if [ "$CONTINUE_ON_FAILURE" = false ]; then
                log "${RED}[P$process_id] Stopping due to failure (use -c to continue)${NC}"
                return 1
            fi
        fi
    done
    return 0
}

# Function to display progress
show_progress() {
    if [ "$VERBOSE" = false ]; then
        local percent=$((TOTAL_RUNS * 100 / (ITERATIONS * PARALLEL_RUNS)))
        printf "\rProgress: %d%% (%d/%d) " $percent $TOTAL_RUNS $((ITERATIONS * PARALLEL_RUNS))
    fi
}

# Function to generate summary report
generate_summary() {
    local total_expected=$((ITERATIONS * PARALLEL_RUNS))
    local success_rate=0
    if [ $TOTAL_RUNS -gt 0 ]; then
        success_rate=$((PASSED_RUNS * 100 / TOTAL_RUNS))
    fi
    
    echo
    log "${BLUE}=== STRESS TEST SUMMARY ===${NC}"
    echo "Test Filter: $TEST_FILTER"
    echo "Configuration: $ITERATIONS iterations × $PARALLEL_RUNS processes × $REPEAT repeats"
    echo "Timeout: ${TIMEOUT}s per test"
    echo "Timestamp: $TIMESTAMP"
    echo
    echo "Results:"
    printf "  %-15s %6d / %6d (%3d%%)\n" "PASSED:" $PASSED_RUNS $TOTAL_RUNS $success_rate
    printf "  %-15s %6d / %6d\n" "FAILED:" $FAILED_RUNS $TOTAL_RUNS
    printf "  %-15s %6d / %6d\n" "TIMEOUT:" $TIMEOUT_RUNS $TOTAL_RUNS
    echo
    echo "Race Conditions Detected: $RACE_CONDITIONS_DETECTED"
    echo "Errors Logged: $ERRORS_DETECTED"
    echo "Log Directory: $LOG_DIR"
    
    # Overall status
    if [ $FAILED_RUNS -eq 0 ] && [ $TIMEOUT_RUNS -eq 0 ] && [ $TOTAL_RUNS -eq $total_expected ]; then
        log "${GREEN}✅ ALL TESTS PASSED - System is stable under stress${NC}"
        return 0
    elif [ $FAILED_RUNS -eq 0 ] && [ $TIMEOUT_RUNS -eq 0 ]; then
        log "${YELLOW}⚠️  Tests passed but some runs incomplete${NC}"
        return 1
    else
        log "${RED}❌ FAILURES DETECTED - System has stability issues${NC}"
        return 1
    fi
}

# Main execution
main() {
    log "${BLUE}🚀 Starting RPC++ Multithreaded Stress Test${NC}"
    echo "Configuration:"
    echo "  Test Filter: $TEST_FILTER"
    echo "  Iterations: $ITERATIONS"
    echo "  Parallel Processes: $PARALLEL_RUNS"
    echo "  Repeat per run: $REPEAT"
    echo "  Timeout: ${TIMEOUT}s"
    echo "  Continue on failure: $CONTINUE_ON_FAILURE"
    echo "  Verbose: $VERBOSE"
    echo
    
    # Calculate iterations per process
    local iterations_per_process=$((ITERATIONS / PARALLEL_RUNS))
    local remainder=$((ITERATIONS % PARALLEL_RUNS))
    
    # Start parallel processes
    local pids=()
    for ((p=1; p<=PARALLEL_RUNS; p++)); do
        local iterations_for_this_process=$iterations_per_process
        if [ $p -le $remainder ]; then
            ((iterations_for_this_process++))
        fi
        
        if [ $PARALLEL_RUNS -gt 1 ]; then
            run_stress_test_process $p $iterations_for_this_process &
            pids+=($!)
        else
            run_stress_test_process $p $iterations_for_this_process
        fi
    done
    
    # Wait for all processes if running in parallel
    if [ $PARALLEL_RUNS -gt 1 ]; then
        local all_success=true
        for pid in "${pids[@]}"; do
            if ! wait $pid; then
                all_success=false
            fi
        done
        
        if [ "$all_success" = false ] && [ "$CONTINUE_ON_FAILURE" = false ]; then
            log "${RED}One or more processes failed${NC}"
        fi
    fi
    
    # Show final progress
    if [ "$VERBOSE" = false ]; then
        echo
    fi
    
    # Generate and display summary
    generate_summary
}

# Trap cleanup
cleanup() {
    if [ $PARALLEL_RUNS -gt 1 ]; then
        log "${YELLOW}Cleaning up background processes...${NC}"
        jobs -p | xargs -r kill 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

# Run main function
main "$@"