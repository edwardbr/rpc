# Optimistic Pointer Implementation Plan

## Overview

This document outlines the plan for implementing `rpc::optimistic_ptr<T>` - a smart pointer optimized for RPC scenarios where objects are expected to be remote most of the time but may occasionally be local. The optimistic pointer is a callable non-RAII pointer that provides a non-locking alternative to `rpc::shared_ptr<T>`. It works transparently for both local and remote objects through their interface proxies.

**Key Features**:
- **Weak semantics for local objects**: Does not keep local objects alive (optimistic_owners doesn't prevent deletion)
- **Shared semantics for remote proxies**: Keeps remote interface_proxy objects alive to prevent premature cleanup
- **Direct callable**: `operator->` works for both local objects and remote proxies without exceptions
- **Non-locking calls**: Locking happens remotely only if the destination object exists
- **Circular dependency resolution**: Children can hold optimistic_ptr to host without RAII ownership

**Use Cases**:
- **Stateless services**: e.g., REST services where object lifetime is not client-managed
- **Circular dependencies**: Host owns children (shared_ptr), children call host (optimistic_ptr)
- **Optional RAII locking**: Use `local_optimistic_ptr` for temporary RAII lock on local objects during call scope

## Design Goals

### Primary Objectives
1. **Non-Locking Remote Calls**: Enable RPC calls without locking on the caller's side - locking happens remotely only if destination object exists
2. **Stateless Service Support**: Optimize for stateless services (e.g., REST services) where object lifetime is not client-managed
3. **Circular Dependency Resolution**: Handle scenarios where children should not hold RAII pointers to their owner (e.g., host owns children, children call host without locking)
4. **Weak Pointer Semantics for Local Objects**: Local objects can be deleted regardless of optimistic reference count
5. **Shared Pointer Semantics for Remote Objects**: Remote object proxies are kept alive by optimistic references to prevent premature cleanup
6. **Type Safety**: Preserve strong typing and casting_interface requirements
7. **Thread Safety**: Safe concurrent access in multi-threaded RPC scenarios

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

    // Helper methods for internal conversions (called by conversion functions)
    friend template<typename T2, typename U> CORO_TASK(optimistic_ptr<T2>) convert_to_optimistic(const shared_ptr<U>&);
    void internal_construct_from_shared(const shared_ptr<T>& sp);

public:
    // Same-type constructors and assignments (synchronous)
    constexpr optimistic_ptr() noexcept = default;
    constexpr optimistic_ptr(std::nullptr_t) noexcept;
    optimistic_ptr(const optimistic_ptr<T>& r) noexcept;
    optimistic_ptr(optimistic_ptr<T>&& r) noexcept;
    optimistic_ptr& operator=(const optimistic_ptr<T>& r) noexcept;
    optimistic_ptr& operator=(optimistic_ptr<T>&& r) noexcept;
    optimistic_ptr& operator=(std::nullptr_t) noexcept;
    ~optimistic_ptr();

    // Heterogeneous constructors and assignments (statically verifiable upcasts)
    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    optimistic_ptr(const optimistic_ptr<Y>& r) noexcept;

    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    optimistic_ptr(optimistic_ptr<Y>&& r) noexcept;

    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    optimistic_ptr& operator=(const optimistic_ptr<Y>& r) noexcept;

    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    optimistic_ptr& operator=(optimistic_ptr<Y>&& r) noexcept;

    // For downcasts, use async dynamic_cast_optimistic instead

    // Standard interface - works for both local and remote objects
    T* operator->() const noexcept;  // Returns pointer (local object or remote proxy)
    T& operator*() const noexcept;   // Returns reference
    T* get() const noexcept;         // Returns pointer

    // Utility methods (identical to shared_ptr)
    void reset() noexcept;
    void swap(optimistic_ptr& r) noexcept;
    long use_count() const noexcept;
    bool unique() const noexcept;
    explicit operator bool() const noexcept;

    // Comparison support
    template<typename Y> bool owner_before(const optimistic_ptr<Y>& other) const noexcept;
    template<typename Y> bool owner_before(const shared_ptr<Y>& other) const noexcept;

    // Internal access for friend functions
    __rpc_internal::__shared_ptr_control_block::control_block_base* internal_get_cb() const { return cb_; }
    element_type_impl* internal_get_ptr() const { return ptr_; }

    template<typename U> friend class optimistic_ptr;
    template<typename U> friend class shared_ptr;
};

// Note: No bad_local_object exception needed
// optimistic_ptr works transparently for both local and remote objects

// Local optimistic pointer - temporary RAII lock for making optimistic calls
template<typename T>
class local_optimistic_ptr {
private:
    T* ptr_{nullptr};
    shared_ptr<T> local_lock_;  // Only set if object is local (RAII lock)

    // MUST NOT be a member of another class - stack-only usage
    static_assert(true, "local_optimistic_ptr is for stack use only");

public:
    // Constructor from optimistic_ptr
    explicit local_optimistic_ptr(const optimistic_ptr<T>& opt) noexcept {
        if (!opt) {
            ptr_ = nullptr;
            return;
        }

        auto cb = opt.internal_get_cb();
        if (cb && cb->is_local) {
            // Local object - create temporary shared_ptr to RAII lock it
            local_lock_ = shared_ptr<T>(cb, opt.internal_get_ptr());
            local_lock_.internal_get_cb()->increment_shared();
            ptr_ = local_lock_.get();
        } else {
            // Remote object - just store the pointer (no RAII lock needed)
            ptr_ = opt.internal_get_ptr();
        }
    }

    // Assignment from optimistic_ptr
    local_optimistic_ptr& operator=(const optimistic_ptr<T>& opt) noexcept {
        local_optimistic_ptr temp(opt);
        swap(temp);
        return *this;
    }

    // Move constructor
    local_optimistic_ptr(local_optimistic_ptr&& other) noexcept
        : ptr_(other.ptr_)
        , local_lock_(std::move(other.local_lock_))
    {
        other.ptr_ = nullptr;
    }

    // Move assignment
    local_optimistic_ptr& operator=(local_optimistic_ptr&& other) noexcept {
        local_optimistic_ptr temp(std::move(other));
        swap(temp);
        return *this;
    }

    // Delete copy constructor and assignment (prevent accidental copies)
    local_optimistic_ptr(const local_optimistic_ptr&) = delete;
    local_optimistic_ptr& operator=(const local_optimistic_ptr&) = delete;

    ~local_optimistic_ptr() = default;  // RAII: local_lock_ releases automatically

    // Access operators - return valid pointer to local object or remote proxy
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }
    T* get() const noexcept { return ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }

    void swap(local_optimistic_ptr& other) noexcept {
        std::swap(ptr_, other.ptr_);
        std::swap(local_lock_, other.local_lock_);
    }

    // Check if this is a local object (has RAII lock)
    bool is_local() const noexcept { return local_lock_.operator bool(); }
};

// Async conversion functions (REQUIRED for type conversions)
template<typename T, typename U>
CORO_TASK(shared_ptr<T>) convert_to_shared(const shared_ptr<U>& from) noexcept;

template<typename T, typename U>
CORO_TASK(shared_ptr<T>) convert_to_shared(const optimistic_ptr<U>& from) noexcept;

template<typename T, typename U>
CORO_TASK(optimistic_ptr<T>) convert_to_optimistic(const shared_ptr<U>& from) noexcept;

template<typename T, typename U>
CORO_TASK(optimistic_ptr<T>) convert_to_optimistic(const optimistic_ptr<U>& from) noexcept;

// Async casting functions
template<typename T, typename U>
CORO_TASK(shared_ptr<T>) dynamic_cast_shared(const shared_ptr<U>& from) noexcept;

template<typename T, typename U>
CORO_TASK(optimistic_ptr<T>) dynamic_cast_optimistic(const optimistic_ptr<U>& from) noexcept;

// Synchronous casting functions (local aliasing only)
template<typename T, typename U>
optimistic_ptr<T> static_pointer_cast(const optimistic_ptr<U>& r) noexcept;

template<typename T, typename U>
optimistic_ptr<T> const_pointer_cast(const optimistic_ptr<U>& r) noexcept;

template<typename T, typename U>
optimistic_ptr<T> reinterpret_pointer_cast(const optimistic_ptr<U>& r) noexcept;

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
    // This function must be called when control block is guaranteed to be alive
    // (e.g., from optimistic_ptr copy constructor when source is valid)
    void increment_optimistic_no_lock() {
        optimistic_owners.fetch_add(1, std::memory_order_relaxed);
    }

    // Safe increment when control block lifetime is uncertain
    // Returns true if successful, false if control block is expired
    bool try_increment_optimistic() {
        // First, ensure control block stays alive during our operation
        long weak_count = weak_owners.load(std::memory_order_relaxed);

        do {
            // If weak_count is 0, control block is being/has been destroyed
            if (weak_count == 0) {
                return false;
            }
            // Try to increment weak_owners to keep control block alive
        } while (!weak_owners.compare_exchange_weak(weak_count, weak_count + 1,
                                                     std::memory_order_acquire,
                                                     std::memory_order_relaxed));

        // Control block is now guaranteed alive, safe to check optimistic_owners
        long prev = optimistic_owners.fetch_add(1, std::memory_order_relaxed);

        if (prev == 0) {
            // First optimistic_ptr: weak_owners already incremented above
            // This increment becomes the "optimistic_ptr weak_owner"
            if (!is_local) {
                // Remote objects: optimistic_ptr participates in remote proxy lifetime management
                get_object_proxy_from_interface()->add_ref(add_ref_options::optimistic);
            }
        } else {
            // Not the first optimistic_ptr: we pre-emptively incremented weak_owners
            // Undo that increment since weak_owners is only for control block lifetime
            decrement_weak_and_destroy_if_zero();
        }

        return true;
    }

    void decrement_optimistic_and_dispose_if_zero() {
        long prev = optimistic_owners.fetch_sub(1, std::memory_order_acq_rel);
        if (prev == 1 && !is_local) {
            // Remote objects: notify object_proxy of transition
            get_object_proxy_from_interface()->release(release_options::optimistic);
        }
        // Local objects: Do NOT prevent disposal when shared_owners == 0
        // Object can be deleted regardless of optimistic_owners count

        // If this was the last optimistic_ptr, decrement weak_owners
        // CRITICAL: This must be done last to prevent control block destruction
        // while other threads might be incrementing optimistic_owners
        if (prev == 1) {
            // Last optimistic_ptr: decrement the weak_owners that was added by first optimistic_ptr
            decrement_weak_and_destroy_if_zero();
        }
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
- **No Exception Classes**: No `bad_local_object` exception - optimistic_ptr works transparently

#### 2b. Updated shared_ptr<T> Class (Breaking Changes)
Updated `shared_ptr` to remove heterogeneous constructors/assignments and require async conversions:

```cpp
template<typename T>
class shared_ptr {
private:
    element_type_impl* ptr_{nullptr};
    __rpc_internal::__shared_ptr_control_block::control_block_base* cb_{nullptr};

    // Helper methods for internal conversions (called by conversion functions)
    friend template<typename T2, typename U> CORO_TASK(shared_ptr<T2>) convert_to_shared(const optimistic_ptr<U>&);
    void internal_construct_from_optimistic(const optimistic_ptr<T>& op);

public:
    // Same-type constructors and assignments (synchronous - KEPT)
    constexpr shared_ptr() noexcept = default;
    constexpr shared_ptr(std::nullptr_t) noexcept;

    // Raw pointer constructors (kept)
    template<typename Y, typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
    explicit shared_ptr(Y* p);

    template<typename Y, typename Deleter, typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
    shared_ptr(Y* p, Deleter d);

    template<typename Y, typename Deleter, typename Alloc, typename = std::enable_if_t<is_ptr_convertible<Y>::value>>
    shared_ptr(Y* p, Deleter d, Alloc cb_alloc);

    // Same-type copy/move (synchronous - KEPT)
    shared_ptr(const shared_ptr<T>& r) noexcept;
    shared_ptr(shared_ptr<T>&& r) noexcept;
    shared_ptr& operator=(const shared_ptr<T>& r) noexcept;
    shared_ptr& operator=(shared_ptr<T>&& r) noexcept;
    shared_ptr& operator=(std::nullptr_t) noexcept;

    // Aliasing constructors (kept)
    template<typename Y>
    shared_ptr(const shared_ptr<Y>& r, element_type* p_alias) noexcept;

    template<typename Y>
    shared_ptr(shared_ptr<Y>&& r, element_type* p_alias) noexcept;

    // Heterogeneous copy/move constructors (kept - standard shared_ptr behavior)
    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    shared_ptr(const shared_ptr<Y>& r) noexcept;

    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    shared_ptr(shared_ptr<Y>&& r) noexcept;

    // Heterogeneous assignments (kept - standard shared_ptr behavior)
    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    shared_ptr& operator=(const shared_ptr<Y>& r) noexcept;

    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    shared_ptr& operator=(shared_ptr<Y>&& r) noexcept;

    // Weak pointer constructor (kept)
    template<typename Y, typename = std::enable_if_t<is_pointer_compatible<Y>::value>>
    explicit shared_ptr(const weak_ptr<Y>& r);

    ~shared_ptr();

    // Standard interface (kept)
    void reset() noexcept;
    template<typename Y, typename Deleter = default_delete<Y>, typename Alloc = std::allocator<char>>
    void reset(Y* p, Deleter d = Deleter(), Alloc cb_alloc = Alloc());

    void swap(shared_ptr& r) noexcept;
    element_type_impl* get() const noexcept;
    T& operator*() const noexcept;
    T* operator->() const noexcept;
    long use_count() const noexcept;
    bool unique() const noexcept;
    explicit operator bool() const noexcept;

    template<typename Y> bool owner_before(const shared_ptr<Y>& other) const noexcept;
    template<typename Y> bool owner_before(const weak_ptr<Y>& other) const noexcept;

    // Internal access for friend functions
    __rpc_internal::__shared_ptr_control_block::control_block_base* internal_get_cb() const { return cb_; }
    element_type_impl* internal_get_ptr() const { return ptr_; }
};

// ✅ KEPT: Synchronous dynamic_pointer_cast (uses local query_interface)
template<typename T, typename U>
shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& from) noexcept;

// ✅ KEPT: Synchronous static pointer casts
template<typename T, typename U>
shared_ptr<T> static_pointer_cast(const shared_ptr<U>& r) noexcept;

template<typename T, typename U>
shared_ptr<T> const_pointer_cast(const shared_ptr<U>& r) noexcept;

template<typename T, typename U>
shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>& r) noexcept;
```

**Key Features of shared_ptr**:
1. ✅ **Standard semantics**: Identical to std::shared_ptr in TEST_STL_COMPLIANCE mode
2. ✅ **Heterogeneous constructors/assignments**: All conversions work synchronously (Derived→Base, etc.)
3. ✅ **Synchronous dynamic_pointer_cast**: Uses local query_interface for casting_interface types
4. ✅ **Aliasing constructors**: Pointer aliasing works as in std::shared_ptr
5. ✅ **Static/const/reinterpret casts**: All compile-time casts work synchronously
6. ✅ **Added helper method**: `internal_construct_from_optimistic` for optimistic_ptr conversions
7. ✅ **casting_interface requirement**: When !TEST_STL_COMPLIANCE, types must derive from casting_interface

**What Works vs What Requires Async**:

**✅ Synchronous (statically verifiable at compile time)**:
```cpp
// Upcast: Derived → Base (compile-time verifiable)
rpc::shared_ptr<Derived> derived = get_derived();
rpc::shared_ptr<Base> base = derived;  // ✅ Works - statically verifiable upcast

// Same-type operations
rpc::shared_ptr<Foo> foo1 = get_foo();
rpc::shared_ptr<Foo> foo2 = foo1;  // ✅ Works - same type

// Static cast (compile-time cast)
rpc::shared_ptr<Derived> derived = get_derived();
rpc::shared_ptr<Base> base = rpc::static_pointer_cast<Base>(derived);  // ✅ Works

// Const cast
rpc::shared_ptr<const Foo> cfoo = get_const_foo();
rpc::shared_ptr<Foo> foo = rpc::const_pointer_cast<Foo>(cfoo);  // ✅ Works
```

**❌ Requires Async (runtime type checking needed)**:
```cpp
// Downcast: Base → Derived (requires runtime type check)
rpc::shared_ptr<Base> base = get_base();
// OLD: auto derived = rpc::dynamic_pointer_cast<Derived>(base);  // ❌ Removed
// NEW: auto derived = CO_AWAIT rpc::dynamic_cast_shared<Derived>(base);  // ✅ Required

// Cross-cast between unrelated types (requires query_interface)
rpc::shared_ptr<Interface1> i1 = get_interface1();
// auto i2 = CO_AWAIT rpc::dynamic_cast_shared<Interface2>(i1);  // ✅ Required
```

**Rationale**:
- **Upcasts are safe**: Compile-time verifiable, no remote query needed
- **Downcasts need validation**: May require remote `query_interface` call
- **Only runtime checks require async**: If the compiler can verify it, it stays synchronous

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

#### Control Block Lifecycle Management
**optimistic_ptr acts like weak_ptr for control block**:
1. **First optimistic_ptr**: When `optimistic_owners` transitions from 0→1, increment `weak_owners`
2. **Last optimistic_ptr**: When `optimistic_owners` transitions from 1→0, decrement `weak_owners` (may destroy control block)
3. **Control block destruction**: Control block destroyed when `weak_owners` reaches 0 (no shared_ptr, weak_ptr, or optimistic_ptr exist)
4. **Thread safety**: All reference count operations are atomic

#### Local Objects (is_local == true)
**Weak Pointer Semantics**:
1. **No Object Lifetime Management**: optimistic_ptr does NOT keep local objects alive
2. **Control Block Lifetime**: optimistic_ptr DOES keep control block alive (through weak_owners)
3. **Disposal Independence**: Object deleted when `shared_owners` reaches zero, regardless of `optimistic_owners` count
4. **Dangling Reference Handling**: optimistic_ptr may reference deleted objects (pointer stored in control block may be stale)
5. **Access Behavior**: Direct access (`operator->`, `get()`) returns pointer to local object or remote proxy

#### Remote Objects (is_local == false)
**Shared Pointer Semantics for Proxy**:
1. **Proxy Lifetime Management**: optimistic_ptr participates in keeping remote interface_proxy objects alive
2. **Reference Counting**: Contributes to object_proxy lifecycle through add_ref/release calls
3. **Disposal Prevention**: Remote proxy objects kept alive while optimistic references exist
4. **Access Behavior**: Direct access returns pointer to remote proxy (which makes RPC calls)

#### Thread Safety Implications and Race Condition Mitigation

**Critical Race Conditions Addressed**:

1. **Control Block Destruction Race**:
   - **Problem**: Creating optimistic_ptr while control block is being destroyed
   - **Solution**: Use `try_increment_optimistic()` with CAS loop on `weak_owners`
   - **Guarantee**: Control block stays alive during optimistic_owners increment, or operation fails cleanly

2. **0→1 Transition Race**:
   - **Problem**: Multiple threads creating first optimistic_ptr simultaneously
   - **Solution**: All threads pre-emptively increment weak_owners, only winner keeps it
   - **Guarantee**: Exactly one weak_owners increment for 0→1 optimistic_owners transition

3. **1→0 Transition Race**:
   - **Problem**: Control block destruction while new optimistic_ptr being created
   - **Solution**: Last optimistic_ptr decrements weak_owners, new attempts check weak_owners != 0
   - **Guarantee**: Control block destroyed exactly once, new attempts fail gracefully

**Reference Counting Invariants**:
- `weak_owners` ≥ 1 while any (shared_ptr OR weak_ptr OR optimistic_ptr) exists
- `weak_owners` = 1 base + count(shared_ptr > 0 || weak_ptr > 0) + bool(optimistic_owners > 0)
- Control block destroyed when `weak_owners` reaches 0
- Object destroyed when `shared_owners` reaches 0 (regardless of optimistic_owners for local)

**Atomic Operations Order**:
1. **increment_optimistic_no_lock**: Called when control block lifetime guaranteed (copy from valid optimistic_ptr)
2. **try_increment_optimistic**: Called when control block lifetime uncertain (convert from shared_ptr/weak_ptr)
   - Load weak_owners (relaxed)
   - CAS loop: weak_owners != 0 → weak_owners + 1 (acquire on success)
   - Increment optimistic_owners (relaxed)
   - If prev == 0: keep weak_owners increment, else decrement it back
3. **decrement_optimistic_and_dispose_if_zero**: Always safe due to control block guarantee
   - Decrement optimistic_owners (acq_rel)
   - If prev == 1 && !is_local: release remote proxy
   - If prev == 1: decrement weak_owners (may destroy control block)

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
- [ ] Remove `bad_local_object` exception class (not needed - optimistic_ptr works for both local and remote)
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
- [ ] Two increment methods for different safety guarantees:
  - [ ] `increment_optimistic_no_lock()` - fast path when control block guaranteed alive (copy from valid optimistic_ptr)
  - [ ] `try_increment_optimistic()` - safe path with CAS loop when control block lifetime uncertain (convert from shared_ptr/weak_ptr)

**When to Use Each Increment Method**:
```cpp
// optimistic_ptr copy constructor - source is valid, control block guaranteed alive
optimistic_ptr(const optimistic_ptr& other) {
    cb_ = other.cb_;
    ptr_ = other.ptr_;
    if (cb_) {
        cb_->increment_optimistic_no_lock();  // ✅ FAST PATH - control block guaranteed alive
    }
}

// Constructor from shared_ptr - control block lifetime uncertain
optimistic_ptr(const shared_ptr<T>& sp) {
    if (sp) {
        if (sp.internal_get_cb()->try_increment_optimistic()) {  // ✅ SAFE PATH - CAS loop
            cb_ = sp.internal_get_cb();
            ptr_ = sp.internal_get_ptr();
        } else {
            cb_ = nullptr;  // Control block was destroyed
            ptr_ = nullptr;
        }
    }
}

// Constructor from weak_ptr - must check expiry
optimistic_ptr(const weak_ptr<T>& wp) {
    auto cb = wp.internal_get_cb();
    if (cb && cb->try_increment_optimistic()) {  // ✅ SAFE PATH - may be expired
        cb_ = cb;
        ptr_ = wp.internal_get_ptr();
    } else {
        cb_ = nullptr;
        ptr_ = nullptr;
    }
}
```

**Key Files**:
- `/rpc/include/rpc/internal/remote_pointer.h` (add optimistic_ptr class)

### Phase 3: local_optimistic_ptr - Callable Optimistic Pointer
**Deliverables**:
- [ ] `local_optimistic_ptr<T>` class with temporary RAII locking for local objects
- [ ] Constructor from `optimistic_ptr<T>` with locality detection
- [ ] Assignment operator from `optimistic_ptr<T>`
- [ ] Move constructor and move assignment
- [ ] Deleted copy constructor and copy assignment (prevent accidental copies)
- [ ] `operator->()` returns valid pointer to local object or remote proxy
- [ ] `operator*()` and `get()` methods for dereferencing
- [ ] `is_local()` method to check if object is local (has RAII lock)
- [ ] `operator bool()` to check for null

**Key Design Constraints**:
- **Stack use only**: MUST NOT be a member of another class
- **Temporary RAII lock**: For local objects, creates temporary `shared_ptr` to prevent deletion during scope
- **Remote proxy passthrough**: For remote objects, simply stores pointer without locking
- **Move-only semantics**: Prevents accidental copies that would create multiple RAII locks

**RAII Behavior**:
```cpp
void example(optimistic_ptr<IService> service) {
    {
        local_optimistic_ptr<IService> locked(service);
        // If local: shared_owners incremented, object locked
        // If remote: no locking, just proxy pointer

        locked->do_work();  // Safe to call

    } // locked destroyed here
    // If local: shared_owners decremented, object may be deleted
    // If remote: no reference count change
}
```

### Phase 3b: Access Operations for optimistic_ptr (Works for Both Local and Remote)
**Deliverables**:
- [ ] `operator->()` returns pointer to local object or remote proxy (no exceptions)
- [ ] `operator*()` returns reference to local object or remote proxy (no exceptions)
- [ ] `get()` method returns pointer to local object or remote proxy (no exceptions)
- [ ] `use_count()`, `unique()`, `operator bool()` identical to shared_ptr

**Behavior**:
- All access operations work transparently for both local and remote objects
- For local objects: returns pointer to actual local object
- For remote objects: returns pointer to remote proxy (interface_proxy)
- No exceptions thrown - operates like standard shared_ptr
- `bad_local_object` exception removed (not needed with this design)

### Phase 4: Pointer Conversions and Casting
**Design Principle**: Maintain standard `shared_ptr` semantics for all operations. Heterogeneous constructors/assignments work synchronously. Dynamic casting uses local `query_interface` when available, with optional async versions for explicit remote calls.

**Deliverables**:
- [ ] **Standard constructors and assignments (synchronous - identical to std::shared_ptr)**:
  - [ ] `shared_ptr(const shared_ptr<T>&)` - same type copy constructor
  - [ ] `shared_ptr(shared_ptr<T>&&)` - same type move constructor
  - [ ] `template<typename Y> shared_ptr(const shared_ptr<Y>&)` - heterogeneous copy constructor (upcasts/downcasts)
  - [ ] `template<typename Y> shared_ptr(shared_ptr<Y>&&)` - heterogeneous move constructor
  - [ ] All corresponding assignment operators
  - [ ] `optimistic_ptr(const optimistic_ptr<T>&)` - same type copy constructor
  - [ ] `optimistic_ptr(optimistic_ptr<T>&&)` - same type move constructor
  - [ ] `template<typename Y> optimistic_ptr(const optimistic_ptr<Y>&)` - heterogeneous copy constructor
  - [ ] `template<typename Y> optimistic_ptr(optimistic_ptr<Y>&&)` - heterogeneous move constructor
  - [ ] All corresponding assignment operators

- [ ] **Synchronous casting functions (work with both BUILD_COROUTINE on/off)**:
  - [ ] `shared_ptr<T> static_pointer_cast(const shared_ptr<U>&)` - static cast (existing)
  - [ ] `shared_ptr<T> const_pointer_cast(const shared_ptr<U>&)` - const cast (existing)
  - [ ] `shared_ptr<T> reinterpret_pointer_cast(const shared_ptr<U>&)` - reinterpret cast (existing)
  - [ ] `shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>&)` - dynamic cast using local query_interface (existing, keep as-is)
  - [ ] `optimistic_ptr<T> static_pointer_cast(const optimistic_ptr<U>&)` - static cast for optimistic
  - [ ] `optimistic_ptr<T> const_pointer_cast(const optimistic_ptr<U>&)` - const cast for optimistic
  - [ ] `optimistic_ptr<T> reinterpret_pointer_cast(const optimistic_ptr<U>&)` - reinterpret cast for optimistic
  - [ ] `optimistic_ptr<T> dynamic_pointer_cast(const optimistic_ptr<U>&)` - dynamic cast using local query_interface

- [ ] **Optional async casting functions (BUILD_COROUTINE only, for explicit remote calls)**:
  - [ ] `CORO_TASK(shared_ptr<T>) dynamic_cast_shared(const shared_ptr<U>&)` - explicit async with remote query_interface fallback
  - [ ] `CORO_TASK(optimistic_ptr<T>) dynamic_cast_optimistic(const optimistic_ptr<U>&)` - explicit async for optimistic pointers

**Key Design Principles**:
1. ✅ **Signature compatibility**: All classes have identical signatures with/without BUILD_COROUTINE
2. ✅ **STL compliance**: When TEST_STL_COMPLIANCE=ON, no optimistic_ptr code exists
3. ✅ **Standard semantics**: Heterogeneous assignments work exactly like std::shared_ptr (Derived→Base works)
4. ✅ **Local query_interface**: Existing `dynamic_pointer_cast` uses local query_interface for casting_interface types
5. ✅ **Optional async**: Async versions are additive, for when you explicitly want remote query_interface
6. ✅ **Non-coroutine builds**: All standard operations work identically without BUILD_COROUTINE

**Usage Examples**:
```cpp
// Standard shared_ptr semantics - all synchronous
rpc::shared_ptr<Foo> sp1 = get_foo();
rpc::shared_ptr<Foo> sp2 = sp1;  // ✅ Copy - same type

rpc::shared_ptr<Derived> derived = get_derived();
rpc::shared_ptr<Base> base = derived;  // ✅ Upcast - works synchronously

// Dynamic cast using local query_interface (synchronous)
rpc::shared_ptr<Base> base2 = get_base();
rpc::shared_ptr<Derived> derived2 = rpc::dynamic_pointer_cast<Derived>(base2);  // ✅ Uses local query_interface
if (derived2) {
    // Successfully cast
}

// OPTIONAL: Explicit async for remote query_interface (BUILD_COROUTINE only)
#ifdef BUILD_COROUTINE
rpc::shared_ptr<Base> remote_base = get_remote_base();
rpc::shared_ptr<Derived> remote_derived = CO_AWAIT rpc::dynamic_cast_shared<Derived>(remote_base);  // ✅ Explicit remote query
#endif

// Static casting (synchronous)
rpc::shared_ptr<void> void_ptr = get_void_ptr();
rpc::shared_ptr<Foo> foo_ptr = rpc::static_pointer_cast<Foo>(void_ptr);

// Optimistic pointers work identically
rpc::optimistic_ptr<Derived> opt_derived = get_opt_derived();
rpc::optimistic_ptr<Base> opt_base = opt_derived;  // ✅ Upcast works

// Dynamic cast with optimistic_ptr
rpc::optimistic_ptr<Base> opt_base2 = get_opt_base();
auto opt_derived2 = rpc::dynamic_pointer_cast<Derived>(opt_base2);  // ✅ Local query_interface

// LOCAL OPTIMISTIC POINTER - Making optimistic pointers callable
// IMPORTANT: Stack use only - never as class member
void process_data(rpc::optimistic_ptr<IService> service_ptr) {
    // Create temporary RAII lock for local objects
    rpc::local_optimistic_ptr<IService> local_service(service_ptr);

    if (local_service) {
        // operator-> works for both local (locked) and remote (proxy) objects
        auto result = local_service->process();  // ✅ Makes RPC call

        // If local: local_lock_ keeps object alive during this scope
        // If remote: calls through remote proxy (no locking needed)
    }
    // RAII: local object automatically unlocked when local_service destroyed
}

// Alternative: assignment
void another_example(rpc::optimistic_ptr<IService> service_ptr) {
    rpc::local_optimistic_ptr<IService> local_service;
    local_service = service_ptr;  // Assign optimistic to local

    if (local_service) {
        local_service->do_work();
    }
}

// Check if local or remote
void inspect_locality(rpc::optimistic_ptr<IService> service_ptr) {
    rpc::local_optimistic_ptr<IService> local_service(service_ptr);

    if (local_service.is_local()) {
        // Object is local and currently RAII-locked
    } else {
        // Object is remote (using proxy)
    }
}
```

**Rationale**:
1. **Standard semantics**: Works exactly like std::shared_ptr - developers don't need to learn new patterns
2. **Local query_interface**: Synchronous dynamic_cast uses local query_interface (all casting_interface types support it)
3. **Optional async**: Async versions are additive - use them when you explicitly need remote query_interface
4. **Signature stability**: Classes have same signatures with/without BUILD_COROUTINE
5. **STL compliance**: When TEST_STL_COMPLIANCE=ON, optimistic_ptr doesn't exist and shared_ptr behaves like std::shared_ptr
6. **casting_interface requirement**: When !TEST_STL_COMPLIANCE, all types must derive from casting_interface for query_interface support

### Phase 4b: Dynamic Cast Implementation Details

**Synchronous dynamic_pointer_cast (works with/without BUILD_COROUTINE)**:
```cpp
namespace rpc {

// Synchronous dynamic cast - uses local query_interface
template<typename T, typename U>
shared_ptr<T> dynamic_pointer_cast(const shared_ptr<U>& from) noexcept {
    if (!from) {
        return shared_ptr<T>();
    }

#ifdef TEST_STL_COMPLIANCE
    // STL mode: standard C++ dynamic_cast
    if (T* ptr = dynamic_cast<T*>(from.get())) {
        return shared_ptr<T>(from, ptr);  // Aliasing constructor
    }
    return shared_ptr<T>();
#else
    // RPC mode: try local query_interface first
    if constexpr (__rpc_internal::is_casting_interface_derived<T>::value) {
        // Try local interface query
        T* iface = const_cast<T*>(static_cast<const T*>(
            from->query_interface(T::get_id(VERSION_2))));
        if (iface) {
            return shared_ptr<T>(from, iface);  // Aliasing constructor
        }
    }

    // Fallback to standard C++ dynamic_cast for non-interface types
    if (T* ptr = dynamic_cast<T*>(from.get())) {
        return shared_ptr<T>(from, ptr);
    }

    return shared_ptr<T>();
#endif
}

// Synchronous dynamic cast for optimistic_ptr
template<typename T, typename U>
optimistic_ptr<T> dynamic_pointer_cast(const optimistic_ptr<U>& from) noexcept {
    // Similar implementation using local query_interface
}

#ifdef BUILD_COROUTINE
// OPTIONAL: Async dynamic cast with explicit remote query_interface
template<typename T, typename U>
CORO_TASK(shared_ptr<T>) dynamic_cast_shared(const shared_ptr<U>& from) noexcept {
    if (!from) {
        CO_RETURN shared_ptr<T>();
    }

    // Try local query_interface first
    if constexpr (__rpc_internal::is_casting_interface_derived<T>::value) {
        T* iface = const_cast<T*>(static_cast<const T*>(
            from->query_interface(T::get_id(VERSION_2))));
        if (iface) {
            CO_RETURN shared_ptr<T>(from, iface);
        }

        // Local failed - try remote query_interface through object_proxy
        auto obj_proxy = from->get_object_proxy();
        if (obj_proxy) {
            shared_ptr<T> result;
            CO_AWAIT obj_proxy->template query_interface<T>(result);
            CO_RETURN result;
        }
    }

    CO_RETURN shared_ptr<T>();
}

// OPTIONAL: Async for optimistic_ptr
template<typename T, typename U>
CORO_TASK(optimistic_ptr<T>) dynamic_cast_optimistic(const optimistic_ptr<U>& from) noexcept;
#endif // BUILD_COROUTINE

} // namespace rpc
```

**Key Design Decisions**:
1. **Signature stability**: Same signatures with/without BUILD_COROUTINE
2. **Local query_interface**: Synchronous dynamic_cast uses local query_interface for all casting_interface types
3. **STL compliance**: TEST_STL_COMPLIANCE mode uses standard C++ dynamic_cast only
4. **Optional async**: Async versions (dynamic_cast_shared/dynamic_cast_optimistic) available only with BUILD_COROUTINE
5. **Explicit remote calls**: Developers must explicitly use async versions when they want remote query_interface
6. **Null checks**: All functions check for null input and return null output

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
- [ ] Unit tests for locality detection (local vs remote objects)
- [ ] Multi-threaded safety tests for optimistic reference counting
- [ ] Interoperability tests between optimistic_ptr, shared_ptr, and weak_ptr
- [ ] Transparent operation tests for both local and remote objects
- [ ] Service layer integration tests with optimistic reference counting
- [ ] `OBJECT_GONE` error code testing for optimistic RPC scenarios
- [ ] Telemetry validation for per-zone optimistic reference tracking
- [ ] Integration tests with existing RPC scenarios ensuring no STL test compilation
- [ ] **local_optimistic_ptr specific tests**:
  - [ ] RAII locking tests for local objects
  - [ ] Passthrough tests for remote objects (no locking)
  - [ ] Move semantics tests (move constructor/assignment)
  - [ ] Copy prevention tests (deleted copy constructor/assignment)
  - [ ] Thread safety tests (concurrent access with RAII locking)
  - [ ] Scope-based lifetime tests (object deletion deferred until local_optimistic_ptr destroyed)
  - [ ] Circular dependency resolution tests (child→host calls with no RAII ownership)
- [ ] **Critical Race Condition Tests**:
  - [ ] **RC1**: Concurrent increment_optimistic vs control block destruction
    - Thread 1: Creating first optimistic_ptr from expired weak_ptr
    - Thread 2: Destroying last shared_ptr/weak_ptr simultaneously
    - Expected: try_increment_optimistic() returns false, control block safely destroyed
  - [ ] **RC2**: Concurrent increment_optimistic vs decrement_optimistic
    - Thread 1: Creating optimistic_ptr (increment)
    - Thread 2: Destroying last optimistic_ptr (decrement to 0)
    - Expected: Control block stays alive during both operations
  - [ ] **RC3**: Multiple threads creating first optimistic_ptr
    - Threads 1-N: All try to create first optimistic_ptr from shared_ptr simultaneously
    - Expected: Only one weak_owners increment, no weak_owners leak
  - [ ] **RC4**: Concurrent decrement_shared vs increment_optimistic
    - Thread 1: Destroying last shared_ptr (object disposal)
    - Thread 2: Creating optimistic_ptr from that shared_ptr
    - Expected: Either optimistic_ptr succeeds before disposal, or fails safely after
  - [ ] **RC5**: Concurrent weak_ptr::lock vs optimistic_ptr creation
    - Thread 1: weak_ptr::lock() attempting to create shared_ptr
    - Thread 2: Creating optimistic_ptr from same control block
    - Expected: Both succeed or both fail consistently
  - [ ] **RC6**: Control block destruction race
    - Thread 1: Destroying last optimistic_ptr (decrement weak_owners)
    - Thread 2: Destroying last weak_ptr (decrement weak_owners)
    - Thread 3: Creating new optimistic_ptr via try_increment_optimistic
    - Expected: Control block destroyed exactly once, Thread 3 fails safely
  - [ ] **RC7**: Local object disposal during optimistic_ptr copy
    - Thread 1: Copying local optimistic_ptr (increment_optimistic_no_lock)
    - Thread 2: Destroying last shared_ptr to local object (object disposal)
    - Expected: Copy succeeds, object may be deleted, control block stays alive
  - [ ] **RC8**: Remote proxy cleanup during optimistic reference increment
    - Thread 1: Creating first optimistic_ptr (calling add_ref on object_proxy)
    - Thread 2: Destroying last shared_ptr (calling release on object_proxy)
    - Expected: Object_proxy operations properly serialized, no use-after-free
  - [ ] **RC9**: optimistic_owners wrap-around (stress test)
    - Create/destroy billions of optimistic_ptrs rapidly across threads
    - Expected: No counter overflow, no control block leaks
  - [ ] **RC10**: Interleaved pointer type transitions
    - Thread 1: shared_ptr → optimistic_ptr conversion
    - Thread 2: optimistic_ptr → shared_ptr conversion
    - Thread 3: weak_ptr → optimistic_ptr conversion
    - Expected: All reference counts consistent, no leaks

**Test Coverage Requirements**:
- Local object access patterns (should work transparently)
- Remote object access patterns (should work transparently through proxy)
- Reference counting correctness across pointer type conversions
- Thread-safe concurrent access to optimistic_owners
- Service layer optimistic reference counting with add_ref/release options
- `OBJECT_GONE` scenarios as normal events (not errors)
- Global optimistic reference counters per zone (telemetry validation)
- Control block to object_proxy lifecycle management
- Conditional compilation verification (`!TEST_STL_COMPLIANCE` only)
- **local_optimistic_ptr coverage**:
  - RAII lock acquisition/release for local objects
  - No locking for remote objects (just proxy pointer storage)
  - Move-only semantics enforcement
  - Concurrent access with proper RAII locking preventing premature deletion
  - Stack-only usage (not as class member)

## Technical Specifications

### API Design Principles

**Synchronous vs Asynchronous Operations**:
- **Synchronous (no CO_AWAIT)**:
  - Same-type copy/move constructors and assignments
  - Synchronous pointer casts (`static_pointer_cast`, `const_pointer_cast`, `reinterpret_pointer_cast`)
  - Basic operations (`get()`, `use_count()`, `operator bool()`, `reset()`, `swap()`)

- **Asynchronous (requires CO_AWAIT)**:
  - Heterogeneous type conversions (`convert_to_shared<T, U>`, `convert_to_optimistic<T, U>`)
  - Pointer type conversions (shared ↔ optimistic)
  - Dynamic casting with remote type checking (`dynamic_cast_shared`, `dynamic_cast_optimistic`)

**Changes from Traditional std::shared_ptr**:
- ✅ **No breaking changes**: All std::shared_ptr operations work identically
- ✅ **Added**: `optimistic_ptr<T>` class (only when !TEST_STL_COMPLIANCE)
- ✅ **Enhanced**: `dynamic_pointer_cast` uses local `query_interface` for casting_interface types (when !TEST_STL_COMPLIANCE)
- ✅ **Added**: Optional async `dynamic_cast_shared`/`dynamic_cast_optimistic` for explicit remote queries (BUILD_COROUTINE only)
- ✅ **Kept**: All heterogeneous constructors/assignments work synchronously
- ✅ **Kept**: All casting operations work synchronously using local query_interface
- ✅ **Requirement**: All types must derive from casting_interface (when !TEST_STL_COMPLIANCE)

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

**Scenario 4: local_optimistic_ptr for Callable Optimistic Pointers**
```cpp
// Child object holds optimistic_ptr to host (no RAII ownership - prevents circular dependency)
class ChildNode {
    rpc::optimistic_ptr<IHost> host_;  // Weak reference to host

public:
    void process_task() {
        // Temporarily lock host if local, or use remote proxy if remote
        rpc::local_optimistic_ptr<IHost> local_host(host_);

        if (local_host) {
            // Safe to call - local object is RAII-locked during this scope
            // OR remote object uses proxy (no locking needed)
            auto config = local_host->get_configuration();
            local_host->report_progress(42);

            // If host is local and gets deleted by another thread,
            // the deletion waits until local_host scope ends
        }
        // RAII: local lock released, host can be deleted if no other references
    }
};

// Host manages children with shared_ptr (RAII ownership)
class Host {
    std::vector<rpc::shared_ptr<ChildNode>> children_;  // Host owns children

public:
    void add_child(rpc::shared_ptr<ChildNode> child) {
        children_.push_back(child);
        // Children can call back to host using optimistic_ptr + local_optimistic_ptr
        // No circular dependency: children don't hold RAII references to host
    }
};
```

**Scenario 5: Stateless Service Calls with local_optimistic_ptr**
```cpp
// REST-like service where object lifetime is not client-managed
class ApiClient {
    rpc::optimistic_ptr<IRestService> service_;  // No RAII ownership

public:
    void make_request(const std::string& endpoint) {
        // Create temporary lock for call duration
        rpc::local_optimistic_ptr<IRestService> locked_service(service_);

        if (locked_service) {
            // Call works for both local and remote services
            auto response = locked_service->http_get(endpoint);

            // If service is local and deleted during this call,
            // RAII lock prevents deletion until scope ends
        } else {
            // Service no longer available (returned OBJECT_GONE)
            handle_service_unavailable();
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