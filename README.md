# RPC++
Remote Procedure Calls for modern C++

Copyright (c) 2024 Edward Boggis-Rolfe
All rights reserved.

## Intro
This library implements an RPC solution that reflects modern C++ concepts.  It is intended for developers that want a pure C++ experience and supports C++ types natively, and all other languages communicate with this library using JSON or some other generic protocol.

## What are Remote Procedure Calls?
Remote procedure calls allow applications to talk each other without getting snarled up in underlying communication protocols.  The transport and the end user programming API's are treated as separate concerns.
### In a nutshell
In other words you can easily make an API that is accessible from another machine, dll, arena or embedded device and not think about serialization.
### A bit of history
RPC has been around for decades targeting mainly the C programming language and was very popular in the 80's and 90's. The technology reached its zenith with the arrival of (D)COM from Microsoft and CORBA from a consortium of other companies.  Unfortunately both organizations hated each other and with their closed source attitudes people fell out of love with them in favour with XMLRPC, SOAP and REST as the new sexy kids on the block.

RPC though is a valuable solution for all solutions, however historically it did not offer direct answers for working over insecure networks, partly because of the short sighted intransigence and secrecy of various organizations.  People are coming off from some of the other aging fads and are returning to higher performance solutions such as this.

### Yes but what are they?
RPC calls are function calls that pretend to run code locally, when in fact the calls are serialized up and sent to a different location such as a remote machine, where the parameters are unpacked and the function call is made.  The return values are then sent back in a similar manner back to the caller, with the caller potentially unaware that the call was made on a remote location.

Typically the caller calls the function with the same signature as the implementation through a 'proxy', The proxy then packages up the call and sends it to the intended target.  The target then calls a 'stub' that unpacks the request and calls the function on the callers behalf.

To do this the intended destination object needs to implement a set of agreed functions.  These functions collectively are known as interfaces, or in C++ speak a pure abstract virtual base classes.  These interfaces are then defined in a language not dissimilar to C++ header files called Interface Definition Language files with an extension of *.idl.

A code generator then reads this idl and generates pure C++ headers containing virtual base classes that implementors need to satisfy as well as proxy and stub code.

The only thing left to do is to implement code that uses a particular transport for getting the calls to their destination and back again.  As well as an rpc::service object that acts as the function calling router for that component.

## Use cases

 * In-process calls.
 * In-process calls between different memory arenas.
 * To other applications on the same machine.
 * To embedded devices.
 * To remote devices on a private network or the Internet.

## Design

This idl format supports structures, interfaces, namespaces and basic STL type objects including string, map, vector, optional, variant.

TODO...

## Feature pipelines

This implementation now supports both **synchronous calls** and **asynchronous coroutines** (C++20), with the mode controlled at build time by the `BUILD_COROUTINE` macro.
This solution currently only supports error codes, exception based error handling is to be implemented at some date.

Currently it has adaptors for
 * in memory calls
 * calls between different memory arenas
 * calls to SGX enclaves

This implementation currently uses YAS for its serialization needs, however it could be extended to other serializers in the future.

## Building and installation
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

SHA3 Credit to: https://github.com/brainhub/SHA3IUF

## Coroutines Support

RPC++ provides comprehensive support for C++20 coroutines, allowing you to write asynchronous RPC code that looks and feels like synchronous code while providing the performance benefits of non-blocking I/O.

### Build Configuration

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

### Programming Model

#### Coroutine-Enabled Functions

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

#### Calling Coroutine Functions

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

### Service Setup

#### Coroutine-Enabled Services

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

### Coroutine Macros

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

### Error Handling

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

### Performance Considerations

#### Benefits of Coroutines
- **Non-blocking I/O**: Threads aren't blocked waiting for network/disk operations
- **High Concurrency**: Handle thousands of concurrent operations efficiently  
- **Resource Efficiency**: Lower memory overhead compared to thread-per-request models
- **Scalability**: Better performance under high load

#### When to Use Each Mode
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

### Migration Strategy

The dual-mode design allows gradual migration:

1. **Start Synchronous**: Begin development with `BUILD_COROUTINE=OFF`
2. **Test Both Modes**: Ensure code compiles and works in both configurations
3. **Performance Test**: Benchmark both modes under your workload
4. **Deploy Appropriately**: Choose mode based on deployment requirements

### Advanced Usage

#### Custom Schedulers

```cpp
// Custom scheduler with thread pool
class custom_scheduler : public coro::io_scheduler {
    // Custom implementation
};

auto scheduler = std::make_shared<custom_scheduler>();
auto service = std::make_shared<rpc::service>("MyService", zone_id, scheduler);
```

#### Mixed Async/Sync Operations

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
