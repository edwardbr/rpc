# RPC++ User Guide

**A Modern C++ Remote Procedure Call Library**

*Version 2.2.0*

---

## Table of Contents

1. [Introduction](#introduction)
2. [What are Remote Procedure Calls?](#what-are-remote-procedure-calls)
3. [Interface Definition Language (IDL)](#interface-definition-language-idl)
4. [Code Generators and Language Support](#code-generators-and-language-support)
5. [Telemetry System](#telemetry-system)
6. [Logging Framework](#logging-framework)
7. [Thread-Local Logging and Enhanced Crash Diagnostics](#thread-local-logging-and-enhanced-crash-diagnostics)
8. [Multithreading and Coroutines](#multithreading-and-coroutines)
9. [Service Proxies and Transport Abstraction](#service-proxies-and-transport-abstraction)
10. [The i_marshaller Interface](#the-i_marshaller-interface)
11. [Getting Started](#getting-started)
12. [Architecture Overview](#architecture-overview)

---

## Introduction

RPC++ is a modern C++ Remote Procedure Call library designed for type-safe communication across different execution contexts including in-process calls, inter-process communication, remote machines, embedded devices, and secure enclaves (SGX). 

**Key Features:**
- **Pure C++ Experience**: Native C++ type support with automatic code generation
- **Transport Agnostic**: Works across processes, threads, memory arenas, enclaves, and networks
- **Type Safe**: Full C++ type system integration with compile-time verification
- **High Performance**: Zero-copy serialization where possible
- **Modern Design**: C++17 features, coroutine support (planned), and extensive telemetry

The library treats transport protocols and programming APIs as separate concerns, allowing developers to focus on business logic rather than communication details.

---

## What are Remote Procedure Calls?

Remote Procedure Calls (RPC) enable applications to communicate with each other without getting entangled in underlying communication protocols. You can easily create APIs accessible from different machines, processes, arenas, or embedded devices without worrying about serialization.

### How RPC Works

RPC calls are function calls that appear to run locally but are actually serialized and sent to a different location where parameters are unpacked and the function is executed. Return values are sent back similarly, making the caller potentially unaware that the call was made remotely.

**The RPC Flow:**
1. **Caller** → calls function through a **Proxy** (same signature as implementation)
2. **Proxy** → packages call and sends to intended target
3. **Target** → receives call through **Stub** that unpacks request
4. **Stub** → calls actual function on caller's behalf
5. **Return** → results flow back through the same path

### Interface Definition Language (IDL)

Functions are defined in Interface Definition Language files (`.idl`) that describe:
- **Interfaces**: Pure abstract virtual base classes in C++
- **Structures**: Data types with C++ STL support
- **Namespaces**: Organizational boundaries
- **Attributes**: Metadata including JSON schema descriptions

**Example IDL File:**
```idl
namespace calculator {
    
    struct complex_number {
        double real;
        double imaginary;
    };
    
    [description="Mathematical calculator interface"]
    interface i_calculator {
        [description="Adds two integers and returns the result"]
        error_code add(int a, int b, [out, by_value] int& result);
        
        [description="Computes complex number multiplication"]
        error_code multiply_complex(
            const complex_number& a, 
            const complex_number& b, 
            [out, by_value] complex_number& result
        );
        
        [description="Gets the calculation history"]
        error_code get_history([out, by_value] std::vector<std::string>& operations);
    };
}
```

### IDL Syntax and Features

**Supported Types:**
- **Basic Types**: `int`, `uint64_t`, `double`, `bool`, `std::string`
- **STL Containers**: `std::vector<T>`, `std::map<K,V>`, `std::set<T>`, `std::array<T,N>`
- **Smart Types**: `std::optional<T>`, `std::variant<T1,T2,...>`
- **Custom Structures**: User-defined POD types
- **Interface References**: `rpc::shared_ptr<interface_type>`

**Parameter Attributes:**
- **`[in]`**: Input parameter (default)
- **`[out]`**: Output parameter  
- **`[by_value]`**: Pass by value (required for out parameters)
- **`[by_ref]`**: Pass by reference (default for in parameters)

**Interface Attributes:**
- **`[description="text"]`**: Human-readable descriptions for JSON schema generation
- **`[deprecated]`**: Mark interfaces/functions as deprecated

---

## Code Generators and Language Support

### The Synchronous Generator

The primary code generator (`synchronous_generator.cpp`) reads IDL files and produces:

1. **Header Files** (`.h`):
   - Pure virtual base classes for interfaces
   - Structure definitions
   - Function metadata with MCP integration
   - Type serialization support

2. **Proxy Implementation** (`.cpp`):
   - Client-side proxy classes that marshal calls
   - Error handling with enclave-compatible logging
   - JSON schema generation for metadata support
   - Template parameter resolution for complex types

3. **Stub Implementation** (`.cpp`):
   - Server-side stub classes that unmarshal calls
   - Automatic interface routing
   - Type-safe parameter handling

**Generated Code Example:**
```cpp
// Generated from calculator.idl
namespace calculator {
    
    class i_calculator_proxy : public rpc::proxy_impl<i_calculator> {
    public:
        error_code add(int a, int b, int& result) override {
            // Automatic marshalling, transport, and error handling
            // ...
        }
    };
    
    // JSON Schema Integration - automatically generated
    std::vector<rpc::function_info> i_calculator::get_function_info() {
        return {
            {"calculator.i_calculator.add", "add", {1}, 0, false, 
             "Adds two integers and returns the result",
             R"({"type":"object","properties":{"a":{"type":"integer"},"b":{"type":"integer"}}})"}
        };
    }
}
```

### Extensibility to Other Languages and Formats

The generator architecture is designed for extensibility:

**Other Serialization Formats:**
- **Current**: YAS (Yet Another Serialization) library
- **Planned**: Protocol Buffers, MessagePack, Apache Avro
- **Custom**: Plugin architecture for proprietary formats

**Other Programming Languages:**
- **Python**: Generate Python binding code with ctypes/pybind11
- **JavaScript/TypeScript**: Generate web-compatible RPC clients
- **Rust**: Generate safe Rust bindings
- **Go**: Generate Go client libraries

**Implementation Pattern:**
```cpp
class custom_generator : public base_generator {
public:
    void generate_language_binding(const interface& iface) {
        // Language-specific code generation
        output_file_ << generate_client_stub(iface);
        output_file_ << generate_serialization(iface);
    }
    
    void generate_serialization_format(const structure& struct_def) {
        // Custom serialization protocol
    }
};
```

---

## Telemetry System

RPC++ includes a comprehensive telemetry system for monitoring distributed RPC operations across complex topologies.

### Core Telemetry Interfaces

**i_telemetry_service** - Base interface for all telemetry implementations:
```cpp
class i_telemetry_service {
public:
    // Service lifecycle tracking
    virtual void on_service_creation(const char* name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) = 0;
    virtual void on_service_deletion(rpc::zone zone_id) = 0;
    
    // RPC operation monitoring  
    virtual void on_service_try_cast(rpc::zone zone_id, rpc::destination_zone destination_zone_id, /* ... */) = 0;
    virtual void on_service_add_ref(/* reference counting parameters */) = 0;
    virtual void on_service_release(/* release parameters */) = 0;
    
    // Interface proxy lifecycle
    virtual void on_interface_proxy_creation(const char* interface_name, /* ... */) = 0;
    virtual void on_interface_proxy_send(const char* method_name, /* ... */) = 0;
    virtual void on_interface_proxy_deletion(/* ... */) = 0;
    
    // Custom events and debugging
    virtual void message(level level, const char* msg) = 0;
};
```

### Telemetry Implementations

**1. Console Telemetry Service**
- Real-time visualization of zone hierarchies and communication
- Color-coded output for different zones
- Topology diagrams showing parent-child relationships
- Thread-safe async logging with spdlog integration

**2. Host Telemetry Service**
- File-based logging for persistent monitoring
- Performance metrics and timing information
- Statistical analysis of RPC patterns
- Integration with external monitoring systems

**3. Enclave Telemetry Service**
- SGX-compatible telemetry for secure enclaves
- Minimal overhead operation
- Secure logging through untrusted proxy

### Telemetry Features

**Zone Topology Visualization:**
```
=== TOPOLOGY DIAGRAM ===
Zone Hierarchy:
  Zone 1: host_service 
  ├─ Zone 2: main_child_service 
     ├─ Zone 3: example_service 
     └─ Zone 4: worker_service
=========================
```

**Real-time Communication Tracking:**
- Inter-zone call monitoring with timing
- Reference counting operation tracking  
- Interface proxy lifecycle management
- Custom event logging with structured data

### Configuration and Usage

```cpp
// Enable comprehensive telemetry
#define USE_RPC_TELEMETRY
#define USE_RPC_TELEMETRY_RAII_LOGGING  // Detailed proxy lifecycle

// Initialize telemetry service
auto telemetry = std::make_shared<rpc::console_telemetry_service>();
rpc::telemetry_service_manager::set(telemetry);

// Automatic integration with RPC operations
// Telemetry calls are inserted automatically by code generation
```

---

## Logging Framework

Separate from telemetry, RPC++ provides a dedicated logging system optimized for performance and enclave compatibility. **Recently refactored in 2024** to use modern fmt::format-based structured logging with improved performance and developer experience.

### Logger Interface

**Enclave-Compatible Design:**
```cpp
// Modern logging with fmt::format support
#ifdef _IN_ENCLAVE
    // SGX-safe logging functions
    extern "C" sgx_status_t rpc_log(int level, const char* str, size_t sz);
#else 
    // Standard logging functions
    extern "C" void rpc_log(int level, const char* str, size_t sz);
#endif

// New structured logging macros with levels (2024 refactoring)
#define RPC_DEBUG(format_str, ...)    // Level 0 - Debug information
#define RPC_TRACE(format_str, ...)    // Level 1 - Detailed tracing
#define RPC_INFO(format_str, ...)     // Level 2 - General information  
#define RPC_WARNING(format_str, ...)  // Level 3 - Warning conditions
#define RPC_ERROR(format_str, ...)    // Level 4 - Error conditions
#define RPC_CRITICAL(format_str, ...) // Level 5 - Critical failures
```

### Recent Logging Refactoring (2024)

**Migration from Legacy Macros:**
The logging system was completely refactored to eliminate the old `LOG_STR` and `LOG_CSTR` macros in favor of modern, structured logging:

```cpp
// OLD STYLE (deprecated and removed)
std::string msg = "Processing request " + std::to_string(id);
LOG_CSTR(msg.c_str());

// NEW STYLE (current)
RPC_INFO("Processing request {}", id);
```

**Key Improvements:**
- **fmt::format Integration**: Direct use of fmt::format for efficient string formatting
- **Eliminated Temporary Strings**: Removed unnecessary string concatenation and temporary variables
- **Structured Logging**: Consistent format string patterns with type-safe parameter substitution
- **Level-based Categorization**: All log calls now properly categorized by severity
- **Generator Code Cleanup**: Fixed code generators to eliminate temporaries in generated code

**Performance Benefits:**
- **Zero String Copying**: Direct formatting eliminates intermediate string objects
- **Compile-time Format Validation**: fmt::format provides compile-time format string checking
- **Reduced Memory Allocations**: Fewer temporary objects means better memory efficiency
- **Better Code Generation**: Generators now produce cleaner, more efficient logging code

### Logging Capabilities

**Performance Optimized:**
- Zero-overhead when disabled (`USE_RPC_LOGGING=OFF`)
- Minimal string formatting overhead with fmt::format
- Direct C-style logging for maximum performance
- Eliminated unnecessary temporary string variables

**Multi-Environment Support:**
- Host applications: Full featured logging with fmt::format
- SGX Enclaves: Secure, restricted logging with format validation
- Embedded systems: Minimal footprint logging with optional fmt support

**Thread Safety:**
- Lock-free logging paths where possible
- Thread-safe buffer management
- Async logging support for high-throughput scenarios
- Atomic reference counting in member_ptr logging

---

## Thread-Local Logging and Enhanced Crash Diagnostics

**NEW FEATURE (September 2024)**: RPC++ now includes a sophisticated thread-local logging system specifically designed for diagnosing multithreaded issues and crashes. This system provides comprehensive diagnostic information when threading bugs occur, making it significantly easier to identify and resolve complex race conditions and multithreaded problems.

### Overview

The thread-local logging system addresses a common challenge in multithreaded debugging: when standard telemetry and logging are enabled, they can mask threading issues by altering timing and synchronization behavior. The new system solves this by:

1. **Per-Thread Circular Buffers**: Each thread maintains its own independent circular buffer of log messages
2. **Frozen Buffer Dumps**: When crashes occur, all buffers are immediately frozen and dumped to disk
3. **Enhanced Stack Traces**: Crash handler provides detailed function names, file locations, and line numbers
4. **Test Context Integration**: Automatically identifies which specific test was running when crashes occur

### Configuration and Setup

#### CMake Configuration

The thread-local logging system is controlled by the `USE_THREAD_LOCAL_LOGGING` CMake option:

```cmake
# Enable thread-local logging (host-only, disabled in enclaves)
set(USE_THREAD_LOCAL_LOGGING ON)
```

#### CMake Presets

A dedicated preset is available for multithreaded debugging:

```bash
# Configure with thread-local logging enabled
cmake --preset Debug_multithreaded

# Build and run tests
cmake --build build --target rpc_test
./build/output/debug/rpc_test -m  # Enable multithreaded tests
```

The `Debug_multithreaded` preset automatically enables:
- `USE_THREAD_LOCAL_LOGGING=ON` - Thread-local circular buffer logging
- `USE_RPC_TELEMETRY=ON` - Standard telemetry system
- `USE_CONSOLE_TELEMETRY=ON` - Console-based telemetry output

### System Architecture

#### Core Components

**1. thread_local_logger_config**: Configuration for circular buffer logging
```cpp
struct thread_local_logger_config {
    size_t buffer_size = 10000;                    // Messages per thread
    size_t max_message_size = 4096;               // Maximum message size  
    std::string dump_directory = "/tmp/rpc_debug_dumps";
};
```

**2. thread_local_circular_buffer**: Per-thread message storage
```cpp
class thread_local_circular_buffer {
public:
    void add_entry(int level, const std::string& message, 
                   const char* file, int line, const char* function);
    void freeze();                    // Prevent further writes
    void dump_to_file(const std::string& filename) const;
};
```

**3. thread_local_logger_manager**: Global coordination and dumping
```cpp
class thread_local_logger_manager {
public:
    static thread_local_logger_manager& get_instance();
    
    thread_local_circular_buffer* get_thread_buffer();
    void freeze_all_buffers();
    void dump_all_buffers_with_stacktrace(const std::string& assert_message, 
                                          const char* file, int line);
};
```

#### Logging Integration

The system seamlessly integrates with existing RPC++ logging macros:

```cpp
// Standard logging macros automatically use thread-local buffers when enabled
RPC_DEBUG("Processing zone {} request", zone_id);
RPC_ERROR("Failed to create proxy for object {}", object_id);
RPC_CRITICAL("Reference counting inconsistency detected");
```

**Backend Selection**: The system automatically chooses the appropriate logging backend:

```cpp
// Conditional compilation determines backend
#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
    #define RPC_LOG_BACKEND(level, message) rpc::thread_local_log(level, message, __FILE__, __LINE__, __FUNCTION__)
#elif defined(USE_RPC_LOGGING) 
    #define RPC_LOG_BACKEND(level, message) rpc_log(level, (message).c_str(), (message).length())
#else
    #define RPC_LOG_BACKEND(level, message) do { (void)(level); (void)(message); } while(0)
#endif
```

### Enhanced Crash Handler Integration

#### Automatic Buffer Dumping

When any signal occurs (SIGSEGV, SIGABRT, etc.), the crash handler automatically:

1. **Freezes All Buffers**: Prevents any further logging to preserve crash state
2. **Dumps Comprehensive Diagnostics**: Creates detailed crash reports with per-thread message histories
3. **Provides Stack Traces**: Enhanced symbol resolution with function names and line numbers
4. **Identifies Test Context**: Shows exactly which gtest test was running

#### Enhanced Stack Traces

The crash handler now provides detailed symbolic information:

```
Thread 1/50 - PID: 403006 (rpc_test) [D]
   0: 0x00005600ed472fdf crash_handler::crash_handler::collect_stack_trace(int) at crash_handler.cpp:237
   1: 0x00005600ed473208 crash_handler::crash_handler::collect_all_thread_stacks(int, int) at crash_handler.cpp:256  
   2: 0x00005600ed451e5d rpc::service::send(...) at service.cpp:250
   3: 0x00005600ed228381 rpc::local_service_proxy::send(...) at basic_service_proxies.h:64
```

**Features**:
- **Function Names**: Full C++ namespaces and function signatures  
- **Source Locations**: File names and exact line numbers
- **Address Information**: Memory addresses for additional debugging
- **Symbol Demangling**: Readable C++ function names

#### Test Integration

Crash reports automatically identify the running test:

```
CRASH DETECTED! Signal: 11 (SIGSEGV (Segmentation fault))
Current Test: remote_type_test/1.multithreaded_bounce_baz_between_two_interfaces
Thread-Local Logs: Check /tmp/rpc_debug_dumps/ for per-thread message history
```

### Diagnostic Output

#### Crash Report Structure

When a crash occurs, the system generates:

**1. Console Output**:
```
=== DUMPING THREAD-LOCAL CIRCULAR BUFFERS ===
*** RPC_ASSERT FAILURE ***
Assert: CRASH SIGNAL: SIGSEGV (Segmentation fault)
Diagnostic files created in: /tmp/rpc_debug_dumps
Main report: /tmp/rpc_debug_dumps/crash_report_20250911_171731.txt
Thread buffers: 101 threads dumped
=== END THREAD-LOCAL BUFFER DUMP ===
```

**2. Main Crash Report** (`crash_report_*.txt`):
```
RPC++ ASSERT FAILURE DIAGNOSTIC REPORT
=====================================

Timestamp: 20250911_171731
Assert Message: CRASH SIGNAL: SIGSEGV (Segmentation fault)  
Location: /path/to/crash_handler.cpp:112
Thread Count: 101

=== THREAD BUFFER FILES ===
Thread 0 (ID: 139677126338240): /tmp/rpc_debug_dumps/thread_0_*.log
Thread 1 (ID: 139677688387264): /tmp/rpc_debug_dumps/thread_1_*.log
...
```

**3. Per-Thread Log Files** (`thread_*_*.log`):
```
Thread ID: 139673317918400
Total entries written: 75
Buffer size: 10000
Buffer frozen: true

=== LOG ENTRIES ===

[17:17:31] Level 0: get_or_create_object_proxy service zone: 1 destination_zone=2, caller_zone=1, object_id = 100 
           (/home/edward/projects/rpc2/rpc/include/rpc/proxy.h:670 in get_or_create_object_proxy)
[17:17:31] Level 0: inner_add_zone_proxy service zone: 1 destination_zone=97, caller_zone=1 
           (/home/edward/projects/rpc2/rpc/src/service.cpp:1338 in inner_add_zone_proxy)
[17:17:31] Level 2: callback 22 
           (/home/edward/projects/rpc2/tests/common/include/common/foo_impl.h:55 in callback)
```

### Usage Examples

#### Basic Usage

```cpp
#include <rpc/thread_local_logger.h>
#include <rpc/logger.h>

int main() {
    // Configure the logger
    rpc::thread_local_logger_config config;
    config.buffer_size = 5000;
    config.dump_directory = "/custom/debug/path";
    
    auto& manager = rpc::thread_local_logger_manager::get_instance();
    manager.configure(config);
    
    // Normal logging - automatically uses thread-local buffers
    RPC_INFO("Starting multithreaded operations");
    
    // Create threads that perform RPC operations
    std::vector<std::thread> workers;
    for (int i = 0; i < 10; ++i) {
        workers.emplace_back([i]() {
            RPC_DEBUG("Worker {} starting", i);
            // ... perform RPC operations ...
            RPC_DEBUG("Worker {} completed", i);
        });
    }
    
    for (auto& worker : workers) {
        worker.join();
    }
    
    return 0;
}
```

#### Manual Diagnostic Dumping

```cpp
// Manually trigger diagnostic dump (useful for testing)
rpc::thread_local_dump_on_assert("Manual diagnostic dump", __FILE__, __LINE__);
```

### Debugging Workflow

#### 1. Enable Thread-Local Logging
```bash
cmake --preset Debug_multithreaded
cmake --build build --target rpc_test
```

#### 2. Run Multithreaded Tests
```bash
./build/output/debug/rpc_test -m  # Enable multithreaded tests
```

#### 3. Analyze Crash Output
When crashes occur:
1. **Check console output** for immediate crash context
2. **Review main crash report** for thread overview
3. **Examine per-thread logs** for detailed operation histories
4. **Correlate timestamps** across threads to understand interaction patterns

#### 4. Common Analysis Patterns

**Timeline Reconstruction**:
- Compare timestamps across multiple thread logs
- Identify the sequence of operations leading to the crash
- Look for timing-dependent race conditions

**Resource Management Issues**:
- Search for `add_ref` and `release` operations
- Verify proper reference counting patterns  
- Check for proxy creation/cleanup imbalances

**Zone Communication Problems**:
- Track inter-zone calls and routing operations
- Identify zone proxy creation and cleanup patterns
- Look for destination/caller zone mismatches

### Performance Considerations

#### Memory Usage
- **Per-thread overhead**: ~40KB per thread (10,000 messages × ~4KB each)
- **Total system impact**: Scales linearly with thread count
- **Configurable limits**: Adjust `buffer_size` based on available memory

#### Performance Impact
- **Minimal runtime overhead**: Atomic operations for thread-safe logging
- **Zero impact when disabled**: Compile-time elimination with `USE_THREAD_LOCAL_LOGGING=OFF`
- **Frozen state efficiency**: No performance impact after crash (buffers frozen)

#### Best Practices

**1. Configure Appropriate Buffer Sizes**:
```cpp
// For high-volume logging scenarios
config.buffer_size = 50000;  // More messages per thread

// For memory-constrained environments  
config.buffer_size = 1000;   // Fewer messages per thread
```

**2. Selective Enabling**:
- Enable only for debugging multithreaded issues
- Disable in production builds for optimal performance
- Use preset configurations for consistent setups

**3. Log Analysis Tools**:
- Use grep/awk to correlate timestamps across thread logs
- Search for specific operation patterns (e.g., "add_ref", "service::send")
- Create scripts to visualize thread interaction timelines

### Security Considerations

#### Enclave Compatibility
- **Disabled in enclaves**: `!defined(_IN_ENCLAVE)` condition prevents activation
- **Host-only feature**: Thread-local logging only works in trusted host environments
- **Secure by design**: No sensitive data exposed through logging paths

#### File System Access
- **Configurable dump directory**: Choose appropriate secure locations
- **File permissions**: Diagnostic files created with standard user permissions
- **Temporary data**: Consider using tmpfs for sensitive debugging scenarios

---

## Multithreading and Coroutines

### Current Multithreading Support

RPC++ is designed as a **fully multithreaded system** with comprehensive thread-safety mechanisms:

**Thread-Safe Components:**
- **Service Registration**: Atomic reference counting and mutex-protected data structures
- **Proxy Management**: Thread-safe proxy creation, caching, and cleanup
- **Object Lifecycle**: Reference counting with race condition detection
- **Zone Communication**: Synchronized inter-zone call routing

**Synchronization Primitives:**
```cpp
// Service-level synchronization
std::recursive_mutex service_protect_; // Service state protection
std::mutex insert_control_;            // Proxy insertion synchronization

// Reference counting atomics
std::atomic<int> lifetime_lock_count_;  // External reference tracking
std::atomic<int> inherited_reference_count_; // Race condition handling
```


## Thread Safety and member_ptr

RPC++ is designed to be thread-safe and supports concurrent access to RPC services from multiple threads. A key component of this thread safety is the `member_ptr` template class.

### What is member_ptr?

`member_ptr` is a thread-safe wrapper around `std::shared_ptr` and `rpc::shared_ptr` that guarantees pointer consistency in multithreaded environments. It prevents race conditions where a shared pointer could change between multiple accesses within the same function call.

### The Problem

In a multithreaded RPC environment, member variables that hold shared pointers could potentially be modified by other threads during function execution. For example:

```cpp
// PROBLEMATIC CODE - Race condition possible
class MyService {
    std::shared_ptr<SomeService> service_;
public:
    void someMethod() {
        if (service_) {                    // First access - service_ could be valid
            service_->doSomething();       // Second access - service_ could be null if another thread cleared it
        }
    }
};
```

### The Solution: member_ptr

`member_ptr` solves this by requiring a single access per function that caches the shared pointer value:

```cpp
// SAFE CODE - Guaranteed consistency
class MyService {
    rpc::member_ptr<SomeService> service_;
public:
    void someMethod() {
        auto service = service_.get_nullable();  // Single access - cached for entire function
        if (service) {                          // Use cached value
            service->doSomething();             // Use same cached value - guaranteed consistent
        }
    }
};
```

### Key Features

- **Single Access Guarantee**: Each function should call `get_nullable()` only once and cache the result
- **Thread Safety**: Multiple threads can safely access the same `member_ptr` without data races
- **Consistency**: The cached shared_ptr remains valid throughout the function lifetime
- **Performance**: Minimal overhead compared to raw shared_ptr usage
- **Standard Interface**: Provides familiar `reset()` method and assignment operators

### Available in Two Flavors

```cpp
namespace stdex {
    template<typename T>
    class member_ptr;  // Wraps std::shared_ptr<T>
}

namespace rpc {
    template<typename T> 
    class member_ptr;   // Wraps rpc::shared_ptr<T>
}
```

### Usage Patterns

**Correct Usage:**
```cpp
void MyClass::processRequest() {
    auto service = service_.get_nullable();  // Cache once
    if (service) {
        service->method1();                  // Use cached value
        service->method2();                  // Use same cached value
    }
}
```

**Incorrect Usage:**
```cpp
void MyClass::processRequest() {
    if (service_.get_nullable()) {           // First call
        service_.get_nullable()->method();   // Second call - could be different!
    }
}
```

### Reset and Assignment

```cpp
// Reset to null
service_.reset();

// Assignment from shared_ptr
service_ = rpc::member_ptr<ServiceType>(my_shared_ptr);

// Direct assignment (also supported)
service_ = my_shared_ptr;
```

This design ensures that RPC++ services remain robust and thread-safe even under heavy concurrent load, preventing subtle race conditions that could cause crashes or undefined behavior.


**Threading Debug System:**
- Comprehensive crash handler with multi-threaded stack trace collection
- Threading bug detection and pattern analysis
- Reference counting validation and leak detection
- Real-time thread state monitoring

### Coroutine Support (Planned)

**Async/Await Pattern:**
```cpp
// Planned coroutine interface
rpc::task<int> calculator_service::add_async(int a, int b) {
    // Asynchronous RPC call
    auto result = co_await remote_calculator->add(a, b);
    co_return result;
}

// Usage
auto task = calculator->add_async(5, 3);
int result = co_await task;
```

**Benefits of Coroutine Integration:**
- **Non-blocking I/O**: Efficient handling of remote calls
- **Scalability**: Support for thousands of concurrent operations
- **Composability**: Easy chaining of async operations
- **Exception Safety**: Proper cleanup on coroutine cancellation

### Thread Safety Best Practices

**Reference Counting Patterns:**
```cpp
// RAII-based reference management
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

---

## Service Proxies and Transport Abstraction

Service proxies provide the transport abstraction layer that enables RPC++ to work across different communication mechanisms.

### Why Custom Service Proxies?

Users are **strongly encouraged** to write custom service proxies to:

1. **Attach Different Transports**:
   - TCP/IP networking
   - Named pipes
   - Shared memory
   - Message queues
   - Custom protocols

2. **Implement Security Subsystems**:
   - Authentication and authorization
   - Encryption and secure channels
   - Certificate validation
   - Access control policies

3. **Add Custom Features**:
   - Compression and decompression
   - Request routing and load balancing
   - Circuit breakers and retry logic
   - Custom serialization formats

### Service Proxy Architecture

**Base Class Interface:**
```cpp
class service_proxy : public i_marshaller {
protected:
    destination_zone destination_zone_id_;
    zone caller_zone_id_;
    
public:
    // Core marshalling interface
    virtual int send(uint64_t protocol_version, encoding enc, uint64_t tag,
                    caller_channel_zone caller_channel_zone_id,
                    caller_zone caller_zone_id, destination_zone destination_zone_id,
                    object object_id, interface_ordinal interface_id, method method_id,
                    size_t data_in_sz, const char* data_in, 
                    std::vector<char>& data_out) = 0;
    
    // Reference counting for distributed objects
    virtual uint64_t add_ref(uint64_t protocol_version, /* ... */) = 0;
    virtual uint64_t release(uint64_t protocol_version, /* ... */) = 0;
    virtual int try_cast(uint64_t protocol_version, /* ... */) = 0;
    
    // Lifecycle management
    virtual void add_external_ref() = 0;
    virtual int release_external_ref() = 0;
};
```

### Built-in Service Proxy Types

**1. In-Memory Service Proxy**
- Direct function calls within the same process
- Zero serialization overhead
- Immediate execution

**2. Arena Service Proxy** 
- Communication between different memory arenas
- Memory management isolation
- Fault tolerance boundaries

**3. SGX Enclave Service Proxy**
- Secure communication with SGX enclaves
- Hardware-enforced security boundaries
- Attestation and secure channel establishment

**4. Host Service Proxy**
- Communication from enclave to untrusted host
- Careful data validation and sanitization
- Security policy enforcement

### Custom Transport Implementation Example

```cpp
class tcp_service_proxy : public service_proxy {
private:
    std::string remote_host_;
    uint16_t remote_port_;
    std::unique_ptr<tcp_connection> connection_;
    
public:
    tcp_service_proxy(const std::string& host, uint16_t port) 
        : remote_host_(host), remote_port_(port) {
        connection_ = std::make_unique<tcp_connection>(host, port);
    }
    
    int send(uint64_t protocol_version, encoding enc, uint64_t tag,
             caller_channel_zone caller_channel_zone_id,
             caller_zone caller_zone_id, destination_zone destination_zone_id,
             object object_id, interface_ordinal interface_id, method method_id,
             size_t data_in_sz, const char* data_in,
             std::vector<char>& data_out) override {
        
        // 1. Serialize RPC header
        rpc_header header{protocol_version, enc, tag, /* ... */};
        
        // 2. Send header + payload over TCP
        connection_->send(reinterpret_cast<char*>(&header), sizeof(header));
        connection_->send(data_in, data_in_sz);
        
        // 3. Receive response
        rpc_response_header response;
        connection_->receive(reinterpret_cast<char*>(&response), sizeof(response));
        
        data_out.resize(response.payload_size);
        connection_->receive(data_out.data(), response.payload_size);
        
        return response.error_code;
    }
    
    // Implement reference counting, try_cast, etc.
};
```

### Security Integration Example

```cpp
class authenticated_service_proxy : public service_proxy {
private:
    std::shared_ptr<authentication_service> auth_;
    std::string session_token_;
    
public:
    int send(/* parameters */) override {
        // 1. Validate session token
        if (!auth_->validate_token(session_token_)) {
            return rpc::error::AUTHENTICATION_FAILED();
        }
        
        // 2. Apply authorization policies
        if (!auth_->authorize_call(interface_id, method_id, session_token_)) {
            return rpc::error::AUTHORIZATION_FAILED();
        }
        
        // 3. Encrypt payload
        auto encrypted_data = encrypt_payload(data_in, data_in_sz);
        
        // 4. Forward to underlying transport
        return underlying_proxy_->send(/* encrypted parameters */);
    }
};
```

---

## The i_marshaller Interface

The `i_marshaller` interface is the core abstraction that enables RPC++ to work across different transport mechanisms and execution contexts.

### Interface Definition

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

### Function-by-Function Analysis

#### 1. send() - Core RPC Call Marshalling

**Purpose**: The primary function for executing remote procedure calls.

**Parameters Explained:**
- **`protocol_version`**: Version compatibility check for wire protocol
- **`encoding`**: Serialization format (YAS binary, JSON, etc.)  
- **`tag`**: Unique identifier for call correlation and debugging
- **Zone Parameters**: Complex routing information for multi-hop calls
- **`object_id`**: Target object instance identifier
- **`interface_id`**: Interface type identifier for polymorphic dispatch
- **`method_id`**: Specific method within the interface
- **`data_in/data_out`**: Serialized parameter data

**Use Cases:**
```cpp
// Direct function call marshalling
int result = send(RPC_VERSION_1, encoding::YAS_BINARY, generate_tag(),
                 caller_channel, caller_zone, destination_zone,
                 object_42, i_calculator_id, add_method_id,
                 serialized_params.size(), serialized_params.data(),
                 response_buffer);
```

#### 2. try_cast() - Interface Type Checking

**Purpose**: Determine if a remote object supports a specific interface (similar to `dynamic_cast`).

**Implementation Pattern:**
```cpp
// Check if remote object supports i_calculator interface
int cast_result = try_cast(RPC_VERSION_1, destination_zone, object_id, i_calculator_id);
if (cast_result == rpc::error::OK()) {
    // Object supports interface, safe to call calculator methods
    auto calculator = create_interface_proxy<i_calculator>(object_id);
}
```

**Error Cases:**
- `rpc::error::INTERFACE_NOT_SUPPORTED()` - Object doesn't implement interface
- `rpc::error::OBJECT_NOT_FOUND()` - Object no longer exists
- `rpc::error::TRANSPORT_ERROR()` - Communication failure

#### 3. add_ref() - Complex Reference Counting

**Purpose**: The most sophisticated function in the interface, managing distributed object lifetimes across complex zone topologies.

**The add_ref_options System:**
```cpp
enum add_ref_options {
    normal = 1,                    // Standard reference counting
    build_destination_route = 2,   // Unidirectional add_ref to destination
    build_caller_route = 4         // Unidirectional add_ref to caller (reverse)
};
```

**Complex Routing Scenarios:**

**Scenario 1: Direct Reference** (`options = normal`)
```
Zone A → Zone B
```
Simple increment of reference count on target object.

**Scenario 2: Destination Route Building** (`options = build_destination_route`)
```
Zone A → Zone B → Zone C
```
Establishes forward routing path for future calls from A to C via B.

**Scenario 3: Caller Route Building** (`options = build_caller_route`)  
```
Zone C ← Zone B ← Zone A
```
Establishes reverse routing path for callbacks from C back to A via B.

**Scenario 4: Bidirectional Route Building** (`options = build_destination_route | build_caller_route`)
```
Zone A ↔ Zone B ↔ Zone C
```
Establishes routing in both directions for complex interface passing scenarios.

**Channel vs Zone Distinction:**
- **`destination_zone_id`**: Final target zone containing the object
- **`destination_channel_zone_id`**: Next hop in routing chain (may be different)
- **`caller_zone_id`**: Original zone making the reference
- **`caller_channel_zone_id`**: Previous hop in routing chain

**Edge Case Handling:**
The implementation includes two critical untested paths:

1. **Line 792 Path**: When `dest_channel == caller_channel && build_channel` - occurs when zone is "passing the buck" to another zone
2. **Line 870 Path**: When building caller route but caller zone is unknown - uses `get_parent()` fallback

#### 4. release() - Reference Cleanup

**Purpose**: Decrement reference count and clean up resources when objects are no longer needed.

**Implementation Considerations:**
```cpp
// Standard release pattern
uint64_t remaining_refs = release(RPC_VERSION_1, destination_zone, object_id, caller_zone);
if (remaining_refs == 0) {
    // Object has been destroyed on remote side
    // Local proxy should be cleaned up
}
```

**Synchronization with add_ref():**
Every `add_ref()` call must be balanced with a corresponding `release()` call to prevent resource leaks. The reference counting system tracks this automatically but requires careful implementation in custom service proxies.

### Implementation Best Practices

**1. Error Handling:**
```cpp
int send_result = send(/* parameters */);
if (send_result != rpc::error::OK()) {
    // Handle transport errors, timeouts, serialization failures
    RPC_ERROR("RPC call failed with error: {}", send_result);
    return send_result;
}
```

**2. Reference Counting Balance:**
```cpp
// Always balance add_ref with release
auto ref_count = add_ref(/* parameters */);
// ... use object ...
auto remaining = release(/* parameters */);
```

**3. Protocol Version Handling:**
```cpp
if (protocol_version > MAX_SUPPORTED_VERSION) {
    return rpc::error::VERSION_NOT_SUPPORTED();
}
```

---

## Getting Started

### Prerequisites

- **C++ Compiler**: Clang 10+, GCC 9.4+, or Visual Studio 2017+
- **CMake**: Version 3.24 or higher
- **Build System**: Ninja (recommended) or Make

### Basic Setup

1. **Clone and Configure:**
```bash
git clone <rpc-repository>
cd rpc2
mkdir build
cd build
cmake .. -G Ninja
```

2. **Build the Library:**
```bash
cmake --build . --target rpc
```

3. **Create Your First IDL:**
```idl
// hello.idl
namespace hello {
    [description="Simple greeting service"]
    interface i_greeter {
        [description="Returns a personalized greeting"]
        error_code greet(const std::string& name, [out, by_value] std::string& greeting);
    };
}
```

4. **Generate Code:**
```bash
cmake --build . --target hello_idl
```

5. **Implement Service:**
```cpp
#include "hello/hello.h"

class greeter_impl : public hello::i_greeter {
public:
    error_code greet(const std::string& name, std::string& greeting) override {
        greeting = "Hello, " + name + "!";
        return rpc::error::OK();
    }
};
```

### CMake Integration

```cmake
# Add RPC++ to your project
find_package(rpc REQUIRED)

# Generate code from IDL
RPCGenerate(
    hello                    # Target name
    hello.idl               # IDL file
    HEADER hello/hello.h    # Generated header
    PROXY hello/hello_proxy.cpp
    STUB hello/hello_stub.cpp
)

# Link with generated code
target_link_libraries(your_app PRIVATE rpc hello)
```

---

## Architecture Overview

### High-Level System Design

```
┌─────────────────────────────────────────────────────────────────┐
│                        Application Layer                        │
├─────────────────────────────────────────────────────────────────┤
│  Interface Implementations  │  Generated Proxies & Stubs       │
├─────────────────────────────────────────────────────────────────┤
│                      RPC++ Core Library                        │  
│  ├── Service Management    ├── Object Lifecycle               │
│  ├── Reference Counting    ├── Zone Communication             │
│  └── Interface Routing     └── Error Handling                 │
├─────────────────────────────────────────────────────────────────┤
│                     Service Proxy Layer                        │
│  ├── In-Memory Proxy      ├── Network Proxy                   │
│  ├── SGX Enclave Proxy    ├── Custom Transport Proxy          │
│  └── Arena Proxy          └── Authentication & Security       │
├─────────────────────────────────────────────────────────────────┤
│                    Serialization Layer                         │
│  ├── YAS Binary Format    ├── JSON Format                     │ 
│  └── Custom Formats       └── Compression & Encryption        │
├─────────────────────────────────────────────────────────────────┤
│                      Transport Layer                           │
│  ├── In-Process Calls     ├── TCP/IP Networking               │
│  ├── Shared Memory        ├── Named Pipes                     │
│  ├── SGX Enclaves         └── Custom Protocols                │
└─────────────────────────────────────────────────────────────────┘
```

### Key Architectural Principles

1. **Separation of Concerns**: Transport, serialization, and business logic are independent
2. **Type Safety**: Full C++ type system integration with compile-time verification  
3. **Performance**: Zero-copy paths and minimal overhead design
4. **Extensibility**: Plugin architecture for custom transports and formats
5. **Reliability**: Comprehensive error handling and reference counting
6. **Security**: Built-in support for secure enclaves and authentication

---

**RPC++ - Modern C++ Remote Procedure Calls**
*Bringing distributed computing into the type-safe, high-performance world of modern C++*