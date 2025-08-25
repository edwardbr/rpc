# RPC++ AI Developer Guide
*Comprehensive debugging and development assistance documentation*

## Overview

This document provides detailed technical information for AI assistants and developers working with the RPC++ codebase. It includes debugging strategies, architecture internals, common issues, and comprehensive context for understanding the complex reference counting and multi-zone communication systems.

## Architecture Deep Dive

### Core Components Hierarchy

```
Service (Zone Container - Root Level)
├── other_zones: Map<{destination_zone, caller_zone}, weak_ptr<service_proxy>>
│   ├── Service Proxy (Communication Channel)
│   │   ├── proxies_: Map<object_id, weak_ptr<object_proxy>>
│   │   ├── lifetime_lock_count_: External reference counter (atomic<int>)
│   │   └── Object Proxies (Remote Object Handles)
│   │       ├── object_id: Remote object identifier
│   │       ├── service_proxy_: Back-reference to parent
│   │       └── inherited_reference_count_: Race condition handling
│   └── [Additional Service Proxies for different zone pairs...]
└── [Local stubs and wrapped objects...]
```

### Zone Communication Patterns

**Zone Types in Typical Setup:**
- **HOST (Zone 1)**: Root parent zone, manages child processes/enclaves
- **MAIN CHILD (Zone 2)**: Intermediate zone, child of HOST
- **EXAMPLE_ZONE (Zone 3)**: Target zone, child of MAIN CHILD

**Service Proxy Types:**

1. **Direct Service Proxies**: `destination_zone_id == zone_id.as_destination()`
   - Purpose: Direct communication to adjacent zones
   - Host their own object_proxies locally
   - Manage actual objects in their destination zone
   - Straightforward reference counting

2. **Routing Service Proxies**: `destination_zone_id != zone_id.as_destination()`
   - Purpose: Route calls through intermediate zones
   - Should NOT host object_proxies locally (routing only)
   - Forward calls to destination zones
   - Complex reference counting due to multi-hop routing

## Reference Counting Architecture

### Critical Understanding: Three-Level Reference Counting

The RPC++ system implements sophisticated multi-level reference counting:

1. **Services** - Root containers managing zones
2. **Service Proxies** - Communication channels between zones  
3. **Object Proxies** - Handles to remote objects

### Service Proxy External Reference Counting

**Key Data Structure:**
```cpp
class service_proxy {
    std::atomic<int> lifetime_lock_count_ = 0;  // External reference counter
    
    void add_external_ref();           // Increment reference count
    int release_external_ref();        // Decrement reference count  
    bool is_unused() const;           // Check if count == 0
};
```

**Reference Sources and Their Purposes:**

1. **Service Proxy Creation** (`inner_add_zone_proxy` - service.cpp:1293)
   ```cpp
   service_proxy->add_external_ref();  // Initial reference when added to other_zones map
   ```

2. **Object Operations** (`add_ref` method - service.cpp:914 and 1053)
   ```cpp
   destination->add_external_ref();    // Reference during add_ref operations
   other_zone->add_external_ref();     // Reference for routing operations
   ```

3. **Object Proxy Creation** (`get_or_create_object_proxy` - proxy.h:742)
   ```cpp
   add_external_ref();                 // Reference while object_proxy exists
   ```

4. **Temporary Operations** (`try_cast`, `prepare_remote_input_interface` - service.cpp:738)
   ```cpp
   other_zone->add_external_ref();     // Temporary reference during operation
   other_zone->release_external_ref(); // Immediately balanced
   ```

### The add_ref Function Complexity

**Location**: service.cpp:747-1096

**Purpose**: Manages reference counting operations across zones with different routing strategies controlled by `add_ref_options` flag.

**Flag System:**
- `normal = 1`: Standard reference counting
- `build_destination_route = 2`: Unidirectional add_ref to destination
- `build_caller_route = 4`: Unidirectional add_ref to caller (reverse direction)

**Critical Untested Paths:**

1. **Line 792**: `dest_channel == caller_channel && build_channel`
   - Scenario: Zone is "passing the buck" to another zone
   - Comment indicates this is an "untested section"
   - RPC_ASSERT(false) shows this path has never been exercised

2. **Line 870**: Building caller route but caller zone unknown
   - Occurs when "a reference to a zone is passed to a zone that does not know of its existence"
   - Falls back to `get_parent()` but this assumes parent knows about the zone
   - May be incorrect in complex topologies

**Reference Flow Patterns:**
1. **Direct Pass**: Caller and destination add_ref calls align
2. **Opposite Directions**: Reference flows counter to each other
3. **Fork Scenarios**: References split through branching zone hierarchies

## Common Debugging Issues

### 1. Reference Count Imbalance

**Symptom**: Service proxy with `reference_count > 0` but `proxies_.empty()`

**Example Debug Timeline:**
```
1. add_external_ref() count=1  ← inner_add_zone_proxy (service proxy creation)
2. add_external_ref() count=2  ← add_ref method (first call)  
3. add_external_ref() count=3  ← add_ref method (second call)
4. add_external_ref() count=4  ← add_ref method (third call)

5. release_external_ref() count=3  ← get_or_create_object_proxy (RELEASE_IF_NOT_NEW)
6. release_external_ref() count=2  ← on_object_proxy_released (object_id=1)  
7. release_external_ref() count=1  ← on_object_proxy_released (object_id=2)

FINAL STATE: reference_count=1, proxies_count=0  ← BUG: Unmatched reference
```

**Root Cause**: 3 `add_ref` calls but only 2 corresponding `release` operations

### 2. Race Conditions in Multithreaded Tests

**Primary Location**: `rpc::service_proxy::on_object_proxy_released` in proxy.h
**Failing Test**: `multithreaded_two_zones_get_one_to_lookup_other`
**Assertion**: `!\"error failed \" \"item != proxies_.end()\"' failed`

**Key Insight**: Race condition occurs during proxy cleanup/destruction, NOT during RPC execution

**Evidence**:
- Signal handler data shows all RPC calls complete successfully before segfault
- Assertion failure in proxy cleanup code, not business logic
- Only affects scenarios with multiple zones creating/destroying proxies
- Crash occurs during test teardown, not during active RPC calls

**Safe vs Unsafe Patterns**:
- ✅ Single-zone multithreaded RPC calls
- ✅ All single-threaded operations regardless of zone count
- ❌ Inter-zone proxy creation and destruction in multithreaded scenarios

### 3. Enclave Compatibility Issues

**Common Problems**:
- `execinfo.h` not available in SGX enclaves
- `iostream`, `thread`, `sstream` not supported
- `printf` not declared in enclave scope
- OS-specific headers incompatible with restricted C library

**Solutions Applied**:
```cpp
#ifndef _IN_ENCLAVE
#include <thread>
#include <sstream>
#include <iostream>
#endif

// Replace printf with LOG_CSTR in generated code
LOG_CSTR("failed in {}");  // Instead of printf
```

## Development Patterns and Best Practices

### Reference Counting RAII Pattern

```cpp
class scoped_service_proxy_ref {
    rpc::shared_ptr<service_proxy> proxy_;
public:
    scoped_service_proxy_ref(rpc::shared_ptr<service_proxy> p) : proxy_(p) {
        if (proxy_) proxy_->add_external_ref();
    }
    ~scoped_service_proxy_ref() {
        if (proxy_) proxy_->release_external_ref();
    }
};
```

### Reference Counting Invariants

**Service Proxy Invariants:**
1. Creation Reference: `inner_add_zone_proxy` → balanced by service proxy removal
2. Operation References: `add_ref` → balanced by corresponding `release` 
3. Object References: `get_or_create_object_proxy` → balanced by `on_object_proxy_released`
4. Temporary References: `try_cast`, etc. → immediately balanced

**Object Proxy Invariants:**
1. Service Proxy Reference: Each object_proxy holds service_proxy alive
2. Clean Destruction: `on_object_proxy_released` must be called
3. Map Cleanup: Object must be removed from `proxies_` map
4. Race Condition Handling: Inherited references properly cleaned up

### Telemetry System Architecture

**Interface Consolidation** (Recent Change):
```cpp
// Old fragmented interface
void on_service_creation(const char* service_name, zone zone_id);
void on_child_zone_creation(const char* service_name, zone zone_id, zone parent_zone_id);

// New consolidated interface
void on_service_creation(const char* service_name, zone zone_id, zone parent_zone_id);
```

**Tag-Based Constructor Pattern** (Prevents unwanted telemetry calls):
```cpp
protected:
    struct child_service_tag {};
    explicit service(const char* name, zone zone_id, child_service_tag);
```

## Code Generation and IDL Processing

### Recent Enhancements (Critical for AI Understanding)

**Template Parameter Detection** (synchronous_generator.cpp):
- Replaced hardcoded template parameter names ('T', 'U') with first-principles analysis
- Universal namespace resolution instead of hardcoded prefixes
- Enhanced type resolution for nested complex types

**Attributes System Refactoring** (coreclasses.h):
```cpp
// Old: typedef std::list<std::string> attributes;
// New: Full class supporting name=value pairs
class attributes {
private:
    std::vector<std::pair<std::string, std::string>> data_;
public:
    void push_back(const std::string& str);
    void push_back(const std::pair<std::string, std::string>& pair);
    // Iterator and utility methods...
};
```

### JSON Schema Generation Integration Details

**Extended Function Info Structure**:
```cpp
struct function_info {
    std::string description;  // From [description="..."] attribute
    std::string json_schema;      // Auto-generated from parameters
    // ... existing fields
};
```

**Supported Complex Types in Schema Generation**:
- User-defined structs/classes in any namespace
- Template instantiations (e.g., `MyTemplate<int>`)
- Nested containers (e.g., `std::vector<MyStruct>`, `std::map<string, MyClass>`)
- Multi-parameter templates with any parameter names

## Debugging Tools and Strategies

### 1. Reference Count Debugging

```cpp
#ifdef DEBUG
void verify_reference_balance() {
    RPC_ASSERT(lifetime_lock_count_ >= 0);
    if (is_unused() && !proxies_.empty()) {
        LOG_CSTR("WARNING: Service proxy unused but has object proxies");
    }
}
#endif
```

### 2. Race Condition Detection

**Atomic Counter Pattern** (Used in investigation):
```cpp
std::atomic<int> lookup_enter_count{0};
std::atomic<int> lookup_exit_count{0};

void segfault_handler(int sig) {
    std::cout << "SEGFAULT! lookup_enter: " << lookup_enter_count.load() 
              << ", lookup_exit: " << lookup_exit_count.load() << std::endl;
    std::cout << "Calls in progress: " 
              << (lookup_enter_count.load() - lookup_exit_count.load()) << std::endl;
}
```

### 3. Service Proxy State Dumps

**Diagnostic Information to Collect**:
- `lifetime_lock_count_` value
- `proxies_.size()`
- `destination_zone_id_` and `caller_zone_id_`
- Active object_proxy object_ids

### 4. Zone Topology Visualization

**Understanding Routing Patterns**:
```
HOST (zone=1) → [routing proxy: 1→3, caller=1] → MAIN CHILD (zone=2) → EXAMPLE_ZONE (zone=3)
```

## Build System Context

### CMake Presets and Their Purposes

**Debug Presets**:
- `Debug`: Standard debug build
- `Debug_SGX`: SGX hardware enclave support
- `Debug_SGX_Sim`: SGX simulation mode

**Key Configuration Options**:
```cmake
RPC_STANDALONE=ON              # Standalone build mode
BUILD_TEST=ON                  # Enable test building
BUILD_ENCLAVE=OFF              # SGX enclave support (base: disabled)
BUILD_COROUTINE=ON             # Coroutine support enabled
DEBUG_RPC_GEN=ON               # Debug code generation
USE_RPC_LOGGING=OFF            # Disable logging to avoid multithreaded interference
```

### Test Execution Patterns

**Systematic Testing Strategy**:
```bash
# Run specific test categories
./build/output/debug/rpc_test --gtest_filter=*multithreaded*
./build/output/debug/rpc_test --gtest_filter=*two_zones*

# Stress testing for race conditions
for i in {1..100}; do
    echo "=== Run $i ==="
    ./build/output/debug/rpc_test --gtest_filter=remote_type_test/1.multithreaded_two_zones_get_one_to_lookup_other
    if [ $? -eq 139 ]; then
        echo "*** SEGFAULT DETECTED AT RUN $i! ***"
        break
    fi
done
```

## Common Error Patterns and Solutions

### 1. "Unknown complex type" in Code Generation
**Cause**: Template parameter detection failed
**Solution**: Enhanced generic template parameter detection in synchronous_generator.cpp

### 2. Enclave Build Failures
**Cause**: OS-specific headers in enclave code
**Solution**: Conditional includes with `#ifndef _IN_ENCLAVE`

### 3. Variable Scope Issues in Service Proxies
**Cause**: Variables declared inside `#ifdef` but used outside
**Solution**: Move declarations outside conditional blocks

### 4. Telemetry Interface Mismatches
**Cause**: Interface consolidation requires parameter updates
**Solution**: Update all implementations to use consolidated signature

### 5. Reference Count Imbalances
**Cause**: Unmatched `add_ref`/`release` operations
**Solution**: Implement RAII patterns and reference tracking

## Future Development Considerations

### Threading and Coroutines
- Current: Synchronous calls with planned coroutine support
- Challenge: Complex reference counting in async contexts
- Solution: Careful lifetime management with coroutine frames

### Transport Extensibility
- Current: In-memory, arena, SGX enclave transports
- Planned: Network transports with security considerations
- Architecture: Plugin-based transport system

### Error Handling Evolution
- Current: Error codes only
- Planned: Exception-based error handling
- Challenge: Exception safety across zone boundaries

## Debugging Checklist for AI Assistants

When encountering issues in RPC++:

1. **Identify the Zone Pattern**:
   - Single zone? → Safe for multithreading
   - Multi-zone? → Check for race conditions

2. **Check Reference Counting**:
   - Are `add_external_ref()` calls balanced?
   - Is `service::check_is_empty()` passing?
   - Any service proxies with `reference_count > 0` but empty proxies?

3. **Verify Build Configuration**:
   - Enclave compatibility issues?
   - Proper CMake presets used?
   - Logging disabled for multithreaded tests?

4. **Examine Code Generation**:
   - Template parameter detection working?
   - Namespace resolution correct?
   - JSON schema metadata properly generated?

5. **Race Condition Analysis**:
   - Does failure occur during cleanup or execution?
   - Is the assertion in proxy management code?
   - Are multiple threads accessing proxy containers?

This guide provides the foundational knowledge needed to understand, debug, and extend the RPC++ system effectively. The key insight is that RPC++ implements a sophisticated distributed object management system that requires careful attention to reference counting balance and thread safety across zone boundaries.