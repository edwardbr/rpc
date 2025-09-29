# Optimistic Pointer Implementation Plan

## Overview

This document outlines the plan for implementing `rpc::optimistic_ptr<T>` - a smart pointer optimized for RPC scenarios where objects are expected to be local most of the time but may occasionally be remote. The optimistic pointer provides better performance than `rpc::shared_ptr<T>` for predominantly local access patterns.

## Design Goals

### Primary Objectives
1. **Local Access Optimization**: Minimize overhead for local object access (direct pointer dereference)
2. **Transparent Remote Fallback**: Seamlessly handle remote objects when local assumption fails
3. **Reference Counting**: Maintain proper object lifetime management across local/remote boundaries
4. **Type Safety**: Preserve strong typing and casting_interface requirements
5. **Thread Safety**: Safe concurrent access in multi-threaded RPC scenarios

### Performance Targets
- **Local Access**: Near-native pointer performance (single indirection)
- **Remote Discovery**: Acceptable one-time overhead for remote object detection
- **Memory Footprint**: Minimal overhead compared to raw pointers

## Architecture Design

### Core Components

#### 1. optimistic_ptr<T> Class
```cpp
#ifndef TEST_STL_COMPLIANCE
template<typename T>
class optimistic_ptr {
private:
    element_type_impl* ptr_{nullptr};           // Same structure as shared_ptr
    __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};

    // No public raw pointer constructor (except void specialization)
    template<typename Y, typename = std::enable_if_t<std::is_same_v<Y, void>>>
    explicit optimistic_ptr(Y* p);

public:
    // Standard shared_ptr-like interface
    T* operator->() const;  // Throws bad_local_object if !is_local
    T& operator*() const;   // Throws bad_local_object if !is_local
    T* get() const;         // Throws bad_local_object if !is_local

    // Assignment and conversion with shared_ptr and weak_ptr
    optimistic_ptr& operator=(const shared_ptr<T>& r) noexcept;
    optimistic_ptr& operator=(const weak_ptr<T>& r) noexcept;
    optimistic_ptr(const shared_ptr<T>& r) noexcept;
    optimistic_ptr(const weak_ptr<T>& r) noexcept;

    // All operators and hash support identical to shared_ptr
    long use_count() const noexcept;
    bool unique() const noexcept;
    explicit operator bool() const noexcept;
};

// Exception thrown when accessing non-local objects
class bad_local_object : public std::exception {
public:
    const char* what() const noexcept override { return "bad_local_object"; }
};
#endif // !TEST_STL_COMPLIANCE
```

#### 2. Enhanced Control Block with Dual Ownership Model and Conditional Compilation
Extended `control_block_base` with optimistic reference counting, dual ownership model, and complete STL compliance protection:

```cpp
struct control_block_base {
    std::atomic<long> shared_owners{0};
    std::atomic<long> weak_owners{1};
#ifndef TEST_STL_COMPLIANCE
    std::atomic<long> optimistic_owners{0};  // NEW: Hidden during STL compliance testing
    bool is_local = false;                   // NEW: Hidden during STL compliance testing
#endif

    void* managed_object_ptr_{nullptr};

#ifndef TEST_STL_COMPLIANCE
    // NEW: Optimistic reference counting with dual ownership model
    void increment_optimistic() {
        long prev = optimistic_owners.fetch_add(1, std::memory_order_relaxed);
        if (prev == 0 && !is_local) {
            // Remote objects: optimistic_ptr participates in lifetime management
            get_object_proxy_from_interface()->add_ref(add_ref_options::optimistic);
        }
        // Local objects: optimistic_ptr behaves like weak_ptr, no lifetime management
    }

    void decrement_optimistic_and_dispose_if_zero() {
        long prev = optimistic_owners.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && !is_local) {
            // Remote objects: notify object_proxy of transition
            get_object_proxy_from_interface()->release(release_options::optimistic);
        }
        // Local objects: Do NOT prevent disposal when shared_owners == 0
        // Object can be deleted regardless of optimistic_owners count
    }

    // Enhanced shared reference counting with conditional optimistic logic
    void increment_shared() {
        long prev = shared_owners.fetch_add(1, std::memory_order_relaxed);
        if (prev == 0 && !is_local) {
            get_object_proxy_from_interface()->add_ref(add_ref_options::shared);
        }
    }

    void decrement_shared_and_dispose_if_zero() {
        long prev = shared_owners.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && !is_local) {
            get_object_proxy_from_interface()->release(release_options::shared);
        }
        if (prev == 1) {
            // Local objects: Dispose immediately when shared_owners reaches 0
            // regardless of optimistic_owners count (weak pointer semantics)
            dispose_object_actual();
            decrement_weak_and_destroy_if_zero();
        }
    }

private:
    object_proxy* get_object_proxy_from_interface(); // Get object_proxy from interface_proxy
#else
    // STL compliance mode: Standard shared_ptr behavior only
    void increment_shared() {
        shared_owners.fetch_add(1, std::memory_order_relaxed);
    }

    void decrement_shared_and_dispose_if_zero() {
        if (shared_owners.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            dispose_object_actual();
            decrement_weak_and_destroy_if_zero();
        }
    }
#endif // !TEST_STL_COMPLIANCE
};
```

**Critical Conditional Compilation Requirements**:
- **Complete Hiding**: `optimistic_ptr` class entirely hidden when `TEST_STL_COMPLIANCE` defined
- **Control Block Fields**: `optimistic_owners` and `is_local` hidden during STL testing
- **Method Implementations**: Optimistic logic conditionally compiled out
- **Exception Classes**: `bad_local_object` hidden during STL compliance testing

#### 3. Service Layer Architecture Changes

**Enhanced RPC Options with Optimistic Flags**:
```cpp
// Enhanced add_ref_options with optimistic bitmask
enum class add_ref_options : uint32_t {
    none = 0,
    shared = 1,
    optimistic = 2  // NEW: Bitmask indicating optimistic reference event
};

// NEW: release_options enum with optimistic indicator
enum class release_options : uint32_t {
    none = 0,
    shared = 1,
    optimistic = 2  // NEW: Bitmask indicating optimistic release event
};

// NEW: RPC error code for optimistic pointer scenarios
enum class error_code {
    // ... existing error codes ...
    OBJECT_GONE  // NEW: Object not available on destination service (normal for optimistic)
};
```

**Updated object_proxy with Reference Counting Interface**:
```cpp
class object_proxy {
public:
    // NEW: Reference counting methods called by control_block
    void add_ref(add_ref_options options);     // Handle shared/optimistic add_ref events
    void release(release_options options);     // Handle shared/optimistic release events (moves cleanup from destructor)

    // Minimal destructor - cleanup moved to release()
    ~object_proxy() = default;  // Cleanup logic moved to release()
};
```

**Service-side Optimistic Reference Management**:
```cpp
class service {
private:
    // NEW: Global optimistic reference counters per connected zone (telemetry only)
    std::unordered_map<zone_id, std::atomic<long>> optimistic_refs_per_zone_;

public:
    // Handle optimistic RPC calls with OBJECT_GONE fallback
    error_code handle_optimistic_rpc_call(object_id id) {
        auto local_shared_ptr = try_create_local_shared_ptr(id);
        if (!local_shared_ptr) {
            return error_code::OBJECT_GONE;  // Normal event, not an error
        }
        // Proceed with RPC call on local object
        return error_code::OK;
    }
};
```

**Updated service_proxy and i_marshaller**:
```cpp
class service_proxy {
    // Enhanced to relay shared/optimistic information
    void add_ref(object_id id, add_ref_options options);   // Pass optimistic flag to remote service
    void release(object_id id, release_options options);   // Pass optimistic flag to remote service
};

class i_marshaller {
    // Updated to handle optimistic reference counting events
    virtual void marshal_add_ref(add_ref_options options) = 0;
    virtual void marshal_release(release_options options) = 0;
};
```

#### 4. Local/Remote Detection Strategy
```cpp
enum class object_locality {
    unknown,        // Not yet determined
    confirmed_local,// Verified local object
    confirmed_remote// Verified remote object via object_proxy
};
```

### Dual Ownership Model

#### Local Objects (is_local == true)
**Weak Pointer Semantics**:
1. **No Lifetime Management**: optimistic_ptr does NOT keep local objects alive
2. **Disposal Independence**: Object deleted when `shared_owners` reaches zero, regardless of `optimistic_owners` count
3. **Dangling Reference Handling**: optimistic_ptr must handle cases where object is deleted while optimistic references exist
4. **Access Behavior**: Direct access (`operator->`, `get()`) works normally for local objects

#### Remote Objects (is_local == false)
**Shared Pointer Semantics**:
1. **Lifetime Management**: optimistic_ptr participates in keeping remote proxy objects alive
2. **Reference Counting**: Contributes to object_proxy lifecycle through add_ref/release calls
3. **Disposal Prevention**: Remote objects kept alive while optimistic references exist
4. **Access Behavior**: Direct access throws `bad_local_object`, must use RPC calls

#### Thread Safety Implications
**Complex Decrement Logic**:
1. **Race Condition Prevention**: Must handle concurrent access to both `shared_owners` and `optimistic_owners`
2. **Atomic State Transitions**: Ensure consistent state when transitioning between zero/non-zero reference counts
3. **Local vs Remote Logic**: Different disposal logic based on `is_local` flag requires careful synchronization
4. **Dangling Reference Protection**: Local objects may be deleted while optimistic references exist - requires safe handling

### Access Patterns

#### Fast Path (Optimistic Local Access)
1. Check `cb_->is_local` flag
2. Direct pointer dereference for local objects
3. Throw `bad_local_object` for remote objects

#### Slow Path (Confirmed Remote Access)
1. Throw `bad_local_object` exception for direct access attempts (`operator->`, `get()`)
2. For RPC calls via optimistic_ptr: destination service attempts local `shared_ptr` creation
3. If object unavailable, return `OBJECT_GONE` (normal event for optimistic scenarios)
4. Enables stateless object calls over unreliable networks without client lifetime management

## Implementation Phases

### Phase 1: Control Block Extensions and object_proxy Integration
**Deliverables**:
- [ ] Add `std::atomic<long> optimistic_owners{0}` to `control_block_base` (conditional compilation)
- [ ] Add `bool is_local` field to `control_block_base` (conditional compilation)
- [ ] Implement dual ownership model in `increment_optimistic()` method:
  - Local objects: weak pointer semantics (no lifetime management)
  - Remote objects: shared pointer semantics (notify object_proxy)
- [ ] Implement dual ownership model in `decrement_optimistic_and_dispose_if_zero()` method:
  - Local objects: do NOT prevent disposal when shared_owners == 0
  - Remote objects: notify object_proxy of transition
- [ ] Update `decrement_shared_and_dispose_if_zero()` for local disposal independence:
  - Dispose immediately when shared_owners reaches 0, regardless of optimistic_owners
- [ ] Add `get_object_proxy_from_interface()` method to access object_proxy when `!is_local`
- [ ] Add `bad_local_object` exception class (conditional compilation)
- [ ] Ensure all optimistic code is hidden when `TEST_STL_COMPLIANCE` defined

**Critical Requirements**:
- **Complete STL Compliance Protection**: All optimistic-related code must be conditionally compiled
- **Thread Safety**: Handle complex race conditions between shared_owners and optimistic_owners
- **Dual Ownership Model**: Different lifetime semantics for local vs remote objects

**Key Files**:
- `/rpc/include/rpc/internal/remote_pointer.h` (modify existing control_block_base with conditional compilation)
- `/rpc/include/rpc/internal/object_proxy.h` (add add_ref/release methods)

### Phase 2: optimistic_ptr Class Structure
**Deliverables**:
- [ ] `optimistic_ptr<T>` class in same header as `shared_ptr` (conditional on `!TEST_STL_COMPLIANCE`)
- [ ] Identical memory layout to `shared_ptr` (element_type_impl* ptr_, control_block_base* cb_)
- [ ] Constructor restrictions: no public raw pointer constructor (except void specialization)
- [ ] Basic copy/move constructors and destructors with optimistic reference counting

**Key Files**:
- `/rpc/include/rpc/internal/remote_pointer.h` (add optimistic_ptr class)

### Phase 3: Access Operations with Locality Checking
**Deliverables**:
- [ ] `operator->()` with `is_local` check, throws `bad_local_object` if remote
- [ ] `operator*()` with `is_local` check, throws `bad_local_object` if remote
- [ ] `get()` method with `is_local` check, throws `bad_local_object` if remote
- [ ] `use_count()`, `unique()`, `operator bool()` identical to shared_ptr

**Exception Behavior**:
- All access operations must check `cb_->is_local` before allowing access
- Throw `bad_local_object` exception for remote object access attempts

### Phase 4: Interoperability with shared_ptr and weak_ptr
**Deliverables**:
- [ ] Assignment operators from `shared_ptr<T>` and `weak_ptr<T>`
- [ ] Copy constructors from `shared_ptr<T>` and `weak_ptr<T>`
- [ ] Assignment operators to `shared_ptr<T>` and `weak_ptr<T>`
- [ ] Proper reference count transitions between different pointer types

**Key Conversions**:
```cpp
optimistic_ptr<T> opt_ptr = shared_ptr_instance;  // Copy from shared_ptr
shared_ptr<T> shared_ptr_instance = opt_ptr;      // Convert to shared_ptr
optimistic_ptr<T> opt_ptr2 = weak_ptr_instance;   // Copy from weak_ptr
```

### Phase 5: Standard Container Support
**Deliverables**:
- [ ] Hash specializations identical to `shared_ptr<T>`
- [ ] All comparison operators (`==`, `!=`, `<`, `<=`, `>`, `>=`)
- [ ] `owner_before()` method for associative containers
- [ ] Template specializations for `std::hash<optimistic_ptr<T>>`

**Container Support**:
```cpp
// Hash support identical to shared_ptr
namespace std {
    template<typename T>
    struct hash<rpc::optimistic_ptr<T>> {
        size_t operator()(const rpc::optimistic_ptr<T>& p) const noexcept;
    };
}

// All operators identical to shared_ptr behavior
template<typename T, typename U>
bool operator==(const optimistic_ptr<T>& a, const optimistic_ptr<U>& b) noexcept;
```

### Phase 6: Service Layer Integration
**Deliverables**:
- [ ] Add `add_ref_options::optimistic` and `release_options::optimistic` enum values
- [ ] Update `object_proxy` with `add_ref(add_ref_options)` and `release(release_options)` methods
- [ ] Move cleanup logic from `object_proxy` destructor to `release()` method
- [ ] Update `service_proxy` to relay shared/optimistic information in add_ref/release calls
- [ ] Update `i_marshaller` to handle optimistic reference counting events
- [ ] Add `error_code::OBJECT_GONE` for optimistic RPC scenarios
- [ ] Implement global optimistic reference counters per zone in `service` (telemetry only)
- [ ] Add `handle_optimistic_rpc_call()` with local `shared_ptr` creation and `OBJECT_GONE` fallback

**Key Files**:
- `/rpc/include/rpc/internal/marshaller.h` (add optimistic to add_ref_options, create release_options)
- `/rpc/include/rpc/internal/object_proxy.h` (add add_ref/release methods, move destructor cleanup)
- `/rpc/include/rpc/internal/service_proxy.h` (update add_ref/release signatures)
- `/rpc/include/rpc/internal/service.h` (add optimistic reference tracking and RPC handling)
- `/rpc/include/rpc/error_codes.h` (add OBJECT_GONE error code)

**Service-side Behavior Changes**:
- **Conditional RAII**: Same logic when optimistic flag is false, different handling when true
- **Telemetry Integration**: Track optimistic references per connected zone for debugging visibility
- **Graceful Degradation**: `OBJECT_GONE` as normal event enables stateless object patterns over unreliable networks

### Phase 7: Testing & Validation
**Deliverables**:
- [ ] Unit tests for locality checking and exception throwing
- [ ] Multi-threaded safety tests for optimistic reference counting
- [ ] Interoperability tests between optimistic_ptr, shared_ptr, and weak_ptr
- [ ] Exception safety tests for `bad_local_object` scenarios
- [ ] Service layer integration tests with optimistic reference counting
- [ ] `OBJECT_GONE` error code testing for optimistic RPC scenarios
- [ ] Telemetry validation for per-zone optimistic reference tracking
- [ ] Integration tests with existing RPC scenarios ensuring no STL test compilation

**Test Coverage Requirements**:
- Local object access patterns (should succeed)
- Remote object access patterns (should throw `bad_local_object`)
- Reference counting correctness across pointer type conversions
- Thread-safe concurrent access to optimistic_owners
- Service layer optimistic reference counting with add_ref/release options
- `OBJECT_GONE` scenarios as normal events (not errors)
- Global optimistic reference counters per zone (telemetry validation)
- Control block to object_proxy lifecycle management
- Conditional compilation verification (`!TEST_STL_COMPLIANCE` only)

## Technical Specifications

### Memory Layout
```cpp
class optimistic_ptr<T> {
    element_type_impl* ptr_{nullptr};           // 8 bytes - identical to shared_ptr
    control_block_base* cb_{nullptr};           // 8 bytes - identical to shared_ptr
    // Total: 16 bytes (identical to shared_ptr)
};

// Enhanced control block
struct control_block_base {
    std::atomic<long> shared_owners{0};     // 8 bytes
    std::atomic<long> weak_owners{1};       // 8 bytes
    std::atomic<long> optimistic_owners{0}; // 8 bytes - NEW
    bool is_local = false;                  // 1 byte
    void* managed_object_ptr_{nullptr};     // 8 bytes
    // Additional virtual function table pointer overhead
};
```

### Performance Characteristics

#### Local Access (Optimistic Case)
- **Dereference Cost**: 1 memory access + 1 bool check for is_local
- **Exception Overhead**: No overhead when is_local == true
- **Memory Footprint**: Identical to shared_ptr (16 bytes)

#### Remote Access (Exception Case)
- **Exception Cost**: `bad_local_object` exception thrown
- **Use Case**: Forces developer to use shared_ptr for remote objects
- **Design Intent**: Optimistic pointer should only be used when locality is expected

### Thread Safety Model

#### Optimistic Reference Counting
- **Increment**: Thread-safe atomic increment of `optimistic_owners`
- **Decrement**: Thread-safe atomic decrement with disposal check
- **Disposal Logic**: Object disposed when both `shared_owners` and `optimistic_owners` reach zero
- **Locality Check**: Read-only access to `is_local` boolean (set once during construction)

#### Synchronization Points
1. **Construction/Assignment**: Atomic increment of `optimistic_owners`
2. **Destruction**: Atomic decrement of `optimistic_owners` with potential disposal
3. **Access Operations**: Read-only check of `is_local` flag
4. **Interoperability**: Thread-safe transitions between shared_ptr, weak_ptr, and optimistic_ptr

## Integration Points

### Existing RPC Infrastructure
- **Control Block System**: Leverage enhanced control blocks with `is_local` flag
- **Object Proxy**: Use existing remote object mechanism for fallback
- **Casting Interface**: Maintain compatibility with `casting_interface` requirements
- **Service Layer**: Integration with service discovery and object lifetime management

### STL Compatibility
- **Hash Support**: Enable use in `std::unordered_map` and `std::unordered_set`
- **Comparison Operators**: Full set of relational operators
- **Iterator Compatibility**: Support for range-based loops and STL algorithms

## Architectural Benefits and Use Cases

### Optimistic Pointer Advantages

**Stateless Object Communication**:
- **Unreliable Networks**: Enables RPC calls to objects over unreliable network connections
- **No Client Lifetime Management**: Clients can make calls without managing server-side object lifetimes
- **Graceful Degradation**: `OBJECT_GONE` provides clean handling when objects are unavailable
- **Performance**: Local object access has minimal overhead compared to full RPC marshalling

**Service Architecture Improvements**:
- **Telemetry Visibility**: Global per-zone optimistic reference counters provide debugging insights
- **Resource Management**: Control block manages object_proxy lifecycle instead of interface_proxy
- **Flexible RAII**: Different resource management patterns for optimistic vs shared references
- **Network Resilience**: System continues operation even when remote objects become unavailable

### Typical Use Cases

**Scenario 1: Distributed Cache Access**
```cpp
// Client optimistically accesses cache entries without managing their lifetime
rpc::optimistic_ptr<cache_entry> entry = get_cache_entry(key);
try {
    auto result = entry->compute_expensive_operation();  // Local access
} catch (const bad_local_object&) {
    // Entry was remote, make RPC call which may return OBJECT_GONE
    // This is normal for cache entries that may expire
}
```

**Scenario 2: Stateless Service Calls**
```cpp
// Service objects that don't require client lifetime management
rpc::optimistic_ptr<stateless_processor> processor = get_processor();
auto result = processor->process_data(input);  // May return OBJECT_GONE if processor unavailable
// Client doesn't need to manage processor lifetime
```

**Scenario 3: Telemetry and Monitoring**
```cpp
// Services can monitor optimistic reference distribution
class service {
    void report_optimistic_stats() {
        for (auto& [zone_id, count] : optimistic_refs_per_zone_) {
            log_telemetry("Zone {} has {} optimistic references", zone_id, count.load());
        }
    }
};
```

## Risk Mitigation

### Performance Risks
- **Validation Overhead**: Mitigated by atomic caching and optimistic assumption
- **Memory Overhead**: Minimal increase (1 byte) over shared_ptr
- **Cache Line Effects**: Careful member ordering to minimize cache misses

### Correctness Risks
- **Race Conditions**: Comprehensive atomic validation and thread-safe reference counting
- **Remote Object Lifetime**: Proper integration with existing object_proxy lifecycle
- **Type Safety**: Maintain casting_interface requirements and compile-time checks

### Compatibility Risks
- **Existing Code**: Designed as optional enhancement, not replacement for shared_ptr
- **ABI Stability**: New class doesn't affect existing shared_ptr ABI
- **Generator Integration**: May require updates to IDL code generation for optimal usage

## Success Metrics

### Performance Metrics
- **Local Access**: Single bool check overhead (minimal compared to shared_ptr)
- **Memory Usage**: Identical to shared_ptr (16 bytes)
- **Reference Counting**: Atomic operations on optimistic_owners counter
- **Exception Handling**: Clean exception throw for remote access attempts

### Quality Metrics
- **Test Coverage**: > 95% line coverage including exception paths
- **Thread Safety**: Pass intensive multi-threaded optimistic reference counting tests
- **Interoperability**: Seamless conversion between optimistic_ptr, shared_ptr, weak_ptr
- **Conditional Compilation**: Never available in TEST_STL_COMPLIANCE mode
- **Exception Safety**: Proper cleanup and reference counting during exception scenarios

### Integration Metrics
- **Header Integration**: Single header implementation alongside shared_ptr
- **Control Block Reuse**: No additional control block types needed
- **Casting Interface**: Full compatibility with existing casting_interface requirements
- **Container Support**: Full STL container compatibility with hash and comparison operators

## Future Enhancements

### Potential Extensions
1. **Batch Validation**: Validate multiple optimistic pointers in single operation
2. **Locality Prediction**: Machine learning-based locality prediction
3. **NUMA Awareness**: Consider NUMA topology for locality decisions
4. **Debugging Support**: Enhanced debugging and profiling tools

### Long-term Integration
- **Generator Optimization**: IDL generator support for optimal optimistic pointer usage
- **Service Discovery**: Integration with dynamic service discovery for locality hints
- **Distributed Caching**: Coordinated caching strategies across distributed services

---

This implementation plan provides a comprehensive roadmap for adding `rpc::optimistic_ptr<T>` to the RPC++ framework while maintaining compatibility with existing infrastructure and performance requirements.