<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ User Guide

**A Modern C++ Remote Procedure Call Library**

*Version 2.2.0*

---

## Table of Contents

1. [Introduction](#introduction)
2. [Getting Started](#getting-started)
3. [Execution Modes: Blocking vs Coroutines](#execution-modes-blocking-vs-coroutines)
4. [Interface Definition Language (IDL)](#interface-definition-language-idl)
5. [Architecture Overview](#architecture-overview)
6. [Error Code System](#error-code-system)
7. [Wire Protocol and Backward Compatibility](#wire-protocol-and-backward-compatibility)
8. [Transport Layer](#transport-layer)
9. [Child Services and Service Proxy Architecture](#child-services-and-service-proxy-architecture)
10. [Build System and Configuration](#build-system-and-configuration)
11. [Logging and Telemetry](#logging-and-telemetry)
12. [Advanced Features](#advanced-features)
13. [Best Practices](#best-practices)

---

## Introduction

RPC++ is a modern C++ Remote Procedure Call library designed for type-safe communication across different execution contexts including in-process calls, inter-process communication, remote machines, embedded devices, and secure enclaves (SGX). The architecture supports multiple programming languages through IDL-based code generation and standardized wire protocols.

### Key Features

- **Type-Safe**: Full C++ type system integration with compile-time verification
- **Transport Agnostic**: Works across processes, threads, memory arenas, enclaves, and networks
- **High Performance**: Zero-copy serialization with optimized data paths
- **Bi-Modal Execution**: Same code runs in both blocking and coroutine modes (see [Execution Modes](#execution-modes-blocking-vs-coroutines))
- **Modern C++**: C++17 features with optional coroutine support
- **Production Ready**: Comprehensive telemetry, logging, and error handling
- **Backward Compatible**: Automatic protocol version negotiation and interface fingerprinting

### What are Remote Procedure Calls?

Remote Procedure Calls (RPC) enable applications to communicate with each other without getting entangled in underlying communication protocols. You can easily create APIs accessible from different machines, processes, arenas, or embedded devices without worrying about serialization.

**The RPC Flow:**
1. **Caller** → calls function through a **Proxy** (same signature as implementation)
2. **Proxy** → packages call and sends to intended target
3. **Target** → receives call through **Stub** that unpacks request
4. **Stub** → calls actual function on caller's behalf
5. **Return** → results flow back through the same path


---

## Getting Started

### Prerequisites

- **C++ Compiler**: Clang 10+, GCC 9.4+, or Visual Studio 2017+
- **CMake**: Version 3.24 or higher
- **Build System**: Ninja (recommended) or Make

### Quick Start

1. **Clone and Configure:**
```bash
git clone https://github.com/edwardbr/rpc
cd rpc2
cmake --preset Debug
```

2. **Build Core Library:**
```bash
cmake --build build --target rpc
```

3. **Run Tests:**
```bash
cmake --build build --target rpc_test
./build/tests/test_host/rpc_test
```

### Basic Example

**calculator.idl:**
```idl
namespace calculator {
    [inline] namespace v1 {
        [status=production]
        interface i_calculator {
            int add(int a, int b, [out] int& result);
            int subtract(int a, int b, [out] int& result);
        };
    }
}
```

**Generated usage:**
```cpp
#include "generated/calculator/calculator.h"

// Create root service
auto root_service = std::make_shared<rpc::service>("root", rpc::zone{1});

// Create child zone with calculator implementation
rpc::shared_ptr<calculator::v1::i_calculator> calc_proxy;
auto error = CO_AWAIT root_service->connect_to_zone<rpc::local_child_service_proxy<calculator::v1::i_calculator, calculator::v1::i_calculator>>(
    "calculator_zone",
    rpc::zone{2},
    rpc::shared_ptr<calculator::v1::i_calculator>(),
    calc_proxy,
    [](const rpc::shared_ptr<calculator::v1::i_calculator>& input,
       rpc::shared_ptr<calculator::v1::i_calculator>& output,
       const std::shared_ptr<rpc::service>& child_service) -> int
    {
        // Register generated stubs in child zone
        calculator_v1_idl_register_stubs(child_service);

        // Create implementation in child zone
        output = rpc::make_shared<calculator_impl>();
        return rpc::error::OK();
    });

// Use the proxy
if (error == rpc::error::OK()) {
    int result;
    auto calc_error = CO_AWAIT calc_proxy->add(5, 3, result);
    if (calc_error == rpc::error::OK()) {
        std::cout << "5 + 3 = " << result << std::endl;
    }
}

// Implementation class
class calculator_impl : public calculator::v1::i_calculator {
    // Required for remote dynamic casting (needed for clang/gcc, not MSVC)
    void* get_address() const override { return (void*)this; }

    // Required for casting to different vtables in derived classes
    const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override {
        if (rpc::match<calculator::v1::i_calculator>(interface_id))
            return static_cast<const calculator::v1::i_calculator*>(this);
        return nullptr;
    }

public:
    int add(int a, int b, int& result) override {
        result = a + b;
        return rpc::error::OK();
    }

    int subtract(int a, int b, int& result) override {
        result = a - b;
        return rpc::error::OK();
    }
};
```

---

## Execution Modes: Blocking vs Coroutines

RPC++ is designed to run the **same code** in both blocking and coroutine modes with minimal changes. This is achieved through conditional macros that adapt to the build configuration:

### Blocking Mode (`BUILD_COROUTINE=OFF`)

```cpp
// Macros resolve to blocking equivalents
auto error = CO_AWAIT service->connect_to_zone<...>(...);
// Becomes: auto error = service->connect_to_zone<...>(...);

CORO_TASK(int) my_function() { ... }
// Becomes: int my_function() { ... }

CO_RETURN result;
// Becomes: return result;
```

### Coroutine Mode (`BUILD_COROUTINE=ON`)

```cpp
// Macros resolve to coroutine equivalents
auto error = CO_AWAIT service->connect_to_zone<...>(...);
// Becomes: auto error = co_await service->connect_to_zone<...>(...);

CORO_TASK(int) my_function() { ... }
// Becomes: coro::task<int> my_function() { ... }

CO_RETURN result;
// Becomes: co_return result;
```

### Benefits of Bi-Modal Design

1. **Easy Testing and Debugging**: Unit tests run in blocking mode with standard debugger support
2. **Production Performance**: Coroutine mode provides async/await performance benefits
3. **Code Reuse**: Same application code works in both modes
4. **Gradual Migration**: Switch between modes by changing build configuration
5. **Transport Flexibility**: Some transports require coroutines (TCP, SPSC), others work in both modes

### Usage Pattern

```cpp
// This code works in both blocking and coroutine modes
CORO_TASK(int) setup_calculator_service() {
    auto root_service = std::make_shared<rpc::service>("root", rpc::zone{1});

    // CRITICAL: Keep output interface alive to prevent zone destruction
    rpc::shared_ptr<i_calculator> calc;

    auto error = CO_AWAIT root_service->connect_to_zone<rpc::local_child_service_proxy<i_calculator, i_calculator>>(
        "calc_zone",                           // Zone name
        rpc::zone{2},                         // Zone ID
        rpc::shared_ptr<i_calculator>(),      // Input interface (passed TO the zone)
        calc,                                 // Output interface (passed FROM the zone)
        setup_callback);                      // Setup callback

    if (error != rpc::error::OK()) {
        CO_RETURN error;
    }

    int result;
    error = CO_AWAIT calc->add(10, 20, result);
    CO_RETURN error;

    // Zone stays alive as long as 'calc' is held
    // Zone destroys itself via RAII when no shared_ptrs remain
}
```

### Zone Lifecycle Management

**Critical RAII Behavior**: Zones destroy themselves automatically when no `rpc::shared_ptr` objects referencing them remain alive. At least one of the following must be held:

- **Input interface** (3rd parameter): Interface passed TO the zone
- **Output interface** (4th parameter): Interface passed FROM the zone
- **Any other** `rpc::shared_ptr` to objects within the zone

```cpp
// DANGER: Zone will destroy itself immediately
{
    rpc::shared_ptr<i_calculator> temp_calc;
    auto error = CO_AWAIT service->connect_to_zone<...>(..., temp_calc, ...);
    // temp_calc goes out of scope - zone destroys itself!
}

// SAFE: Keep interface alive to maintain zone
rpc::shared_ptr<i_calculator> persistent_calc;
auto error = CO_AWAIT service->connect_to_zone<...>(..., persistent_calc, ...);
// Zone stays alive as long as persistent_calc is held
```

### Build Configuration

```bash
# For unit testing and debugging (blocking mode)
cmake --preset Debug -DBUILD_COROUTINE=OFF

# For production deployment (coroutine mode)
cmake --preset Debug -DBUILD_COROUTINE=ON
```

---

## Interface Definition Language (IDL)

The IDL system defines interfaces, data structures, and metadata that drive code generation and wire protocol compatibility.

### Core Syntax

**Namespaces:**
```idl
namespace company {
    [inline] namespace service_name {
        [inline] namespace v1 {
            // Interface definitions here
        }
    }
}
```

**Interfaces:**
```idl
[status=production, description="Calculator service interface"]
interface i_calculator {
    [description="Adds two integers"]
    int add(int a, int b, [out] int& result);

    [deprecated]  // Marks method as deprecated (fingerprint-safe)
    int legacy_add(int a, int b, [out] int& result);
};
```

**Data Structures:**
```idl
[status=production]
struct calculation_request {
    double operand_a;
    double operand_b;
    std::string operation;
    int precision = 2;  // Default values supported
};
```

### Supported Types

- **Primitives**: `int`, `double`, `bool`, `std::string`
- **Containers**: `std::vector<T>`, `std::map<K,V>`, `std::array<T,N>`
- **Optionals**: `std::optional<T>`, `std::variant<T...>`
- **RPC Types**: `rpc::shared_ptr<interface>` for object passing
- **Custom Types**: User-defined structs and classes

### RPC Type Limitations

**`rpc::shared_ptr` Restriction**: Currently, `rpc::shared_ptr` objects cannot be embedded within structures or other data types. They can only be used as direct parameters in interface function signatures:

```cpp
// SUPPORTED: rpc::shared_ptr as interface function parameters
[status=production]
interface i_service_manager {
    error_code register_service([in] rpc::shared_ptr<i_service> service);
    error_code get_service([out] rpc::shared_ptr<i_service>& service);
    error_code call_service([in] const rpc::shared_ptr<i_service>& service, int data);
};

// NOT SUPPORTED: rpc::shared_ptr embedded in structures
struct service_info {
    std::string name;
    rpc::shared_ptr<i_service> service;  // ❌ NOT CURRENTLY SUPPORTED
    int priority;
};

// WORKAROUND: Use separate parameters instead of embedding
[status=production]
interface i_service_manager {
    error_code register_service_info([in] const std::string& name,
                                   [in] rpc::shared_ptr<i_service> service,
                                   [in] int priority);
};
```

This limitation exists because `rpc::shared_ptr` requires special marshalling logic for distributed object lifecycle management that is currently only implemented at the interface parameter level.

### Parameter Attributes

- **`[in]`**: Input parameter (default for value types)
- **`[out]`**: Output parameter (must be reference)

### Parameter Reference Types

RPC++ supports various reference types for different parameter passing semantics:

```cpp
[status=production]
interface i_data_processor {
    // Value parameters (implicit [in])
    error_code process_simple(int value, std::string data);

    // Explicit [in] reference parameters
    error_code process_by_ref([in] const std::string& data);

    // Rvalue reference parameters for move semantics
    error_code process_by_move([in] std::string&& data);
    error_code process_complex_move([in] ComplexData&& data);

    // Output reference parameters
    error_code get_result([out] std::string& result);
    error_code get_complex_result([out] ComplexData& result);

    // Mixed parameter types
    error_code process_mixed([in] const std::string& input,
                           [in] std::vector<int>&& bulk_data,
                           [out] ProcessResult& result);
};
```

**Rvalue Reference Benefits**:
- **Server-Side Efficiency**: Object is deserialized once in the stub and moved directly into application logic
- **Zero-Copy Transfer**: Eliminates intermediate copying between stub and implementation
- **Memory Optimization**: Reduces allocations by reusing deserialized objects
- **Modern C++**: Leverages C++11+ move semantics in RPC calls

**Performance Advantage in Stub**:
```cpp
// Server implementation receives moved object
class data_processor_impl : public i_data_processor {
public:
    error_code process_by_move(std::string&& data) override {
        // 'data' is moved directly from stub deserialization
        // No additional copy - object built once in stub
        stored_data_ = std::move(data);  // Efficient move
        return process_internal();
    }

    error_code process_complex_move(ComplexData&& complex_data) override {
        // Large object moved directly from wire format
        // Stub deserializes once, moves to implementation
        return complex_processor_.process(std::move(complex_data));
    }

private:
    std::string stored_data_;
    ComplexProcessor complex_processor_;
};
```

**Usage Pattern**:
```cpp
// Client code can pass temporaries efficiently
std::string large_data = generate_large_string();
auto error = CO_AWAIT processor->process_by_move(std::move(large_data));

// Or pass temporary objects directly
auto error2 = CO_AWAIT processor->process_by_move(generate_temporary_string());

// Complex objects with move constructors
ComplexData complex = build_complex_data();
auto error3 = CO_AWAIT processor->process_complex_move(std::move(complex));
```

### Interface Lifecycle Attributes

- **`[status=development]`**: Interface under development (fingerprint changes allowed)
- **`[status=production]`**: Stable interface (fingerprint locked by CI/CD)
- **`[status=deprecated]`**: Legacy interface (fingerprint preserved)

---

## Architecture Overview

### Core Components

```cpp
// High-level architecture
┌─────────────────┐    ┌─────────────────┐
│   Application   │    │   Application   │
│     (Client)    │    │    (Server)     │
├─────────────────┤    ├─────────────────┤
│  Generated      │    │  Generated      │
│  Proxy          │◄──►│  Stub           │
├─────────────────┤    ├─────────────────┤
│  Service Proxy  │    │  Service Proxy  │
│  (Transport)    │    │  (Transport)    │
├─────────────────┤    ├─────────────────┤
│  i_marshaller   │    │  i_marshaller   │
│  (Protocol)     │    │  (Protocol)     │
└─────────────────┘    └─────────────────┘
```

### Service Architecture

**Service Proxy Types:**
- **In-Memory**: Direct function calls within the same process
- **SGX Enclave**: Secure communication with SGX enclaves
- **TCP**: Network communication (coroutine builds only)
- **SPSC**: Single producer/single consumer inter-process communication
- **Arena**: Communication between different memory arenas

### Zone Management and Proxy Creation

RPC++ uses a **zone-based architecture** where each service instance operates in its own zone. Zones communicate through **service proxies** created using three main APIs:

#### 1. Service Instantiation

The root service is instantiated directly:

```cpp
// Create root service (only service class instantiated directly)
auto root_service = std::make_shared<rpc::service>("root_service", rpc::zone{1});

// With coroutine support
#ifdef BUILD_COROUTINE
auto root_service = std::make_shared<rpc::service>("root_service", rpc::zone{1}, io_scheduler);
#endif
```

#### 2. connect_to_zone - Child Zone Creation

Creates child zones that inherit from the calling service:

```cpp
// Create child zone with local service proxy
rpc::shared_ptr<i_calculator> child_calc;
auto error = CO_AWAIT root_service->connect_to_zone<rpc::local_child_service_proxy<i_calculator, i_calculator>>(
    "calc_zone",                    // Zone name
    rpc::zone{2},                   // Child zone ID
    rpc::shared_ptr<i_calculator>(), // Input interface (nullptr = no input)
    child_calc,                     // Output interface
    [](const rpc::shared_ptr<i_calculator>& input,
       rpc::shared_ptr<i_calculator>& output,
       const std::shared_ptr<rpc::service>& child_service) -> int
    {
        // Register stubs in child zone
        calculator_idl_register_stubs(child_service);

        // Create implementation in child zone
        output = rpc::make_shared<calculator_impl>();
        return rpc::error::OK();
    });
```

#### 3. attach_remote_zone - Peer Zone Connection

Connects to peer zones that are not children of the local service:

```cpp
// Attach to remote peer zone
rpc::shared_ptr<i_remote_service> remote_service;
rpc::interface_descriptor output_desc;
auto error = CO_AWAIT service->attach_remote_zone<rpc::spsc::service_proxy, i_host, i_remote_service>(
    "remote_zone",
    input_descriptor,       // Interface descriptor from peer
    output_desc,           // Interface descriptor to send back
    [](const rpc::shared_ptr<i_host>& host,
       rpc::shared_ptr<i_remote_service>& remote,
       const std::shared_ptr<rpc::service>& service) -> CORO_TASK(int)
    {
        // Create local implementation that can use remote host
        remote = rpc::make_shared<remote_service_impl>(host);
        CO_RETURN rpc::error::OK();
    },
    transport_args...);
```

#### 4. create_child_zone - Static Child Creation

Static method for creating child zones from within child services:

```cpp
// Used within child service implementations
std::shared_ptr<rpc::child_service> new_child;
auto error = CO_AWAIT rpc::child_service::create_child_zone<rpc::host_service_proxy, i_host, i_example>(
    "enclave_zone",
    rpc::zone{3},
    parent_zone_id,
    input_descriptor,
    output_descriptor,
    setup_callback,
#ifdef BUILD_COROUTINE
    io_scheduler,
#endif
    new_child,
    transport_args...);
```

### Available Service Proxy Types

**1. local_child_service_proxy<PARENT_INTERFACE, CHILD_INTERFACE>**
- Direct function calls within the same process
- Parent-child relationship with automatic cleanup

**2. enclave_service_proxy**
- Secure communication with SGX enclaves
- Hardware-enforced security boundaries

**3. host_service_proxy**
- Communication from enclave to untrusted host
- Used within enclave implementations

**4. spsc::service_proxy** (Coroutine builds only)
- Single producer/single consumer inter-process communication
- High-performance shared memory transport

**5. tcp::service_proxy** (Coroutine builds only)
- Network communication over TCP
- Proof of concept implementation

### Object Lifecycle Management

Objects are automatically managed across zones using `rpc::shared_ptr<interface>`:

```cpp
// Objects created in one zone
rpc::shared_ptr<i_business_object> obj;
factory->create_object(obj);

// Automatically marshalled when passed to other zones
worker->process_object(obj);  // add_ref/release handled automatically
```

**Reference Counting:**
- Automatic add_ref/release across zone boundaries
- Bidirectional reference tracking for cleanup
- Race condition detection and handling

---

## Reference Counting and Object Lifecycle

RPC++ implements a sophisticated distributed reference counting system that manages object lifetimes across multiple zones. Understanding this system is crucial for debugging distributed object lifecycle issues.

### Architecture Components

The reference counting system consists of four main layers working together:

#### 1. Interface Proxies (`i_example_proxy`, etc.)
- **Purpose**: Type-safe client-side proxies for specific interfaces (e.g., `i_calculator_proxy`, `i_user_service_proxy`)
- **Location**: Lives in the client zone where the smart pointer is held
- **User View**: What developers interact with through `rpc::shared_ptr<i_example>` or `rpc::optimistic_ptr<i_example>`
- **Creation**: Interface proxies are automatically generated and managed by the RPC subsystem - developers never directly instantiate them
- **Lifetime Management**:
  - Developers create `rpc::shared_ptr<i_example>` or `rpc::optimistic_ptr<i_example>` to hold interface proxies
  - The lifetime of the interface proxy (or local object) is controlled by these smart pointers
  - These smart pointers automatically control the remote reference count of the destination stub
  - **Both `rpc::shared_ptr` and `rpc::optimistic_ptr` keep the communication channel open** (service proxy remains alive)
  - `rpc::shared_ptr` keeps the remote stub alive (strong reference semantics)
  - `rpc::optimistic_ptr` does NOT keep the remote stub alive (weak reference semantics)
  - **Key insight**: An `rpc::optimistic_ptr` in Zone A can successfully call a remote object in Zone C that is kept alive by an `rpc::shared_ptr` in Zone B - the communication channel remains open even though Zone A doesn't own the object
- **Responsibilities**:
  - Provides type-safe method calls matching the interface definition
  - Delegates to underlying object proxy for actual RPC communication
  - Multiple interface proxies can point to the same remote object (fake multiple inheritance)

*Note: Interface proxies are backed by control blocks that track local `rpc::shared_ptr` and `rpc::weak_ptr` reference counts, but these details are typically transparent to application code.*

#### 2. Object Proxy (`object_proxy`)
- **Purpose**: Aggregates ALL interface proxies pointing to the same remote object in a given zone
- **Location**: One per zone per remote object
- **Counters**:
  - `shared_count_`: Aggregate count of shared references across all interfaces for this object
  - `optimistic_count_`: Aggregate count of optimistic references across all interfaces for this object
- **Responsibilities**:
  - Pools multiple interface types to same object (supports "fake multiple inheritance")
  - Sends `sp_add_ref()` / `sp_release()` to service proxies only on 0→1 and 1→0 aggregate transitions
  - Manages relationship with `service_proxy` for remote communication
- **Critical Behavior**: When a reference type count (shared or optimistic) goes from 0→1, must call `service_proxy->sp_add_ref()` to establish remote reference **immediately and sequentially** to ensure remote count ≥ 1

#### 3. Service Proxies (Relay Layer)
- **Purpose**: Relay reference counting operations between zones (caller → intermediates → destination)
- **Location**: One per (source zone, destination zone, caller zone) triplet
- **Counters**:
  - `lifetime_lock_count_`: External references keeping this service_proxy alive
  - Incremented by `add_external_ref()`, decremented by `inner_release_external_ref()`
- **Reference Relay Behavior**:
  - **Owning Proxies** (zone_id == caller_zone_id): Directly manage objects in the destination zone
  - **Routing Proxies** (zone_id != caller_zone_id): Act as intermediaries, relaying reference counts between zones
  - Service proxies forward `sp_add_ref()` and `sp_release()` calls through the zone hierarchy
  - Reference counts accumulate at the destination stub from all calling zones
- **Lifecycle**:
  - Created via `service::inner_add_zone_proxy()` which calls `add_external_ref()` (count = 1)
  - Each object_proxy 0→1 transition calls `add_external_ref()` again
  - Destructor removes from `other_zones` map if `is_responsible_for_cleaning_up_service_` is true
- **Critical Fix**: Routing service_proxies don't have `cleanup_after_object()` called, so destructor must check this condition and call `remove_zone_proxy()` to prevent null `weak_ptr` lookups

**Multi-Zone Reference Relay Example**:
```
Zone A (Caller) → Zone B (Intermediate) → Zone C (Destination)
     │                    │                       │
Interface Proxy      Routing Proxy          Owning Proxy
shared_count_=1    (relays ref counts)     shared_count_=1
     │                    │                       │
     └───── sp_add_ref() ─┴──── sp_add_ref() ────┴──→ Remote Stub
                                                      shared_count_=1
```

#### 4. Remote Stub (`object_stub`)
- **Purpose**: Represents the actual object implementation in the service zone
- **Location**: Lives in the zone where the actual object implementation exists
- **Counters**:
  - `shared_count_`: Total shared references from ALL zones combined (relayed through service proxies)
  - `optimistic_count`: Total optimistic references from ALL zones combined
- **Deletion Rule**: Stub is deleted when `shared_count_` reaches 0, **regardless** of `optimistic_count` value (weak semantics)

### Reference Counting Flow

#### Creating a Remote Object Reference

When `rpc::shared_ptr<i_foo> foo_proxy` is created to a remote object:

1. **Interface Proxy Creation**:
   - Application creates `rpc::shared_ptr<i_foo>` pointing to remote object
   - Interface proxy (e.g., `i_foo_proxy`) is instantiated with backing control block
   - Control block delegates to object proxy for distributed reference management

2. **Object Proxy Aggregation**:
   - Object proxy's `shared_count_` increments from 0→1
   - Detects 0→1 transition (first reference to this object in this zone)
   - Calls `service_proxy->add_external_ref()` (keeps service_proxy alive)
   - Calls `CO_AWAIT service_proxy->sp_add_ref(object_id, options::normal, ref_count)`

3. **Service Proxy Relay** (Caller → Intermediates → Destination):
   - Service proxies relay `sp_add_ref()` through zone hierarchy
   - **Routing Proxies** (intermediates): Forward call to next zone without owning the object
   - **Owning Proxy** (destination): Makes final call to local stub
   - Each intermediate zone's service proxy remains alive to maintain relay path

4. **Remote Stub Update**:
   - Destination zone's `object_stub->add_ref(is_optimistic=false)` increments `shared_count_`
   - Stub remains alive as long as `shared_count_ > 0`
   - Reference count represents total from ALL calling zones combined

#### Destroying a Remote Object Reference

When `rpc::shared_ptr<i_foo> foo_proxy` is destroyed:

1. **Interface Proxy Destruction**:
   - Application destroys `rpc::shared_ptr<i_foo>`
   - Control block reference count decrements
   - When count reaches 0, delegates to object proxy for cleanup

2. **Object Proxy Cleanup**:
   - Object proxy's `shared_count_` decrements from 1→0
   - Detects 1→0 transition (last reference in this zone)
   - Calls `service_proxy->on_object_proxy_released(shared_from_this(), is_optimistic=false)`

3. **Service Proxy Cleanup** (`cleanup_after_object`):
   - Calls `CO_AWAIT service_proxy->sp_release(object_id, release_options::normal, ref_count)`
   - Service proxies relay `sp_release()` through zone hierarchy to destination
   - Calls `inner_release_external_ref()` (decrements `lifetime_lock_count_`)
   - If `inner_count == 0 && is_responsible_for_cleaning_up_service_`, calls `svc->remove_zone_proxy()`
   - Removes object_proxy from `proxies_` map (done in `on_object_proxy_released` before scheduling cleanup)

4. **Remote Stub Cleanup**:
   - Destination stub's `shared_count_` decrements
   - When `shared_count_ == 0 && !is_optimistic`, stub is deleted from service
   - Stub memory is freed regardless of `optimistic_count` value
   - Intermediate routing service proxies remain alive as long as other objects use them

### Dual Reference Counting (Shared vs Optimistic)

RPC++ supports two reference types with different lifetime semantics:

**Shared References** (`rpc::shared_ptr`):
- Keep remote stubs alive (RAII semantics)
- Stub deleted when aggregate `shared_count_` across all zones reaches 0
- Use for ownership semantics

**Optimistic References** (`rpc::optimistic_ptr` - Future Feature):
- Do NOT keep remote stubs alive (weak semantics)
- Stub can be deleted even if `optimistic_count > 0`
- Calling through optimistic_ptr after stub deletion returns `OBJECT_GONE`
- Use for circular dependencies, stateless services, or non-owning access

**Key Invariant**: Both reference types are tracked independently through the entire stack:
- Control block has `shared_count_` and `optimistic_count_`
- Object proxy has `shared_count_` and `optimistic_count_`
- Remote stub has `shared_count_` and `optimistic_count`

**Critical Requirement**: `sp_add_ref()` is called **once for each type** when going 0→1:
- First shared reference: calls `sp_add_ref(..., normal)`
- First optimistic reference: calls `sp_add_ref(..., optimistic)`
- This ensures remote service tracks both types independently

### Common Debugging Scenarios

#### Scenario 1: "tmp is null" Assertion Failures

**Symptom**: Crash with error "tmp is null zone: X destination_zone=Y"

**Root Cause**: Service proxy was destroyed but its `weak_ptr` entry remains in `other_zones` map

**Diagnosis Steps**:
1. Check if service_proxy destructor was called (add logging/telemetry)
2. Verify if `remove_zone_proxy()` was called in destructor
3. For routing proxies (`zone_id != caller_zone_id`), verify destructor checks this condition
4. Check if `is_responsible_for_cleaning_up_service_` is set correctly

**Fix Pattern** (from recent debugging):
```cpp
// In service_proxy destructor
if (is_responsible_for_cleaning_up_service_)
{
    auto svc = service_.lock();
    if (get_zone_id().as_caller() != caller_zone_id_)  // Routing proxy check
    {
        svc->remove_zone_proxy(destination_zone_id_, caller_zone_id_);
    }
}
```

**Key Lesson**: When encountering null `weak_ptr` lookups, check if the object's destructor properly removed the `weak_ptr` from all relevant collections.

#### Scenario 2: Premature Object Deletion

**Symptom**: Remote calls fail with `OBJECT_NOT_FOUND` or `OBJECT_GONE` even though references should exist

**Diagnosis**:
1. Enable telemetry logging for reference counts
2. Trace `sp_add_ref` and `sp_release` calls for the object
3. Check if reference counts balance (adds == releases)
4. Verify no double-release bugs (releasing same reference twice)

**Common Causes**:
- Missing `add_external_ref()` call when creating service_proxy
- Extra `inner_release_external_ref()` call in cleanup path
- Factory return path not properly balanced
- Control block calling release without corresponding add_ref

#### Scenario 3: Service Proxy Lifecycle Issues

**Symptom**: Service proxy prematurely removed from `other_zones` map, causing routing failures

**Root Cause**: External ref count reaching 0 while objects still need the proxy for routing

**Key Understanding**:
- Service proxy should be removed when `lifetime_lock_count_ == 0 && is_responsible_for_cleaning_up_service_`
- For routing proxies (middle nodes in multi-zone topology), `cleanup_after_object()` is never called
- Destructor MUST handle cleanup for routing proxies

**Prevention**:
- Ensure `add_external_ref()` / `release_external_ref()` calls balance
- Don't call `remove_zone_proxy()` in `cleanup_after_object()` for routing proxies
- Destructor handles cleanup for proxies where `zone_id != caller_zone_id`

### Multi-Zone Reference Aggregation

**Important**: Remote stub deletion is based on **aggregate** shared reference count across **ALL zones**:

```
Zone A: rpc::shared_ptr<i_foo> obj_a → shared_count_a = 1
Zone B: rpc::shared_ptr<i_foo> obj_b → shared_count_b = 1
Stub (Zone C): shared_count_ = 2 (1 from A + 1 from B)

If Zone A releases: shared_count_ = 1 → stub still alive
If Zone B releases: shared_count_ = 0 → stub DELETED
```

This means stub lifetime depends on references from all zones combined, not just a single zone.

### Best Practices

1. **Enable Telemetry for Debugging**:
   ```bash
   cmake --preset Debug -DUSE_RPC_TELEMETRY=ON
   ./build/output/debug/rpc_test --telemetry-console
   ```

2. **Check Reference Balance**:
   - Every `add_external_ref()` must have corresponding `release_external_ref()`
   - Every `sp_add_ref()` must have corresponding `sp_release()`
   - Imbalanced references cause premature or delayed cleanup

3. **Understand Proxy Lifecycle**:
   - Routing proxies (middle nodes) need special destructor handling
   - `cleanup_after_object()` only called for proxies owning objects
   - Destructor must check `zone_id != caller_zone_id` for routing proxy cleanup

4. **Debugging Null Weak Pointers**:
   - Always check if destructor was called
   - Verify destructor removed weak_ptr from all collections
   - Check for routing proxy condition in cleanup code

### Reference Counting State Diagram

```
[Create shared_ptr]
    ↓
[Control Block: shared_count_=1]
    ↓
[Object Proxy: shared_count_ 0→1]
    ↓
[Service Proxy: add_external_ref(), lifetime_lock_count_++]
    ↓
[Remote Stub: sp_add_ref(), shared_count_++]
    ↓
[Object Alive, RPC Calls Succeed]
    ↓
[Destroy shared_ptr]
    ↓
[Control Block: shared_count_ 1→0]
    ↓
[Object Proxy: shared_count_ 1→0, triggers cleanup]
    ↓
[Service Proxy: sp_release(), inner_release_external_ref()]
    ↓
[Remote Stub: shared_count_--, delete if == 0]
    ↓
[Object Deleted, RPC Calls Return OBJECT_GONE]
```

This system ensures proper distributed object lifecycle management across all zones while supporting complex multi-zone topologies and circular reference scenarios.

---

## Error Code System

RPC++ implements a flexible error code system designed for seamless integration with legacy applications. The system allows error codes to be offset to prevent conflicts with existing application error codes.

### Design Principles

**Legacy Application Integration**: RPC++ error codes can be offset to coexist with existing error code systems without conflicts:

```cpp
// Configure RPC++ error codes to avoid conflicts
rpc::error::set_offset_val(10000);           // Base offset
rpc::error::set_offset_val_is_negative(false); // Positive offset direction
rpc::error::set_OK_val(0);                   // Success value (typically 0)
```

**RPC-Internal Usage**: RPC error codes are reserved for internal RPC++ operations and should only be used by applications for **inspection purposes**, not as application error codes.

### Core Error Codes

RPC++ defines specific error codes for different failure scenarios:

```cpp
// Critical errors
rpc::error::OK()                        // Success (configurable)
rpc::error::OUT_OF_MEMORY()            // Memory allocation failure
rpc::error::SECURITY_ERROR()           // Security/permission issues
rpc::error::TRANSPORT_ERROR()          // Transport layer failure

// Protocol errors
rpc::error::INVALID_METHOD_ID()        // Method not found
rpc::error::INVALID_INTERFACE_ID()     // Interface not implemented
rpc::error::INVALID_CAST()             // Type casting failure
rpc::error::INVALID_VERSION()          // Protocol version mismatch

// Zone management errors
rpc::error::ZONE_NOT_SUPPORTED()       // Zone type not supported
rpc::error::ZONE_NOT_INITIALISED()     // Zone not ready
rpc::error::ZONE_NOT_FOUND()           // Zone lookup failure
rpc::error::OBJECT_NOT_FOUND()         // Object reference invalid

// Serialization errors
rpc::error::PROXY_DESERIALISATION_ERROR()  // Client-side decode failure
rpc::error::STUB_DESERIALISATION_ERROR()   // Server-side decode failure
rpc::error::INCOMPATIBLE_SERIALISATION()   // Format not supported

// Connection errors
rpc::error::SERVICE_PROXY_LOST_CONNECTION() // Transport disconnected
rpc::error::CALL_CANCELLED()               // Operation cancelled
```

### Offset Configuration

The error code system supports flexible offsetting for legacy integration:

**Positive Offset Mode**:
```cpp
rpc::error::set_offset_val(50000);
rpc::error::set_offset_val_is_negative(false);
// Results in: OK=0, OUT_OF_MEMORY=50001, SECURITY_ERROR=50002, etc.
```

**Negative Offset Mode**:
```cpp
rpc::error::set_offset_val(-10000);
rpc::error::set_offset_val_is_negative(true);
// Results in: OK=0, OUT_OF_MEMORY=-10001, SECURITY_ERROR=-10002, etc.
```

### Usage Guidelines

**For Application Code**:
```cpp
// CORRECT: Inspect RPC error codes
auto error = CO_AWAIT service->connect_to_zone<...>(...);
if (error == rpc::error::OK()) {
    // Success
} else if (error == rpc::error::ZONE_NOT_FOUND()) {
    // Handle specific RPC error
    RPC_ERROR("Zone connection failed: {}", rpc::error::to_string(error));
} else {
    // Handle other RPC errors
    RPC_ERROR("RPC operation failed: {}", rpc::error::to_string(error));
}

// INCORRECT: Don't use RPC error codes as application error codes
int my_application_function() {
    if (some_condition) {
        return rpc::error::INVALID_DATA(); // DON'T DO THIS
    }
    return 0;
}
```

**For IDL Interface Design**:
```cpp
// Use application-specific error codes in IDL interfaces
typedef int error_code;  // Application-defined error codes

[status=production]
interface i_calculator {
    error_code add(int a, int b, [out] int& result);           // Returns app error codes
    error_code divide(int a, int b, [out] double& result);     // Returns app error codes
};
```

### Error String Conversion

```cpp
// Convert error codes to human-readable strings
int error = CO_AWAIT some_rpc_operation();
if (error != rpc::error::OK()) {
    const char* error_str = rpc::error::to_string(error);
    RPC_ERROR("Operation failed: {}", error_str);
}
```

### Range Information

```cpp
// Get error code range for validation
int min_error = rpc::error::MIN();  // Minimum RPC error code
int max_error = rpc::error::MAX();  // Maximum RPC error code

bool is_rpc_error = (error >= min_error && error <= max_error);
```

### Error Code Pass-Through and Translation

**Application Error Code Pass-Through**: All non-RPC error codes from application functions are passed through unchanged to the caller:

```cpp
// Server implementation
class calculator_impl : public calculator::v1::i_calculator {
public:
    error_code divide(int a, int b, [out] double& result) override {
        if (b == 0) {
            return 1001;  // Application-specific error code
        }
        result = static_cast<double>(a) / b;
        return 0;  // Application success code
    }
};

// Client receives exactly the same error codes
auto error = CO_AWAIT calc->divide(10, 0, result);
// error == 1001 (application error passed through unchanged)
```

**Multi-Application Error Translation**: When connecting applications with different error code ranges, the service proxy must translate errors to maintain compatibility

**Error Translation Scenarios**:

1. **Direct Connection** (same error ranges):
   ```cpp
   // No translation needed - direct pass-through
   Client App A ←→ RPC Layer ←→ Server App A
   App codes: 0, 1001-1999     App codes: 0, 1001-1999
   RPC codes: 50000+          RPC codes: 50000+
   ```

2. **Cross-Application Connection** (different error ranges):
   ```cpp
   // Translation required in service proxy
   Client App A ←→ Connection to an app with a different error code system ←→ Server App B
   App codes: 0, 1000-1999                                                 →  App codes: 0, 2000-2999
   RPC codes: 50000+                                                       →  RPC codes: 60000+
   ```

---

## Wire Protocol and Backward Compatibility

### Protocol Version Negotiation

RPC++ implements automatic protocol version negotiation that gracefully handles version mismatches:

```cpp
// Current implementation (v2.2.0)
uint64_t protocol_version = 3;  // Default to latest version

// Automatic fallback mechanism
int result = send(protocol_version, encoding, /* other params */);
if (result == rpc::error::INVALID_VERSION()) {
    protocol_version = 2;  // Fall back to legacy version
    result = send(protocol_version, encoding, /* other params */);
}
```

**Version Compatibility Matrix:**
- **Version 3** (Current): Enhanced zone routing, improved error handling
- **Version 2** (Deprecated): Stable baseline supported indefinitely
- **Version 1** (Removed): An early proof of concept implementation with many issues that needed to be fixed

### Interface Fingerprinting

The wire protocol includes interface fingerprinting to ensure type safety across version boundaries:

```cpp
// Interface signature includes all method signatures
// Ensures compile-time compatibility checking
uint64_t interface_id = generate_interface_fingerprint(i_calculator::methods);
```

**Fingerprint Protection Logic:**
1. **Development Status**: Fingerprint changes permitted - interfaces can evolve freely
2. **Production Status**: Fingerprint **LOCKED** - any interface changes rejected by CI/CD
3. **Deprecated Status**: Fingerprint preserved - no changes allowed but usage discouraged

### Serialization Formats

**Current Supported Formats:**
```cpp
enum class encoding {
    yas_binary = 0,    // High-performance binary (default)
    yas_json = 1,      // Human-readable for debugging
    // Future: protobuf, msgpack, custom formats
};
```

**Format Features:**
- **Content Detection**: Automatic format detection from wire data
- **Performance Optimization**: Binary preferred, JSON fallback
- **Debugging Support**: JSON format for wire-level inspection
- **Custom Formats**: Plugin architecture for domain-specific serialization

### Format Negotiation and Automatic Fallback

RPC++ implements **automatic format negotiation** where clients and servers can discover compatible serialization formats without manual configuration. When a stub encounters an unsupported encoding format, it automatically triggers fallback logic.

**Supported Encoding Formats:**
```cpp
enum class encoding : uint64_t {
    enc_default = 0,           // Maps to yas_binary
    yas_binary = 1,            // High-performance binary format
    yas_compressed_binary = 2, // Space-optimized binary format
    yas_json = 8,              // Universal fallback format
};
```

**Automatic Format Fallback Process:**

1. **Initial Request**: Proxy sends RPC call with current encoding (e.g., `yas_compressed_binary`)
2. **Format Validation**: Stub validates encoding support at method entry
3. **Fallback Trigger**: If unsupported, stub returns `INCOMPATIBLE_SERIALISATION()` error
4. **Automatic Retry**: Proxy detects error and retries with `yas_json` (universal format)
5. **Success or Failure**: Either succeeds with fallback format or fails if `yas_json` unsupported

**Example Scenario:**
```cpp
// Client using compressed binary format
auto client_proxy = service->get_proxy<i_calculator>();
client_proxy->set_encoding(rpc::encoding::yas_compressed_binary);

// Server only supports basic formats (older implementation)
// Automatic fallback sequence:
// 1. Client → Server: yas_compressed_binary → INCOMPATIBLE_SERIALISATION
// 2. Client → Server: yas_json (fallback) → SUCCESS
```

**Generated Code Behavior:**

**Stub-Side Validation:**
```cpp
// Generated in every stub method
if (enc != rpc::encoding::yas_binary &&
    enc != rpc::encoding::yas_compressed_binary &&
    enc != rpc::encoding::yas_json &&
    enc != rpc::encoding::enc_default)
{
    CO_RETURN rpc::error::INCOMPATIBLE_SERIALISATION();
}
```

**Proxy-Side Retry Logic:**
```cpp
// Generated in every proxy method
if(__rpc_ret == rpc::error::INCOMPATIBLE_SERIALISATION())
{
    if(__rpc_encoding != rpc::encoding::yas_json)
    {
        __rpc_sp->set_encoding(rpc::encoding::yas_json);
        __rpc_encoding = rpc::encoding::yas_json;
        continue; // Retry with universal format
    }
    else
    {
        CO_RETURN __rpc_ret; // No more fallback options
    }
}
```

**Key Benefits:**
- **Zero Configuration**: Applications work across different RPC++ versions automatically
- **Performance Optimization**: Uses best available format, falls back when needed
- **Deployment Safety**: Prevents serialization incompatibilities from breaking services
- **Future Compatibility**: New formats can be added without breaking existing deployments

**Combined Version and Format Negotiation:**
```cpp
// Automatic negotiation handles both version and format compatibility
auto error = CO_AWAIT proxy->calculate(10, 20, result);
// Internally handles:
// - Version 3 → Version 2 fallback (if needed)
// - yas_compressed_binary → yas_json fallback (if needed)
// - Returns result or meaningful error
```

This dual negotiation ensures maximum compatibility across heterogeneous deployments with mixed RPC++ versions and encoding capabilities.

### Deployment Protection

During code generation, RPC++ creates a `check_sums/` directory structure organized by interface status:

```
build/generated/check_sums/
├── production/
│   ├── calculator__v1__i_calculator          # SHA3 hash: a4f2b8c9d1e7...
│   └── user_service__v1__i_user_profile      # SHA3 hash: 3e8a9f2c5b1d...
├── development/
│   └── calculator__v1__i_calculator_dev      # SHA3 hash: (changes allowed)
└── deprecated/
    └── calculator__v1__i_old_calculator      # SHA3 hash: (preserved)
```

**CI/CD Integration:**
```bash
# Example CI/CD pipeline check
for status_dir in production deprecated; do
    for fingerprint_file in build/generated/check_sums/$status_dir/*; do
        if ! diff -q "$fingerprint_file" "committed_fingerprints/$status_dir/$(basename $fingerprint_file)"; then
            echo "ERROR: Production interface fingerprint changed!"
            exit 1
        fi
    done
done
```

### Template Type Fingerprinting Limitations

**Template Fingerprinting Constraint**: Template types do not have fully formed fingerprints unless they are used in concrete form within the IDL. Abstract template definitions cannot generate stable fingerprints for interface compatibility checking.

```cpp
// INCOMPLETE FINGERPRINTING: Abstract template definition
template<typename T>
struct generic_container {
    T data;
    std::vector<T> items;
};

// Template interfaces also lack concrete fingerprints
template<typename T>
interface i_generic_processor {
    error_code process(const generic_container<T>& input, [out] T& result);
};
```

**Required: Concrete Template Instantiations**

For proper fingerprinting, templates must be instantiated with specific concrete types in the IDL:

```cpp
// PROPER FINGERPRINTING: Concrete instantiations
[status=production]
struct string_container {
    std::string data;
    std::vector<std::string> items;
};

[status=production]
struct int_container {
    int data;
    std::vector<int> items;
};

// Concrete interfaces with proper fingerprints
[status=production]
interface i_string_processor {
    error_code process(const string_container& input, [out] std::string& result);
};

[status=production]
interface i_int_processor {
    error_code process(const int_container& input, [out] int& result);
};
```

**Template Usage Patterns**:

1. **Development Phase**: Use templates for rapid prototyping
   ```cpp
   // Template for development iteration
   template<typename T>
   struct dev_container { T value; };
   ```

2. **Production Phase**: Create concrete instantiations
   ```cpp
   // Concrete types for production deployment
   [status=production]
   struct user_data_container {
       std::string user_id;
       std::vector<std::string> permissions;
   };

   [status=production]
   struct config_data_container {
       int config_id;
       std::vector<int> values;
   };
   ```

**Fingerprinting Impact**:
- **Abstract Templates**: Cannot participate in fingerprint-based compatibility checking
- **Concrete Types**: Generate stable fingerprints for production interface validation
- **Version Safety**: Only concrete instantiations provide interface fingerprint protection
- **CI/CD Integration**: Fingerprint validation only works with concrete types

This limitation ensures that interface compatibility checking operates on well-defined, concrete types rather than generic templates that could have varying behavior based on instantiation.

---

## Transport Layer

### Transport Implementation Examples

**1. Local Child Service (In-Process)**
```cpp
// Direct function calls within the same process
rpc::shared_ptr<i_calculator> calc;
auto error = CO_AWAIT service->connect_to_zone<rpc::local_child_service_proxy<i_calculator, i_calculator>>(
    "calculator_zone",
    rpc::zone{2},
    rpc::shared_ptr<i_calculator>(),
    calc,
    setup_callback);
```

**2. SGX Enclave Communication**
```cpp
// Communication with SGX enclaves (from test code)
auto error = CO_AWAIT service->connect_to_zone<rpc::enclave_service_proxy>(
    "secure_enclave",
    rpc::zone{enclave_zone_id},
    host_interface,
    enclave_interface,
    enclave_path);
```

**3. SPSC Inter-Process Communication** (Coroutine builds only)
```cpp
// High-performance shared memory communication
auto error = CO_AWAIT service->connect_to_zone<rpc::spsc::service_proxy>(
    "peer_process",
    peer_zone_id.as_destination(),
    host_interface,
    remote_interface,
    timeout_ms,
    &send_queue,
    &receive_queue);

// Or attach to existing peer
auto error = CO_AWAIT service->attach_remote_zone<rpc::spsc::service_proxy, i_host, i_remote>(
    "remote_zone",
    input_descriptor,
    output_descriptor,
    setup_callback,
    connection_args...);
```

**4. TCP Network Communication** (Coroutine builds only)
```cpp
// Network communication over TCP
auto error = CO_AWAIT service->connect_to_zone<rpc::tcp::service_proxy>(
    "remote_host",
    remote_zone_id,
    local_interface,
    remote_interface,
    "192.168.1.100",
    8080,
    timeout_ms);
```

### The i_marshaller Interface

The `i_marshaller` interface is the core abstraction that enables RPC++ to work across different transport mechanisms:

```cpp
class i_marshaller {
public:
    virtual ~i_marshaller() = default;

    // Core RPC call marshalling
    virtual int send(uint64_t protocol_version, encoding enc, uint64_t tag,
                    caller_channel_zone caller_channel_zone_id,
                    caller_zone caller_zone_id,
                    destination_zone destination_zone_id,
                    object object_id,
                    interface_ordinal interface_id,
                    method method_id,
                    size_t data_in_sz, const char* data_in,
                    std::vector<char>& data_out) = 0;

    // Interface discovery mechanism
    virtual int try_cast(uint64_t protocol_version,
                        destination_zone destination_zone_id,
                        object object_id,
                        interface_ordinal interface_id) = 0;

    // Distributed object lifecycle management
    virtual uint64_t add_ref(uint64_t protocol_version,
                           destination_channel_zone destination_channel_zone_id,
                           destination_zone destination_zone_id,
                           object object_id,
                           caller_channel_zone caller_channel_zone_id,
                           caller_zone caller_zone_id,
                           add_ref_options options) = 0;

    virtual uint64_t release(uint64_t protocol_version,
                           destination_zone destination_zone_id,
                           object object_id,
                           caller_zone caller_zone_id) = 0;
};
```

---

## Child Services and Service Proxy Architecture

### Child Service Lifecycle and Parent Relationships

**Child services** (`rpc::child_service`) are zones that have a parent-child relationship with their creating service. Understanding their lifecycle management is critical for proper resource cleanup and preventing DLL unloading issues.

#### Parent Service Proxy Keepalive

**Critical Design Principle**: Child services maintain a `shared_ptr` to a **parent service proxy** (`local_service_proxy`) that keeps the parent service alive for the entire lifetime of the child zone.

```cpp
class child_service {
    // Keeps parent service alive
    std::shared_ptr<local_service_proxy> parent_service_proxy_;

    // Child's own service instance
    std::shared_ptr<rpc::service> service_;
};
```

**Why This Matters**:
- Child zone must ensure parent remains alive while child is active
- Parent service proxy provides communication channel back to parent
- Prevents premature parent service destruction
- **DLL Safety**: Critical for preventing parent DLL unload while child threads are active

#### Child Service Shutdown Sequence

A child service shuts itself down and releases the parent service proxy when **ALL** of the following conditions are met:

1. **No External Shared References**: All external shared references to objects in the child zone == 0
   - **Note**: Incoming optimistic references do NOT prevent shutdown
2. **No Internal References**: All internal references (both shared and optimistic) from the child to objects in other zones == 0
   - **Critical**: Outgoing optimistic references DO prevent shutdown
3. **No Routing Links**: The child is not acting as an intermediate routing node between other services

**Critical Distinction - Reference Directionality**:
- **Incoming optimistic references** (other zones holding optimistic pointers TO this service): Do NOT prevent shutdown. The service may die even if external zones hold optimistic references to its objects.
- **Outgoing optimistic references** (this service holding optimistic pointers TO remote objects): DO prevent shutdown. If the service has active optimistic pointers to remote objects, it may not shut down.

**Shutdown Trigger**:
```cpp
// Child service monitors reference counts in both directions
if (all_external_shared_refs_to_child == 0 &&  // Incoming shared only (optimistic don't matter)
    all_internal_refs_from_child == 0 &&       // Outgoing (shared + optimistic) - MUST be 0!
    !is_routing_intermediary)
{
    // Safe to shut down
    // Service may die even if other zones hold optimistic refs TO it
    // But CANNOT die if it holds optimistic refs TO other zones
    // 1. Complete all pending operations
    // 2. Release all resources
    // 3. Release parent_service_proxy_ (may trigger parent cleanup)
    // 4. Destroy child service
}
```

#### DLL Unloading Safety

**Critical Requirement**: If a child zone is implemented in a DLL or shared object, the parent service **MUST NOT** unload that DLL until:

1. Child service has completed shutdown
2. Child service garbage collection is complete
3. **All threads have returned from the DLL code**

**Reference Implementation**: The `enclave_service_proxy` already has cleanup logic for similar scenarios (waiting for threads to exit enclave, ensuring all ecalls/ocalls complete, graceful cleanup before destroying enclave). This pattern should be adapted for DLL service proxies.

**Implementation Pattern**:
```cpp
// Parent service holding child in DLL
class parent_service_manager {
    std::shared_ptr<rpc::child_service> dll_child_;
    void* dll_handle_;  // DLL/SO handle

    ~parent_service_manager() {
        // 1. Release child service reference
        dll_child_.reset();

        // 2. Wait for child garbage collection
        //    (child destructor completes, threads return)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // 3. Ensure no threads are in DLL code
        //    (implementation-specific synchronization)

        // 4. NOW safe to unload DLL
        if (dll_handle_) {
            dlclose(dll_handle_);  // or FreeLibrary() on Windows
        }
    }
};
```

**Thread Safety**:
- Parent must not call `dlclose()` / `FreeLibrary()` while child threads may be active
- Child service destructor must complete before DLL unload
- Race condition if DLL unloads before threads return from child code
- May require explicit synchronization (barriers, join points)

### Service Proxy Type Classification

RPC++ provides different service proxy types optimized for specific transport scenarios. Understanding their characteristics is essential for choosing the correct proxy type.

#### Local Service Proxies (In-Process, Bi-Modal)

**`local_service_proxy`**: Child → Parent communication
**`local_child_service_proxy`**: Parent → Child communication

**Key Characteristics**:
- **Simplest proxies**: Minimal wiring, direct function calls
- **Bi-modal execution**: Work in both synchronous (`BUILD_COROUTINE=OFF`) and coroutine (`BUILD_COROUTINE=ON`) modes
- **In-process only**: Both zones must be in the same process
- **No channel object**: Direct communication without intermediate transport layer
- **Parent-child relationship**: Managed lifecycle with automatic cleanup

**Usage Example**:
```cpp
// Parent creates child zone
rpc::shared_ptr<i_calculator> child_calc;
auto error = CO_AWAIT parent_service->connect_to_zone<
    rpc::local_child_service_proxy<i_parent, i_calculator>>(
    "calc_zone",
    rpc::zone{2},
    rpc::shared_ptr<i_parent>(),  // No parent interface passed to child
    child_calc,                    // Child returns calculator interface
    setup_callback);

// Child can access parent through local_service_proxy (implicit)
```

**When to Use**:
- In-process communication between parent and child zones
- Testing and debugging (simpler than network transports)
- Embedded systems or constrained environments
- When coroutines are not available

#### SPSC Service Proxies (Inter-Process, Coroutine-Only)

**`spsc::service_proxy`**: Single Producer Single Consumer queue-based transport

**Key Characteristics**:
- **Coroutine-only**: Requires `BUILD_COROUTINE=ON`
- **High-performance**: Lock-free queues for inter-process communication
- **Channel object required**: Uses `spsc::channel_manager` to manage bidirectional queues
- **Sender/receiver pairs**: Dedicated sender and receiver threads/tasks pumping queues
- **Unreliable transport handling**: Must handle connection loss and recovery

**Channel Manager Architecture**:
```cpp
class spsc::channel_manager {
    // Bidirectional SPSC queues
    queue_type* send_spsc_queue_;     // This zone → Remote zone
    queue_type* receive_spsc_queue_;  // Remote zone → This zone

    // Async pump tasks
    coro::task<void> send_pump_task();
    coro::task<void> receive_pump_task();

    // Service proxy registration
    std::atomic<int> service_proxy_ref_count_{0};
    void attach_service_proxy();
    coro::task<void> detach_service_proxy();
};
```

**Usage Example**:
```cpp
#ifdef BUILD_COROUTINE
// Create bidirectional SPSC queues
auto send_queue = create_spsc_queue();
auto receive_queue = create_spsc_queue();

// Attach to peer zone
auto error = CO_AWAIT service->attach_remote_zone<
    rpc::spsc::service_proxy, i_host, i_remote>(
    "peer_zone",
    input_descriptor,
    output_descriptor,
    setup_callback,
    timeout_ms,
    &send_queue,
    &receive_queue);
#endif
```

**When to Use**:
- High-throughput inter-process communication
- Shared memory transport between processes
- When coroutines are available
- Performance-critical message passing

**Channel Object Responsibilities**:
- Manage bidirectional queue pair
- Pump messages between queues and RPC layer
- Handle connection lifecycle (setup, teardown, error recovery)
- Coordinate shutdown between both directions

#### TCP Service Proxies (Network, Coroutine-Only)

**`tcp::service_proxy`**: Network-based TCP transport

**Key Characteristics**:
- **Coroutine-only**: Requires `BUILD_COROUTINE=ON`
- **Network communication**: TCP sockets for remote machine communication
- **Channel object required**: Manages socket I/O and message framing
- **Sender/receiver pairs**: Async send/receive tasks for non-blocking I/O
- **Unreliable transport**: Must handle network disconnects, timeouts, retries

**Channel Manager Pattern** (similar to SPSC):
```cpp
class tcp::channel_manager {
    // Socket and I/O state
    int socket_fd_;
    std::vector<char> send_buffer_;
    std::vector<char> receive_buffer_;

    // Async I/O tasks
    coro::task<void> send_pump_task();
    coro::task<void> receive_pump_task();

    // Connection management
    coro::task<void> handle_disconnect();
    coro::task<void> reconnect();
};
```

**Usage Example**:
```cpp
#ifdef BUILD_COROUTINE
// Connect to remote host over TCP
auto error = CO_AWAIT service->connect_to_zone<rpc::tcp::service_proxy>(
    "remote_host",
    remote_zone_id,
    local_interface,
    remote_interface,
    "192.168.1.100",  // IP address
    8080,              // Port
    timeout_ms);
#endif
```

**When to Use**:
- Communication across network boundaries
- Remote machine RPC calls
- Distributed systems
- Cloud deployments

**Channel Object Responsibilities**:
- Manage TCP socket lifecycle (connect, disconnect, reconnect)
- Frame and deframe messages over byte stream
- Handle send/receive queuing and flow control
- Coordinate graceful shutdown with pending I/O

#### SGX Enclave Service Proxies (Secure Enclaves, Hybrid Mode)

**`enclave_service_proxy`**: Host → Enclave communication
**`host_service_proxy`**: Enclave → Host communication

**Synchronous Mode** (`BUILD_COROUTINE=OFF`):
- **Direct SGX EDL calls**: Uses `ecall` (host→enclave) and `ocall` (enclave→host)
- **Inefficient**: Each call crosses enclave boundary synchronously
- **Thread limitations**: Limited number of threads can exist in enclave simultaneously
- **Simple**: No channel object needed, direct calls

**Coroutine Mode** (`BUILD_COROUTINE=ON`):
- **Hybrid approach**: Initial `ecall` to set up executor and SPSC channel
- **SPSC transport**: After setup, uses `spsc::channel_manager` for communication
- **Thread pool**: Enclave creates its own executor with thread pool
- **Shared channel**: `host_service_proxy` shares the `enclave_service_proxy`'s channel

**Setup Sequence (Coroutine Mode)**:
```cpp
#ifdef BUILD_COROUTINE
// 1. Host makes initial ecall into enclave
auto error = CO_AWAIT service->connect_to_zone<rpc::enclave_service_proxy>(
    "secure_enclave",
    rpc::zone{enclave_zone_id},
    host_interface,
    enclave_interface,
    enclave_path);

// 2. Inside enclave (setup callback):
//    - Create enclave service with its own executor/thread pool
//    - Set up SPSC channel manager (shared memory queues)
//    - Create enclave_service_proxy (host→enclave) using channel
//    - Create host_service_proxy (enclave→host) sharing the SAME channel
//    - Return enclave interface to host

// 3. After setup:
//    - All communication flows through SPSC channel
//    - No more ecalls/ocalls for RPC (only for setup/teardown)
//    - Enclave thread pool services requests asynchronously
#endif
```

**Why Coroutine Mode Uses SPSC**:
- **Efficiency**: Avoids repeated expensive ecall/ocall overhead
- **Concurrency**: Enclave thread pool can handle multiple requests
- **Thread limits**: SGX enclave thread limits don't block host threads
- **Performance**: SPSC queue is much faster than enclave boundary crossing

**Synchronous Mode Limitations**:
```cpp
// Each call crosses enclave boundary
auto result = enclave->calculate(a, b);  // ecall overhead
auto data = enclave->get_data();         // another ecall
auto status = enclave->process(data);    // another ecall
// Hundreds of microseconds per call
```

**Coroutine Mode Benefits**:
```cpp
#ifdef BUILD_COROUTINE
// Calls queued in SPSC, processed by enclave thread pool
auto result = CO_AWAIT enclave->calculate(a, b);  // SPSC queue
auto data = CO_AWAIT enclave->get_data();         // SPSC queue
auto status = CO_AWAIT enclave->process(data);    // SPSC queue
// Microseconds per call (no ecall overhead)
#endif
```

**When to Use**:
- Intel SGX secure enclaves
- Trusted execution environments
- Hardware-protected computation
- Security-critical operations

**Channel Sharing Architecture**:
```
Host Zone                Enclave Zone
    │                         │
    ├─ enclave_service_proxy  │
    │  (uses channel_manager) │
    │         │                │
    │         ▼                │
    │    SPSC Channel ◄────────┼─ Shared by both proxies
    │         ▲                │
    │         │                │
    │         └────────────────┼─ host_service_proxy
    │                          │  (shares channel_manager)
    │                         │
```

### Service Proxy Type Comparison Matrix

| Feature | Local | SPSC | TCP | Enclave (Sync) | Enclave (Coro) |
|---------|-------|------|-----|----------------|----------------|
| **Coroutine Requirement** | Optional | Required | Required | Optional | Required |
| **Channel Object** | ❌ No | ✅ Yes | ✅ Yes | ❌ No | ✅ Yes (SPSC) |
| **Transport** | Direct calls | Shared memory queues | TCP sockets | SGX ecall/ocall | SPSC + initial ecall |
| **Sender/Receiver Pairs** | ❌ No | ✅ Yes | ✅ Yes | ❌ No | ✅ Yes |
| **Reliability** | Always reliable | Reliable (in-process) | Unreliable (network) | Reliable | Reliable |
| **Performance** | Highest (direct) | Very high (lock-free) | Network latency | Low (ecall overhead) | High (SPSC) |
| **Thread Pool** | Not needed | Not needed | Not needed | Limited | Enclave executor |
| **Use Case** | Parent-child zones | Inter-process | Remote machines | Simple enclave | Efficient enclave |

### Channel Object Design Patterns

Services proxies that require **channel objects** (`spsc::service_proxy`, `tcp::service_proxy`, and `enclave_service_proxy` in coroutine mode) follow a common architectural pattern:

#### Channel Manager Responsibilities

1. **Bidirectional Communication**:
   - Manage send and receive paths independently
   - Coordinate message framing and deframing
   - Handle flow control and backpressure

2. **Async I/O Pump Tasks**:
   - Sender task: Pull messages from RPC layer, write to transport
   - Receiver task: Read from transport, push messages to RPC layer
   - Run as coroutines scheduled on executor

3. **Service Proxy Registration**:
   - Track how many service proxies are using the channel
   - Keep channel alive while any proxy references it
   - Initiate shutdown when last proxy detaches

4. **Graceful Shutdown**:
   - Wait for in-flight operations to complete
   - Flush pending messages
   - Close transport cleanly
   - Release resources

#### Channel Lifecycle Pattern

```cpp
class channel_manager {
    // Reference counting for service proxies
    std::atomic<int> service_proxy_ref_count_{0};
    std::atomic<int> operations_in_flight_{0};

    // Attach service proxy (called when proxy created)
    void attach_service_proxy() {
        ++service_proxy_ref_count_;
    }

    // Detach service proxy (called when proxy destroyed)
    CORO_TASK(void) detach_service_proxy() {
        if (--service_proxy_ref_count_ == 0) {
            // Last proxy detached - initiate shutdown
            CO_AWAIT shutdown();
        }
    }

    // Graceful shutdown sequence
    CORO_TASK(void) shutdown() {
        // 1. Stop accepting new operations
        shutting_down_ = true;

        // 2. Wait for in-flight operations
        while (operations_in_flight_ > 0) {
            CO_AWAIT scheduler_->schedule();  // Yield
        }

        // 3. Flush pending messages
        CO_AWAIT flush_pending();

        // 4. Close transport
        close_transport();

        // 5. Release resources
        cleanup();
    }
};
```

### Best Practices for Service Proxy Selection

#### Use `local_child_service_proxy` when:
- ✅ Communication is within the same process
- ✅ Parent-child relationship with managed lifecycle
- ✅ Simplicity is more important than async I/O
- ✅ Coroutines may not be available
- ✅ Debugging and testing

#### Use `spsc::service_proxy` when:
- ✅ High-performance inter-process communication needed
- ✅ Shared memory transport is available
- ✅ Coroutines are available (`BUILD_COROUTINE=ON`)
- ✅ Lock-free queues provide performance benefits

#### Use `tcp::service_proxy` when:
- ✅ Communication across network boundaries
- ✅ Remote machine RPC calls
- ✅ Distributed systems or cloud deployments
- ✅ Coroutines are available

#### Use `enclave_service_proxy` (sync mode) when:
- ✅ Simple SGX enclave integration
- ✅ Low call volume (ecall overhead acceptable)
- ✅ Coroutines not available
- ✅ Simplicity over performance

#### Use `enclave_service_proxy` (coroutine mode) when:
- ✅ High-performance enclave communication
- ✅ High call volume (ecall overhead unacceptable)
- ✅ Coroutines available
- ✅ Enclave can manage its own thread pool

---

## Build System and Configuration

### CMake Presets

Available build configurations:

```bash
# Standard configurations
cmake --preset Debug          # Debug build with full features
cmake --preset Release        # Optimized release build

# SGX configurations
cmake --preset Debug_SGX      # Debug build with SGX hardware support
cmake --preset Debug_SGX_Sim  # Debug build with SGX simulation

# Custom configuration
cmake --preset Debug -DBUILD_COROUTINE=ON -DUSE_RPC_LOGGING=ON
```

### Key Build Options

```cmake
# Core features
BUILD_COROUTINE=ON             # Enable coroutine support (TCP, SPSC transports)
BUILD_ENCLAVE=ON               # Enable SGX enclave support
BUILD_TEST=ON                  # Build test suite

# Debugging and development
USE_RPC_LOGGING=ON             # Enable comprehensive logging
USE_THREAD_LOCAL_LOGGING=ON    # Enable thread-local diagnostic logging
DEBUG_RPC_GEN=ON               # Debug code generation

# Performance and optimization
RPC_STANDALONE=ON              # Standalone build mode
CMAKE_EXPORT_COMPILE_COMMANDS=ON  # Export compile commands for tooling
```

### Bi-Modal Macro Definitions

The build system automatically defines macros that enable bi-modal execution:

**When `BUILD_COROUTINE=OFF` (Blocking Mode):**
```cpp
#define CO_AWAIT          // Expands to nothing
#define CO_RETURN return  // Standard return statement
#define CORO_TASK(T) T    // Standard return type
```

**When `BUILD_COROUTINE=ON` (Coroutine Mode):**
```cpp
#define CO_AWAIT co_await           // Standard C++20 co_await
#define CO_RETURN co_return         // Standard C++20 co_return
#define CORO_TASK(T) coro::task<T>  // Coroutine task type
```

This allows the same source code to compile and run correctly in both modes without `#ifdef` conditionals in application code.

### Code Generation

IDL files are automatically processed during build:

```bash
# Generate code from IDL
cmake --build build --target calculator_idl

# Generated files structure
build/generated/
├── include/calculator/calculator.h           # Interface definitions
├── src/calculator/calculator_proxy.cpp       # Client-side proxy
├── src/calculator/calculator_stub.cpp        # Server-side stub
├── src/calculator/yas/calculator.cpp         # Serialization code
└── json_schema/calculator.json               # JSON schema metadata
```

---

## Logging and Telemetry

### Modern Logging System

RPC++ uses a structured logging system based on fmt::format:

```cpp
// Available logging levels
RPC_DEBUG("Detailed debugging information: {}", value);
RPC_TRACE("Function entry/exit tracing: {}", function_name);
RPC_INFO("General information: {}", status);
RPC_WARNING("Warning condition: {}", warning_msg);
RPC_ERROR("Error occurred: {}", error_code);
RPC_CRITICAL("Critical failure: {}", failure_reason);
```

**Logging Features:**
- **Zero-overhead when disabled** (`USE_RPC_LOGGING=OFF`)
- **Compile-time format validation** with fmt::format
- **Thread-safe** with minimal locking
- **Multi-environment support**: Host, SGX enclaves, embedded systems

### Thread-Local Diagnostic Logging

For debugging complex multithreaded scenarios:

```cpp
// Enable thread-local logging
cmake --preset Debug -DUSE_THREAD_LOCAL_LOGGING=ON

// Automatic crash dumps
// Each thread maintains a circular buffer of recent log entries
// On crash, all thread buffers are dumped to /tmp/rpc_crash_dumps/
```

**Use Cases:**
- Race condition debugging
- Service lifecycle analysis
- Cross-thread operation correlation
- Performance profiling

### Telemetry System

Comprehensive telemetry for production monitoring:

```cpp
// Telemetry collection
auto telemetry = service.get_telemetry();
telemetry->record_call_duration(interface_id, method_id, duration_ms);
telemetry->record_error(error_code, context);
telemetry->record_zone_creation(zone_id, parent_zone_id);

// Export telemetry data
auto json_report = telemetry->export_json();
auto metrics = telemetry->get_performance_metrics();
```

---

## Advanced Features

### Coroutine-Only Transports

Some transport types are only available when `BUILD_COROUTINE=ON`:

```cpp
#ifdef BUILD_COROUTINE
// TCP network transport (coroutine-only)
auto error = CO_AWAIT service->connect_to_zone<rpc::tcp::service_proxy>(
    "remote_host", remote_zone_id, local_interface, remote_interface,
    "192.168.1.100", 8080, timeout_ms);

// SPSC inter-process transport (coroutine-only)
auto error = CO_AWAIT service->connect_to_zone<rpc::spsc::service_proxy>(
    "peer_process", peer_zone_id, host_interface, remote_interface,
    timeout_ms, &send_queue, &receive_queue);
#endif

// These transports work in both modes:
// - local_child_service_proxy (in-process)
// - enclave_service_proxy (SGX enclaves)
// - host_service_proxy (enclave-to-host)
```

**Why Some Transports Require Coroutines:**
- **TCP**: Async I/O operations for network communication
- **SPSC**: Async queue operations for inter-process communication
- **Performance**: Non-blocking I/O prevents thread starvation

### SGX Enclave Integration

Secure computation with Intel SGX:

```cpp
// Enclave configuration
auto enclave_proxy = service.connect_to_enclave<i_secure_processor>(
    "secure_enclave.signed.so",
    SGX_DEBUG_FLAG,
    launch_token
);

// Secure RPC calls
secure_data input = prepare_sensitive_data();
secure_result output;
auto error = enclave_proxy->process_secure_data(input, output);
```

**Security Features:**
- Automatic attestation and secure channel establishment
- Memory protection for sensitive data
- Secure serialization with validation

### JSON Schema Generation

Automatic API documentation and validation:

```idl
[description="Calculator service for mathematical operations"]
interface i_calculator {
    [description="Adds two integers and returns the result"]
    error_code add(int a, int b, [out] int& result);
}
```

Generated JSON schema:
```json
{
  "interfaces": {
    "i_calculator": {
      "description": "Calculator service for mathematical operations",
      "methods": {
        "add": {
          "description": "Adds two integers and returns the result",
          "input_schema": { "properties": { "a": {"type": "integer"}, "b": {"type": "integer"} } },
          "output_schema": { "properties": { "result": {"type": "integer"} } }
        }
      }
    }
  }
}
```

---

## Best Practices

### Interface Design

**Single Responsibility Principle** - **HIGHLY RECOMMENDED**

```cpp
// ❌ BAD: Monolithic interface with mixed responsibilities
interface i_user_management {
    // User data, authentication, billing, analytics mixed together
};

// ✅ GOOD: Focused interfaces with single responsibility
namespace user_management {
    [inline] namespace v1 {
        interface i_user_profile {
            int get_user_profile(int user_id, [out] user_profile& profile);
            int update_user_email(int user_id, string new_email);
        };
    }
}

namespace authentication {
    [inline] namespace v1 {
        interface i_auth_service {
            int authenticate_user(string username, string password, [out] auth_token& token);
            int validate_session(auth_token token, [out] bool& is_valid);
        };
    }
}
```

### Versioned Namespaces - **HIGHLY RECOMMENDED**

**ALL types and interfaces should be placed in versioned namespaces:**

```cpp
namespace calculator {
    [inline] namespace v1 {
        [status=production]  // Locked - cannot change
        interface i_calculator {
            int add(int a, int b, [out] int& result);
        };
    }

    [inline] namespace v2 {
        [status=development]  // Can evolve freely
        interface i_calculator {
            int add(int a, int b, [out] int& result);
            int add_with_precision(double a, double b, int precision, [out] double& result);
        };
    }
}
```

### Graceful Deprecation

```cpp
[deprecated]  // Triggers compiler warnings, preserves fingerprint
int legacy_method(int param);  // Use new_method() instead
```

### Error Handling

```cpp
// Always check return codes
auto error = proxy->method_call(params);
if (error != rpc::error::OK()) {
    RPC_ERROR("RPC call failed: {}", error);
    return error;
}

// Use structured error information
if (error == rpc::error::OBJECT_NOT_FOUND()) {
    // Handle missing object
} else if (error == rpc::error::TRANSPORT_ERROR()) {
    // Handle network issues
}
```

### Performance Optimization

```cpp
// Prefer by-value for small types
int process_id(int id);

// Use by-reference for large types
int process_data([in] const large_data_structure& data);

// Minimize RPC calls with batch operations
int process_batch([in] const std::vector<item>& items,
                  [out] std::vector<result>& results);
```

### Development Workflow

1. **Prototype** with `[status=development]` - fingerprint tracking but changes allowed
2. **Stabilize** interface design through testing and review
3. **Promote** to `[status=production]` - fingerprint becomes immutable
4. **Deploy** with CI/CD fingerprint validation
5. **Evolve** by creating new versioned interfaces, not modifying existing ones

This ensures that **production deployments are never broken** by accidental interface changes while still allowing rapid development iteration.

## Format Negotiation and Automatic Fallback

RPC++ provides automatic format negotiation and fallback mechanisms to ensure compatibility across different encoding formats and protocol versions. This system enables seamless communication between components that may support different serialization formats or RPC protocol versions.

### Supported Encoding Formats

RPC++ supports multiple encoding formats with an extensible architecture that allows for additional formats to be added:

**Built-in Formats**:
- **`rpc::encoding::enc_default`** - System default encoding
- **`rpc::encoding::yas_binary`** - Binary YAS serialization (compact, fast)
- **`rpc::encoding::yas_compressed_binary`** - Compressed binary YAS (smallest size)
- **`rpc::encoding::yas_json`** - JSON-based YAS serialization (universal compatibility)

**Extensible Architecture**: The encoding system is designed to support additional serialization formats such as:
- **Protocol Buffers** - Can be added as `rpc::encoding::protobuf`
- **MessagePack** - Can be added as `rpc::encoding::msgpack`
- **Apache Avro** - Can be added as `rpc::encoding::avro`
- **Custom Formats** - Application-specific serialization schemes

The automatic fallback mechanism works with any encoding format, always falling back to `yas_json` as the universal compatibility layer when other formats are not supported by both endpoints.

### Automatic Format Fallback Process

When an RPC call is made, the system follows this automatic fallback sequence:

1. **Initial Attempt**: Use the proxy's current encoding setting
2. **Stub Validation**: The receiving stub validates if it supports the requested encoding
3. **Incompatibility Detection**: If unsupported, stub returns `INCOMPATIBLE_SERIALISATION` error
4. **Automatic Retry**: Proxy detects the error and retries with `yas_json` (universal format)
5. **Success**: Call succeeds with the fallback encoding, proxy updates its encoding setting

### Generated Code Behavior

The RPC++ code generator creates automatic fallback logic in both proxy and stub code:

**Stub-side Validation** (validates incoming encoding):
```cpp
// Example with built-in formats
if (enc != rpc::encoding::yas_binary &&
    enc != rpc::encoding::yas_compressed_binary &&
    enc != rpc::encoding::yas_json &&
    enc != rpc::encoding::enc_default)
{
    CO_RETURN rpc::error::INCOMPATIBLE_SERIALISATION();
}

// Example with extended formats (including Protocol Buffers)
if (enc != rpc::encoding::yas_binary &&
    enc != rpc::encoding::yas_compressed_binary &&
    enc != rpc::encoding::yas_json &&
    enc != rpc::encoding::protobuf &&
    enc != rpc::encoding::enc_default)
{
    CO_RETURN rpc::error::INCOMPATIBLE_SERIALISATION();
}
```

**Proxy-side Retry Logic** (automatic fallback):
```cpp
if(__rpc_ret == rpc::error::INCOMPATIBLE_SERIALISATION())
{
    if(__rpc_encoding != rpc::encoding::yas_json)
    {
        __rpc_sp->set_encoding(rpc::encoding::yas_json);
        __rpc_encoding = rpc::encoding::yas_json;
        continue; // Retry with universal format
    }
}
```

### Version Negotiation and Fallback

RPC++ also provides automatic version negotiation for protocol compatibility:

**Version Fallback Behavior**:
- When an unsupported protocol version is encountered, the system automatically falls back to supported versions
- **Discovery**: Unsupported versions (e.g., version 4, 1, 0) automatically fall back to the highest supported version (typically VERSION_3)
- **Progressive Negotiation**: The system iterates through versions starting from the requested version down to the minimum supported version
- **State Management**: Service proxy version state is automatically updated after successful negotiation

**Version Negotiation Loop** (in generated proxy code):
```cpp
while (protocol_version >= __rpc_min_version)
{
    __rpc_ret = CO_AWAIT __rpc_op->send(protocol_version, /* ... */);

    if (__rpc_ret == rpc::error::INVALID_VERSION())
    {
        if (protocol_version == __rpc_min_version)
        {
            CO_RETURN __rpc_ret; // No more versions to try
        }
        --protocol_version; // Try next lower version
        __rpc_sp->update_remote_rpc_version(protocol_version);
        continue;
    }
    break; // Success
}
```

### Universal Fallback Guarantees

- **`yas_json` encoding** works with all RPC++ implementations (universal compatibility)
- **Combined fallback** handles both version and encoding issues simultaneously
- **Graceful degradation** ensures calls succeed even with mismatched capabilities
- **Transparent operation** - applications don't need to handle fallback logic manually

### Testing Format and Version Fallback

For testing fallback mechanisms, RPC++ provides direct proxy testing approaches:

**Format Fallback Testing**:
```cpp
// Force invalid encoding to test fallback
auto invalid_encoding = static_cast<rpc::encoding>(999);
auto __rpc_op = rpc::casting_interface::get_object_proxy(*foo_proxy);
auto error = CO_AWAIT do_something_in_val(test_value, __rpc_op,
                                         rpc::VERSION_3, invalid_encoding);
// Should succeed due to automatic fallback to yas_json
```

**Version Fallback Testing**:
```cpp
// Force unsupported version to test fallback
auto error = CO_AWAIT do_something_in_val(test_value, __rpc_op,
                                         4, rpc::encoding::yas_json);
// Should succeed due to automatic version negotiation
```

### Benefits

1. **Seamless Interoperability**: Different RPC++ versions can communicate automatically
2. **Zero Configuration**: No manual encoding negotiation required
3. **Performance Optimization**: Uses best available encoding, falls back when needed
4. **Robust Deployment**: New deployments work with existing systems
5. **Development Flexibility**: Test different encoding strategies without breaking compatibility

### Adding New Encoding Formats

The RPC++ encoding system is designed for extensibility. To add new formats like Protocol Buffers:

1. **Extend the Encoding Enum**: Add new encoding types to `rpc::encoding`
2. **Implement Serializers**: Create serialization/deserialization logic for the new format
3. **Update Generated Code**: The code generator automatically includes new formats in validation logic
4. **Maintain Fallback**: `yas_json` remains the universal fallback for maximum compatibility

**Example Protocol Buffers Integration**:
```cpp
// New encoding type
namespace rpc {
    enum class encoding {
        enc_default,
        yas_binary,
        yas_compressed_binary,
        yas_json,
        protobuf,          // New format
        msgpack,           // Another new format
        custom_format      // Application-specific
    };
}

// Generated stub code automatically supports new formats
if (enc != rpc::encoding::yas_binary &&
    enc != rpc::encoding::yas_compressed_binary &&
    enc != rpc::encoding::yas_json &&
    enc != rpc::encoding::protobuf &&
    enc != rpc::encoding::msgpack &&
    enc != rpc::encoding::custom_format &&
    enc != rpc::encoding::enc_default)
{
    CO_RETURN rpc::error::INCOMPATIBLE_SERIALISATION();
}

// Fallback logic works for any format
if(__rpc_ret == rpc::error::INCOMPATIBLE_SERIALISATION())
{
    if(__rpc_encoding != rpc::encoding::yas_json)
    {
        __rpc_sp->set_encoding(rpc::encoding::yas_json);
        __rpc_encoding = rpc::encoding::yas_json;
        continue; // Retry with universal format
    }
}
```

**Benefits of Format Extensibility**:
- **Performance Optimization**: Use Protocol Buffers for high-performance scenarios
- **Ecosystem Integration**: Support industry-standard formats without breaking compatibility
- **Gradual Migration**: Deploy new formats while maintaining compatibility with existing systems
- **Custom Requirements**: Add application-specific serialization formats
- **Zero Downtime Upgrades**: New formats work alongside existing deployments

The automatic fallback system ensures that RPC++ applications maintain compatibility while allowing individual components to optimize for their specific requirements, whether using built-in formats or custom extensions like Protocol Buffers.

### Service Proxy Data Handling Architecture

**Opaque Data Principle**: Service proxies handle relayed data as **opaque blobs** when forwarding between zones. This architectural principle ensures that service proxies don't need to understand the complete structure of data they're relaying, allowing for layered protocols and transport abstractions.

**Opaque Handling Benefits**:
- **Transport Independence**: Proxies don't need to understand all protocol layers
- **Layered Security**: Encrypted payloads remain encrypted during relay
- **Protocol Evolution**: New transport layers can be added without breaking existing proxies
- **Performance**: Avoids unnecessary serialization/deserialization in relay nodes
- **Modularity**: Each proxy component has clear separation of concerns

**Implementation Guidelines**:
- Service proxies should treat relayed data as binary blobs
- Each layer adds only its required headers/metadata
- Avoid unpacking data not relevant to the current transport layer
- Maintain protocol abstraction boundaries for future extensibility

---

---

## Appendix: Detailed Technical Information

### IDL System Details

The IDL format supports structures, interfaces, namespaces and basic STL type objects including string, map, vector, optional, variant.

### Implementation Status

This implementation now supports both **synchronous calls** and **asynchronous coroutines** (C++20), with the mode controlled at build time by the `BUILD_COROUTINE` macro.
This solution currently only supports error codes, exception based error handling is to be implemented at some date.

Currently it has adaptors for:
 * in memory calls
 * calls between different memory arenas
 * calls to SGX enclaves

This implementation currently uses YAS for its serialization needs, however it could be extended to other serializers in the future.

### Detailed Build Configuration

Out of the box without any configuration management this library can be compiled on windows and linux machines.  It requires a relatively up to date c++ compiler and CMake.

There are different CMake options specified in the root CMakeLists.txt:
```
  option(BUILD_ENCLAVE "build enclave code" ON)
  option(BUILD_HOST "build host code" ON)
  option(BUILD_EXE "build exe code" ON)
  option(BUILD_TEST "build test code" ON)
  option(DEBUG_HOST_LEAK "enable leak sanitizer (only use when ptrace is accessible)" OFF)
  option(DEBUG_HOST_ADDRESS "enable address sanitizer" OFF)
  option(DEBUG_HOST_THREAD "enable thread sanitizer (cannot be used with leak sanitizer)" OFF)
  option(DEBUG_HOST_UNDEFINED "enable undefined behaviour sanitizer" OFF)
  option(DEBUG_HOST_ALL "enable all sanitizers" OFF)
  option(DEBUG_ENCLAVE_MEMLEAK "detect memory leaks in enclaves" OFF)
  option(ENABLE_CLANG_TIDY "Enable clang-tidy in build (needs to build with clang)" OFF)
  option(ENABLE_CLANG_TIDY_FIX "Turn on auto fix in clang tidy" OFF)
  option(ENABLE_COVERAGE "Turn on code coverage" OFF)
  option(CMAKE_VERBOSE_MAKEFILE "verbose build step" OFF)
  option(CMAKE_RULE_MESSAGES "verbose cmake" OFF)
  option(FORCE_DEBUG_INFORMATION "force inclusion of debug information" ON)
  option(USE_RPC_LOGGING "turn on rpc logging" OFF)
  option(RPC_HANG_ON_FAILED_ASSERT "hang on failed assert" OFF)
  option(USE_RPC_TELEMETRY "turn on rpc telemetry" OFF)
  option(USE_RPC_TELEMETRY_RAII_LOGGING "turn on the logging of the addref release and try cast activity of the services, proxies and stubs" OFF)
 ```

**Note**: For current build configurations, see `CMakePresets.json` which provides preconfigured builds including `Debug`, `Debug_multithreaded`, `Debug_SGX`, `Release`, etc.

From the command line (traditional approach):
```bash
mkdir build
cd build
cmake ..
```

Or using presets (recommended):
```bash
cmake --preset Debug
cmake --build build --target rpc
```

If you are using VSCode add the CMake Tools extension, you will then see targets on the bar at the bottom to compile code for different targets.

Alternatively you can set them explicitly from the .vscode/settings.json file

```
{
    "cmake.configureArgs": [
        "-DRPC_USE_LOGGING=ON",
        "-BUILD_ENCLAVE=ON"
    ]
}
```

Also for viewing idl files more easily install the "Microsoft MIDL and Mozilla WebIDL syntax highlighting" extension to get some highlighting support.

Currently tested on Compilers:
 * Clang 10+
 * GCC 9.4
 * Visual Studio 2017 or later

Though strongly suggest upgrading to the latest version of these compilers

### Coroutines Support Details

RPC++ provides comprehensive support for C++20 coroutines, allowing you to write asynchronous RPC code that looks and feels like synchronous code while providing the performance benefits of non-blocking I/O.

#### Build Configuration

Coroutine support is controlled by the `BUILD_COROUTINE` CMake option and can be enabled at build time:

```bash
# Enable coroutines using preset
cmake --preset Debug_with_coroutines
cmake --build build

# Or manually enable
cmake -DBUILD_COROUTINE=ON ..
cmake --build build
```

When `BUILD_COROUTINE=ON`:
- All RPC functions return `coro::task<T>` instead of `T`
- Functions use `co_await` for asynchronous operations
- Functions use `co_return` instead of `return`
- Services require a `coro::io_scheduler` for execution

When `BUILD_COROUTINE=OFF`:
- Functions work synchronously as traditional blocking calls
- No coroutine dependencies required
- Simpler deployment for scenarios where async isn't needed

#### Programming Model

**Coroutine-Enabled Functions**

With coroutines enabled, RPC functions become asynchronous:

```cpp
// IDL Definition (same for both modes)
interface ICalculator {
    error_code add(int a, int b, [out] int result);
    error_code multiply_async(double x, double y, [out] double result);
}

// Implementation with coroutines
class calculator_impl : public ICalculator {
public:
    // Returns coro::task<int> when BUILD_COROUTINE=ON
    // Returns int when BUILD_COROUTINE=OFF
    CORO_TASK(int) add(int a, int b, int& result) override {
        // Simulate async work
        CO_AWAIT some_async_operation();
        result = a + b;
        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int) multiply_async(double x, double y, double& result) override {
        // Can await other RPC calls
        int temp_result;
        auto ret = CO_AWAIT other_service->calculate_base(x, y, temp_result);
        if (ret != rpc::error::OK()) {
            CO_RETURN ret;
        }
        result = temp_result * 2.0;
        CO_RETURN rpc::error::OK();
    }
};
```

**Calling Coroutine Functions**

```cpp
// Client code calling RPC functions
auto calc = get_calculator_proxy();

// Coroutine mode - functions must be awaited
CORO_TASK(void) do_calculation() {
    int result;
    auto ret = CO_AWAIT calc->add(5, 3, result);
    if (ret == rpc::error::OK()) {
        std::cout << "Result: " << result << std::endl;
    }
    CO_RETURN;
}

// Non-coroutine mode - direct function calls
void do_calculation_sync() {
    int result;
    auto ret = calc->add(5, 3, result);  // Direct call, no await
    if (ret == rpc::error::OK()) {
        std::cout << "Result: " << result << std::endl;
    }
}
```

#### Service Setup

**Coroutine-Enabled Services**

When building with coroutines, services require an I/O scheduler:

```cpp
#ifdef BUILD_COROUTINE
#include <coro/io_scheduler.hpp>

// Create scheduler for async operations
auto scheduler = std::make_shared<coro::io_scheduler>();

// Create service with scheduler
auto service = std::make_shared<rpc::service>("MyService", zone_id, scheduler);

// Schedule coroutine work
service->schedule([]() -> coro::task<void> {
    // Async work here
    co_await some_async_task();
    co_return;
});

#else
// Simple synchronous service
auto service = std::make_shared<rpc::service>("MyService", zone_id);
#endif
```

#### Coroutine Macros

RPC++ provides macros that adapt based on the build configuration:

| Macro | Coroutine Mode | Non-Coroutine Mode |
|-------|----------------|-------------------|
| `CORO_TASK(T)` | `coro::task<T>` | `T` |
| `CO_AWAIT expr` | `co_await expr` | `expr` |
| `CO_RETURN val` | `co_return val` | `return val` |
| `SYNC_WAIT(task)` | `coro::sync_wait(task)` | `task` |

This allows the same code to compile in both modes:

```cpp
CORO_TASK(int) my_function() {
    auto result = CO_AWAIT async_operation();
    CO_RETURN process_result(result);
}
```

#### Error Handling

Error handling works consistently in both modes:

```cpp
CORO_TASK(int) handle_errors() {
    int result;
    auto ret = CO_AWAIT risky_operation(result);

    // Check error codes as usual
    if (ret != rpc::error::OK()) {
        RPC_ERROR("Operation failed with code: {}", ret);
        CO_RETURN ret;  // Propagate error
    }

    CO_RETURN rpc::error::OK();
}
```

#### Performance Considerations

**Benefits of Coroutines**
- **Non-blocking I/O**: Threads aren't blocked waiting for network/disk operations
- **High Concurrency**: Handle thousands of concurrent operations efficiently
- **Resource Efficiency**: Lower memory overhead compared to thread-per-request models
- **Scalability**: Better performance under high load

**When to Use Each Mode**
- **Use Coroutines (`BUILD_COROUTINE=ON`) when**:
  - Building high-throughput services
  - Network/remote RPC calls are common
  - You need to handle many concurrent requests
  - I/O latency is a bottleneck

- **Use Synchronous (`BUILD_COROUTINE=OFF`) when**:
  - Simple in-process or local communication
  - Embedded/resource-constrained environments
  - Simpler debugging and deployment requirements
  - C++20 coroutines not available

#### Migration Strategy

The dual-mode design allows gradual migration:

1. **Start Synchronous**: Begin development with `BUILD_COROUTINE=OFF`
2. **Test Both Modes**: Ensure code compiles and works in both configurations
3. **Performance Test**: Benchmark both modes under your workload
4. **Deploy Appropriately**: Choose mode based on deployment requirements

#### Advanced Usage

**Custom Schedulers**

```cpp
// Custom scheduler with thread pool
class custom_scheduler : public coro::io_scheduler {
    // Custom implementation
};

auto scheduler = std::make_shared<custom_scheduler>();
auto service = std::make_shared<rpc::service>("MyService", zone_id, scheduler);
```

**Mixed Async/Sync Operations**

```cpp
CORO_TASK(int) mixed_operations() {
    // Fast local operation - no await needed
    auto local_result = fast_local_calc();

    // Slow remote operation - await it
    int remote_result;
    auto ret = CO_AWAIT slow_remote_call(remote_result);

    CO_RETURN combine_results(local_result, remote_result);
}
```

This coroutine support provides the foundation for building high-performance, scalable RPC applications while maintaining the flexibility to deploy in simpler synchronous environments when appropriate.

---

*This guide covers RPC++ version 2.2.0. For the latest updates and detailed API documentation, visit the project repository.*