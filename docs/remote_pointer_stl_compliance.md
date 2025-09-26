# Remote Pointer STL Compliance Implementation

## Overview

This document describes the implementation work done to make the `rpc::shared_ptr` and `rpc::weak_ptr` classes in `/rpc/include/rpc/internal/remote_pointer.h` compliant with STL specifications for testing purposes.

## Problem Statement

The RPC++ framework includes custom implementations of `shared_ptr` and `weak_ptr` in the `rpc` namespace that are designed for distributed object management. However, to validate that these implementations follow STL semantics correctly, we needed to make them compilable and testable against the Microsoft STL test suite located in `/tests/std_test/`.

The key challenge was that these tests expect the smart pointers to be in the `std` namespace and have access to standard library components like `std::allocator`, `std::default_delete`, etc., but we cannot include `<memory>` since we're implementing an independent version of these components.

## Solution Architecture

The solution uses conditional compilation with the `TEST_STL_COMPLIANCE` macro to provide two modes:

1. **Production Mode** (`#ifndef TEST_STL_COMPLIANCE`): Classes are in `rpc` namespace with RPC-specific functionality
2. **Test Mode** (`#ifdef TEST_STL_COMPLIANCE`): Classes are in `std` namespace with STL-compliant interfaces

## Key Components Implemented

### 1. STL Foundation Types (TEST_STL_COMPLIANCE mode only)

```cpp
namespace std {
    // Default deleters for scalar and array types
    template<typename T> struct default_delete;
    template<typename T> struct default_delete<T[]>;

    // Exception type for expired weak_ptr access
    class bad_weak_ptr : public std::exception;

    // Minimal unique_ptr implementation
    template<typename T, typename Deleter = default_delete<T>>
    class unique_ptr;

    // Factory function for unique_ptr
    template<typename T, typename... Args>
    unique_ptr<T> make_unique(Args&&... args);

    // Lightweight packaged_task with CTAD coverage for callable wrappers
    template<typename Signature>
    class packaged_task;

    // owner_less predicate mirroring <memory> semantics with CTAD support
    template<typename T = void>
    struct owner_less;
}
```

### 2. Core Smart Pointer Classes

Both `shared_ptr<T>` and `weak_ptr<T>` are implemented with:

- **Thread-safe reference counting** using atomic operations
- **Custom control blocks** for different allocation strategies
- **Aliasing constructor support** for shared ownership of different objects
- **Type-safe conversions** between compatible pointer types

### 3. Memory Management Features

- **`make_shared<T>`**: Efficient single-allocation construction
- **`allocate_shared<T>`**: Custom allocator support
- **Multiple constructor overloads**: Raw pointer, deleter, allocator combinations
- **Exception safety**: RAII and strong exception safety guarantees

### 4. STL Compliance Features

- **Hash specializations**: Support for `std::unordered_map` and `std::unordered_set`
- **Comparison operators**: Full set of relational operators
- **Conversion constructors & deduction guides**: Support for `unique_ptr` to `shared_ptr` conversion with CTAD, including array types
- **Aliasing constructors**: Share ownership while pointing to different objects
- **`owner_less` predicate**: Matches standard ordering semantics for associative containers

## Technical Challenges Resolved

### 1. Namespace Conflicts
**Problem**: Need classes in both `rpc` and `std` namespaces
**Solution**: Conditional compilation blocks with careful namespace management

### 2. Missing STL Dependencies
**Problem**: Cannot include `<memory>` but need `std::default_delete`, `std::allocator_traits`, etc.
**Solution**: Implemented minimal but compliant versions of required STL components

### 3. Constructor Ambiguity
**Problem**: Multiple constructors with similar signatures causing compilation errors
**Solution**: Removed duplicate constructor overloads and refined SFINAE constraints

### 4. Friend Function Access
**Problem**: `allocate_shared` needs access to private constructors
**Solution**: Added proper friend declarations for factory functions

### 5. Return Type Consistency
**Problem**: `weak_ptr::lock()` returning wrong type in test mode
**Solution**: Conditional return types based on compilation mode

### 6. Array Element Type Handling
**Problem**: Array specialisations stored raw pointer-to-array and broke CTAD
**Solution**: Store `remove_extent_t` everywhere internally so deduction guides and comparisons match the standard

### 7. `unique_ptr` Conversion Coverage
**Problem**: Production builds compile against the real `<memory>` and rejected our CTAD guide
**Solution**: Express deduction guides using fully qualified `std::unique_ptr` so both modes agree

### 8. `enable_shared_from_this` Copy Safety
**Problem**: Copying or assigning derived objects overwrote the stored `weak_ptr`, breaking `shared_from_this`
**Solution**: Provide no-op copy/move operations in `enable_shared_from_this` so the hidden weak state is never propagated

### 9. Allocator Propagation in `allocate_shared`
**Problem**: Custom allocators (for example `Mallocator`) were ignored, leading to wrong allocation counters
**Solution**: Rebind and reuse the caller-provided allocator for both control block and object storage

## Files Modified

- `/rpc/include/rpc/internal/remote_pointer.h` - Main implementation
- `/tests/std_test/CMakeLists.txt` - Build configuration for STL tests

## Test Results

Successfully compiles and passes key MSVC STL conformance tests, including:
- `Dev10_445289_make_shared` (factory functions & reference counting)
- `P0433R2_deduction_guides` (CTAD for smart pointers, owner_less, packaged_task)

These cover:
- `make_shared<T>()` and `allocate_shared<T>()` with allocators
- `shared_ptr` / `weak_ptr` basic and array semantics
- CTAD for `shared_ptr`, `weak_ptr`, `owner_less`, `packaged_task`
- Memory management, reference counting, and exception safety guarantees

## Build Issues Resolved

### Function Pointer Handling (FIXED)
**Problem**: Invalid conversions between function pointers and `void*` in control blocks
**Root Cause**: Function pointers cannot be implicitly converted to `void*` in C++
**Solution**:
- Added explicit `static_cast<void*>()` conversions when storing pointers in control blocks
- Used `if constexpr (std::is_function_v<T>)` to handle function types with `reinterpret_cast`
- Applied `static_cast` for regular object pointers

**Code Changes**:
```cpp
// Fixed constructor conversion
control_block_impl(T* p, Deleter d, Alloc a)
    : control_block_base(static_cast<void*>(p))  // explicit cast

// Fixed disposal with type checking
void dispose_object_actual() override {
    if (managed_object_ptr_) {
        T* obj_ptr;
        if constexpr (std::is_function_v<T>) {
            obj_ptr = reinterpret_cast<T*>(managed_object_ptr_);
        } else {
            obj_ptr = static_cast<T*>(managed_object_ptr_);
        }
        object_deleter_(obj_ptr);
        managed_object_ptr_ = nullptr;
    }
}
```

## Usage

### Building STL Tests
```bash
cmake --build build --target Dev10_445289_make_shared
./build/output/debug/Dev10_445289_make_shared
```

### Compilation Modes
- **Production**: Uses `rpc::shared_ptr` and `rpc::weak_ptr`
- **Testing**: Define `TEST_STL_COMPLIANCE` to use `std::shared_ptr` and `std::weak_ptr`

## Future Work

1. **Fix Function Pointer Handling**: Resolve the diagnostic warnings about function pointer storage
2. **Expand Test Coverage**: Enable and fix additional STL tests in the test suite
3. **Performance Validation**: Ensure the implementation meets performance requirements
4. **Thread Safety Testing**: Validate atomic operations under concurrent access

## Design Principles

- **No Standard Library Dependencies**: Complete independence from `<memory>`
- **STL Semantic Compatibility**: Behavior matches standard library expectations
- **Thread Safety**: All operations are thread-safe using atomic primitives
- **Exception Safety**: Strong exception safety guarantees throughout
- **Minimal Overhead**: Efficient implementation suitable for distributed systems

This implementation provides a solid foundation for both production RPC usage and STL compliance testing, ensuring that the custom smart pointer implementations follow established C++ standards while maintaining the specific requirements of the RPC++ framework.
