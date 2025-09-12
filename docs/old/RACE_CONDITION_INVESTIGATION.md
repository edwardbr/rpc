<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# Race Condition Investigation Report

## Problem Summary
A race condition in the RPC++ test suite causes occasional segmentation faults during multithreaded tests, specifically in `multithreaded_two_zones_get_one_to_lookup_other`.

## Investigation Process

### Initial Symptoms
- Occasional segfaults during `rpc_test` execution
- Problem appears in multithreaded scenarios involving inter-zone communication
- Assertion failure: `rpc::service_proxy::on_object_proxy_released(rpc::object): Assertion '!"error failed " "item != proxies_.end()"' failed`

### Systematic Elimination Process

1. **Parameter Type Investigation** - RULED OUT
   - Initially suspected [in] vs [out] parameter handling differences
   - Aggressive testing of both parameter types showed race condition affects all RPC calls
   - Not related to parameter marshalling complexity

2. **Single vs Multi-threaded Testing** - NARROWED SCOPE
   - Single-threaded tests: No issues
   - `multithreaded_standard_tests`: No issues (same zone)
   - `multithreaded_two_zones_*`: Race condition present

3. **Inter-zone Communication Focus** - IDENTIFIED PATTERN
   - Race condition only occurs with inter-zone RPC calls
   - Single-zone multithreaded operations are stable

4. **Host Lookup Mechanism Analysis** - RULED OUT
   - Added atomic counters and signal handling to track `host::look_up_app` calls
   - Signal handler output: `lookup_enter_count: 3, lookup_exit_count: 3, Calls in progress: 0`
   - All lookups complete successfully before crash occurs

## Root Cause Analysis

### Confirmed Root Cause
**RPC Proxy Lifecycle Management Race Condition**
- Location: `rpc::service_proxy::on_object_proxy_released` in `/home/edward/projects/rpc2/rpc/include/rpc/proxy.h`
- The race condition occurs during proxy cleanup/destruction, NOT during RPC execution
- Multiple threads attempting to clean up service proxies concurrently

### Technical Details

#### Evidence Supporting Root Cause
1. **Signal Handler Data**: All RPC calls complete successfully before segfault
2. **Assertion Location**: Failure in proxy cleanup code, not business logic
3. **Multithreaded Pattern**: Only affects scenarios with multiple zones creating/destroying proxies
4. **Timing**: Crash occurs during test teardown, not during active RPC calls

#### Affected Code Paths
- **Primary**: Inter-zone proxy creation and destruction in `multithreaded_two_zones_*` tests
- **Safe Operations**: Single-zone multithreaded RPC calls
- **Safe Operations**: All single-threaded operations regardless of zone count

## Test Configuration Changes Made

### Files Modified During Investigation
1. **`/home/edward/projects/rpc2/tests/fixtures/src/test_host.cpp`**
   - Added atomic counters for `host::look_up_app` call tracking
   - Implemented signal handlers for crash analysis
   - **Lines 12-22**: Global atomic counters and segfault handler
   - **Lines 59-69**: Instrumented `host::look_up_app` method

2. **Build Configuration**
   - Set `BUILD_ENCLAVE=OFF` for faster compilation during testing
   - Set `USE_RPC_LOGGING=OFF` to eliminate multithreaded logging interference

### Debugging Methodology
```cpp
// Atomic counters for tracking RPC calls
std::atomic<int> lookup_enter_count{0};
std::atomic<int> lookup_exit_count{0};

// Signal handler for crash analysis
void segfault_handler(int sig) {
    std::cout << "SEGFAULT DETECTED! lookup_enter_count: " << lookup_enter_count.load() 
              << ", lookup_exit_count: " << lookup_exit_count.load() << std::endl;
    std::cout << "Calls in progress: " << (lookup_enter_count.load() - lookup_exit_count.load()) << std::endl;
    signal(sig, SIG_DFL);
    raise(sig);
}
```

## Specific Failing Test Case

### Test: `multithreaded_two_zones_get_one_to_lookup_other`
- **Location**: `/home/edward/projects/rpc2/tests/test_host/main.cpp:653`
- **Execution Pattern**: Creates multiple zones, performs inter-zone lookups, runs standard_tests
- **Race Window**: During concurrent proxy cleanup after test completion

### Reproduction Command
```bash
# Run with GTest filter for multithreaded tests
./build/output/debug/rpc_test --gtest_filter=*multithreaded*

# Specific failing test
./build/output/debug/rpc_test --gtest_filter=remote_type_test/1.multithreaded_two_zones_get_one_to_lookup_other
```

## Next Steps for Resolution

### Immediate Investigation Areas
1. **`/home/edward/projects/rpc2/rpc/include/rpc/proxy.h`** around line 520
   - Examine `on_object_proxy_released` implementation
   - Review proxy container access patterns
   - Check for missing synchronization primitives

2. **Proxy Lifecycle Management**
   - Review concurrent access to `proxies_` container
   - Verify thread-safety of proxy registration/deregistration
   - Examine shared_ptr reference counting in multithreaded contexts

3. **Zone Cleanup Coordination**
   - Investigate how multiple zones coordinate proxy cleanup
   - Check for proper synchronization during zone destruction

### Recommended Fixes
1. Add proper mutex protection around proxy container operations
2. Implement reference counting coordination between zones
3. Add cleanup ordering guarantees for inter-zone proxies

## Technical Context

### RPC++ Architecture Relevant to Issue
- **Multi-zone Communication**: Proxies can reference objects in different zones
- **Proxy Management**: Central registry tracks active proxies per zone
- **Cleanup Coordination**: Multiple threads may attempt cleanup simultaneously
- **Assertion Location**: Safety check in proxy deregistration fails under race conditions

### Build Environment
- **CMake Version**: 3.24+
- **C++ Standard**: C++17
- **Build Generator**: Ninja
- **Test Framework**: Google Test

This race condition represents a fundamental thread-safety issue in the RPC++ proxy management system when dealing with inter-zone communication scenarios.