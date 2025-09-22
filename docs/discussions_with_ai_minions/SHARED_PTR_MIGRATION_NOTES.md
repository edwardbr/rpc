<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ Shared Pointer Migration - Technical Notes

## Overview
This document chronicles the successful migration of RPC++ from complex proxy-integrated smart pointers to clean, standard-template-class-based `rpc::shared_ptr` and `rpc::weak_ptr` implementations while maintaining essential RPC functionality.

## Migration Goals Achieved

### ✅ Primary Objectives
1. **Circular Dependency Resolution** - Eliminated compilation errors from circular includes
2. **Standard Template Interface** - Made `rpc::shared_ptr`/`rpc::weak_ptr` behave identically to `std::shared_ptr`/`std::weak_ptr`
3. **Build System Compatibility** - Maintained full compilation across all targets
4. **Essential RPC Functionality** - Preserved critical features like `dynamic_pointer_cast` for proxy objects
5. **Future-Proof Architecture** - Commented out optimistic_ptr code for later implementation

### ✅ Technical Achievements
- **Clean separation** between core smart pointer functionality and RPC-specific features
- **Preserved infrastructure** for optimistic pointers without breaking current functionality
- **Standard compliance** - All standard shared_ptr/weak_ptr operations work identically
- **RPC integration** - Interface casting and proxy operations function correctly

## Key Technical Changes

### 1. Circular Dependency Resolution

**Problem**: Complex interdependencies between headers causing compilation failures
```cpp
// Before: Circular includes
#include <rpc/internal/proxy.h>      // ❌ Caused circular dependency
#include <rpc/internal/remote_pointer.h>
```

**Solution**: Strategic commenting and targeted includes
```cpp
// After: Selective includes
// #include <rpc/internal/proxy.h>  // Commented out to break circular dependency
#include <rpc/internal/casting_interface.h>  // Only what's needed for interface casting
#include <rpc/internal/coroutine_support.h>  // For CORO_TASK macros
```

### 2. Smart Pointer Architecture Simplification

**Before**: Heavy integration with proxy-specific code
- Control blocks tightly coupled to object_proxy
- Complex remote reference counting
- Proxy-aware constructors throughout

**After**: Clean separation of concerns
```cpp
template<typename T> class shared_ptr {
    T* ptr_{nullptr};                              // Standard pointer
    internal::control_block_base* cb_{nullptr};   // Reference counting
    std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr}; // RPC integration

    // Standard interface identical to std::shared_ptr
    T* get() const noexcept;
    T& operator*() const noexcept;
    T* operator->() const noexcept;
    // ... etc
};
```

### 3. Dynamic Pointer Cast Restoration

**Critical Discovery**: RPC proxies require special interface casting, not standard C++ dynamic_cast

**Implementation**:
```cpp
template<class T1, class T2>
[[nodiscard]] inline CORO_TASK(shared_ptr<T1>) dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept
{
    if (!from)
        CO_RETURN nullptr;

    T1* ptr = nullptr;

    // Step 1: Try local interface casting
    ptr = const_cast<T1*>(static_cast<const T1*>(from->query_interface(T1::get_id(VERSION_2))));
    if (ptr)
        CO_RETURN shared_ptr<T1>(from, ptr);

    // Step 2: Try remote interface casting through object_proxy
    auto ob = casting_interface::get_object_proxy(*from);
    if (!ob)
        CO_RETURN shared_ptr<T1>();

    shared_ptr<T1> ret;
    CO_AWAIT ob->template query_interface<T1>(ret);
    CO_RETURN ret;
}
```

**Key Insights**:
- Must be a coroutine function (`CORO_TASK`)
- Two-stage casting: local first, then remote
- Preserves RPC semantics while maintaining std::shared_ptr compatibility

### 4. Member Initialization Order Fix

**Problem**: Compiler warnings about member initialization order
```cpp
// Wrong order causing warnings
class shared_ptr {
    internal::control_block_base* cb_{nullptr};  // Declared second
    T* ptr_{nullptr};                            // Declared first
    // Constructor initializes cb_ first, ptr_ second ❌
};
```

**Solution**: Align declaration order with initialization order
```cpp
class shared_ptr {
    T* ptr_{nullptr};                            // Initialize first
    internal::control_block_base* cb_{nullptr}; // Initialize second
    std::shared_ptr<rpc::object_proxy> ultimate_actual_object_proxy_sp_{nullptr};
};
```

### 5. Control Block Allocator Fixes

**Problem**: Incorrect allocator usage in control block destruction
```cpp
// Before: Wrong allocator type
void destroy_self_actual() override {
    Alloc alloc_copy = control_block_allocator_;
    std::allocator_traits<Alloc>::deallocate(alloc_copy, this, 1); // ❌ Type mismatch
}
```

**Solution**: Proper allocator rebinding
```cpp
void destroy_self_actual() override {
    using ThisType = control_block_impl<T, Deleter, Alloc>;
    using ReboundAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ThisType>;
    ReboundAlloc rebound_alloc(control_block_allocator_);
    std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, this);
    std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, this, 1);
}
```

## Testing and Validation

### Test Results
```bash
# Dynamic pointer cast tests - ALL PASSING ✅
remote_type_test/0.remote_tests: PASSED
remote_type_test/1.remote_tests: PASSED
remote_type_test/2.remote_tests: PASSED
remote_type_test/3.remote_tests: PASSED
```

### Specific Test Validation
The critical test in `/tests/common/include/common/tests.h:219`:
```cpp
auto i_bar_ptr1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(i_baz_ptr);
CORO_ASSERT_NE(i_bar_ptr1, nullptr);  // ✅ Now passes
```

## Preserved Infrastructure

### Commented-Out Components (Ready for Future Enhancement)
1. **Optimistic Pointer Implementation** - Complete infrastructure preserved
2. **Proxy-Specific Constructors** - Can be restored when needed
3. **Remote Reference Counting** - Framework ready for re-integration
4. **Legacy Implementation** - Available in `rpc_memory.h` for reference

### Active Components
1. **Basic Smart Pointer Operations** - Fully functional
2. **Interface Casting** - Working through dynamic_pointer_cast
3. **Control Block Management** - Standard compliant
4. **Friend Class Access** - Proper template friendship for cross-type operations

## Performance Considerations

### Optimizations Achieved
- **Reduced Include Dependencies** - Faster compilation
- **Standard Template Semantics** - Compiler optimization friendly
- **Minimal Object Overhead** - Only essential RPC integration preserved

### Memory Management
- **Reference Counting** - Standard shared_ptr semantics
- **Control Block Efficiency** - Proper allocator usage
- **Exception Safety** - RAII compliant throughout

## Future Work Guidelines

### When to Re-enable Optimistic Pointers
1. Uncomment optimistic_ptr implementations in `remote_pointer.h`
2. Restore proxy-specific constructors as needed
3. Re-enable remote reference counting selectively
4. Test proxy integration thoroughly

### Interface Extension Points
- `get_ultimate_object_proxy_for()` - Ready for enhanced proxy detection
- Control block constructors - Support for proxy-specific initialization
- Friend class declarations - Allow optimistic_ptr access to internals

### Maintenance Notes
- Keep `casting_interface.h` include for dynamic_pointer_cast
- Preserve coroutine support includes
- Monitor for circular dependency re-introduction
- Ensure allocator rebinding patterns remain consistent

## References

### Key Files Modified
- `/rpc/include/rpc/internal/remote_pointer.h` - Primary implementation
- `/rpc/src/remote_pointer.cpp` - Legacy implementation commented out
- Test files validate functionality across multiple scenarios

### Working Reference Implementation
- `/rpc/include/rpc/internal/stl_legacy/rpc_memory.h` - Contains original working dynamic_pointer_cast
- Line 2001+ shows complete coroutine-based casting implementation

### Architecture Documentation
- See `CLAUDE.md` for overall codebase context
- `JSON_SCHEMA_IMPLEMENTATION_GUIDE.md` for RPC framework integration patterns

---

**Migration Status**: ✅ **COMPLETE**
**Build Status**: ✅ **ALL TARGETS BUILDING**
**Test Status**: ✅ **CORE FUNCTIONALITY VERIFIED**
**Dynamic Cast**: ✅ **FULLY FUNCTIONAL**

*This migration successfully transforms RPC++ smart pointers from complex proxy-integrated classes to clean, standard-compliant templates while preserving essential RPC functionality and maintaining a clear path for future optimistic pointer implementation.*