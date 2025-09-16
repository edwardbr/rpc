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
9. [Build System and Configuration](#build-system-and-configuration)
10. [Logging and Telemetry](#logging-and-telemetry)
11. [Advanced Features](#advanced-features)
12. [Best Practices](#best-practices)

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
git clone <rpc-repository>
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
auto root_service = rpc::make_shared<rpc::service>("root", rpc::zone{1});

// Create child zone with calculator implementation
rpc::shared_ptr<calculator::v1::i_calculator> calc_proxy;
auto error = CO_AWAIT root_service->connect_to_zone<rpc::local_child_service_proxy<calculator::v1::i_calculator, calculator::v1::i_calculator>>(
    "calculator_zone",
    rpc::zone{2},
    rpc::shared_ptr<calculator::v1::i_calculator>(),
    calc_proxy,
    [](const rpc::shared_ptr<calculator::v1::i_calculator>& input,
       rpc::shared_ptr<calculator::v1::i_calculator>& output,
       const rpc::shared_ptr<rpc::service>& child_service) -> int
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
    auto root_service = rpc::make_shared<rpc::service>("root", rpc::zone{1});

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

### Parameter Attributes

- **`[in]`**: Input parameter (default for value types)
- **`[out]`**: Output parameter (must be reference)

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
auto root_service = rpc::make_shared<rpc::service>("root_service", rpc::zone{1});

// With coroutine support
#ifdef BUILD_COROUTINE
auto root_service = rpc::make_shared<rpc::service>("root_service", rpc::zone{1}, io_scheduler);
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
       const rpc::shared_ptr<rpc::service>& child_service) -> int
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
       const rpc::shared_ptr<rpc::service>& service) -> CORO_TASK(int)
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
rpc::shared_ptr<rpc::child_service> new_child;
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

**Multi-Application Error Translation**: When connecting applications with different error code ranges, the service proxy must translate errors to maintain compatibility:

```cpp
// Legacy App A uses error range: 0=success, 1000-1999=errors
// Modern App B uses error range: 0=success, 2000-2999=errors
// RPC uses offset range: 50000+

class error_translating_service_proxy {
    int translate_app_a_to_app_b(int app_a_error) {
        if (app_a_error == 0) return 0;  // Success
        if (app_a_error >= 1000 && app_a_error <= 1999) {
            return 2000 + (app_a_error - 1000);  // Translate range
        }
        return app_a_error;  // Pass through unknown codes
    }

    int translate_rpc_errors(int rpc_error) {
        // RPC errors need rebasing for App B's range
        if (rpc_error >= rpc::error::MIN() && rpc_error <= rpc::error::MAX()) {
            // Rebase RPC errors to App B's reserved RPC range (e.g., 3000+)
            return 3000 + (rpc_error - rpc::error::MIN());
        }
        return rpc_error;
    }
};
```

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
   Client App A ←→ Translation Proxy ←→ Server App B
   App codes: 0, 1000-1999    →    App codes: 0, 2000-2999
   RPC codes: 50000+         →    RPC codes: 60000+
   ```

**Translation Implementation Pattern**:
```cpp
// In custom service proxy implementation
template<typename TargetProxy>
class error_translating_proxy : public TargetProxy {
public:
    error_code translated_method(int param, [out] int& result) override {
        auto error = TargetProxy::translated_method(param, result);

        // Translate application errors between ranges
        if (is_source_app_error(error)) {
            return translate_to_target_app_range(error);
        }

        // Rebase RPC errors for target application
        if (is_rpc_error(error)) {
            return rebase_rpc_error_for_target(error);
        }

        return error;  // Pass through unchanged
    }
};
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

    // Distributed object lifecycle management
    virtual int try_cast(uint64_t protocol_version,
                        destination_zone destination_zone_id,
                        object object_id,
                        interface_ordinal interface_id) = 0;

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

---

*This guide covers RPC++ version 2.2.0. For the latest updates and detailed API documentation, visit the project repository.*