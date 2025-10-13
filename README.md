# RPC++

[![License](https://img.shields.io/badge/license-All%20Rights%20Reserved-red.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.24%2B-green.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)](README.md)

**A Modern C++ Remote Procedure Call Library for High-Performance Distributed Systems**

RPC++ enables type-safe communication across execution contexts including in-process calls, inter-process communication, remote machines, embedded devices, and secure enclaves (SGX). Build applications that scale from embedded systems to distributed microservices using the same C++ code.

Please notify me if you see egregious errors in the documentation

---

## 🚀 Key Features

- **🔒 Type-Safe**: Full C++ type system integration with compile-time verification
- **🌐 Transport Agnostic**: Works across processes, threads, memory arenas, enclaves, and networks
- **🌐 Format Agnostic**: Works with JSON, YAS binary, can be extended to others such as protocol buffers
- **⚡ Location independence**: Local calls to local objects and serialization to remote objects
- **🔄 Bi-Modal Execution**: Same code runs in both blocking and coroutine modes
- **🛡️ Production Ready**: Comprehensive telemetry, logging, and error handling
- **🔧 Modern C++**: C++17 features with optional C++20 coroutine support
- **📊 Auto-Generated Docs**: JSON schema generation for API documentation

---

## 📋 Table of Contents

- [Quick Start](#-quick-start)
- [Core Concepts](#-core-concepts)
- [Supported Transports](#-supported-transports)
- [Build Configuration](#-build-configuration)
- [Examples](#-examples)
- [Documentation](#-documentation)
- [Contributing](#-contributing)
- [Requirements](#-requirements)

---

## 🚀 Quick Start

### Prerequisites

- **C++17 Compiler**: Clang 10+, GCC 9.4+, or Visual Studio 2017+
- **CMake**: 3.24 or higher
- **Build System**: Ninja (recommended)

### Build and Test

```bash
# Clone and configure
git clone https://github.com/edwardbr/rpc
cd rpc2
cmake --preset Debug

# Build core library
cmake --build build --target rpc

# Run tests
cmake --build build --target rpc_test
./build/tests/test_host/rpc_test
```

### Hello World Example

**calculator.idl:**
```idl
namespace calculator {
    [inline] namespace v1 {
        [status=production]
        interface i_calculator {
            int add(int a, int b, [out] int& result);
        };
    }
}
```

**Usage:**
```cpp
#include "generated/calculator/calculator.h"

// Create service and connect to calculator zone
auto root_service = std::make_shared<rpc::service>("root", rpc::zone{1});
rpc::shared_ptr<calculator::v1::i_calculator> calc;

auto error = CO_AWAIT root_service->connect_to_zone<...>(
    "calc_zone", rpc::zone{2}, nullptr, calc, setup_callback);

// Make RPC call
int result;
error = CO_AWAIT calc->add(5, 3, result);
std::cout << "5 + 3 = " << result << std::endl;  // Output: 5 + 3 = 8
```

---

## 🧩 Core Concepts

### Interface Definition Language (IDL)

Define interfaces using familiar C++ syntax with RPC attributes:

```idl
[status=production, description="Calculator service"]
interface i_calculator {
    [description="Adds two integers"]
    int add(int a, int b, [out] int& result);

    [description="Process bulk data efficiently"]
    int process_data([in] std::vector<double>&& data, [out] std::vector<double>& results);
};
```

### Bi-Modal Execution

Write once, deploy as either **blocking** or **coroutine** mode:

```cpp
// Same code works in both modes!
CORO_TASK(int) calculate() {
    int result;
    auto error = CO_AWAIT calc->add(10, 20, result);
    CO_RETURN error;
}

// Blocking mode: CORO_TASK(int) → int, CO_AWAIT → direct call
// Coroutine mode: CORO_TASK(int) → coro::task<int>, CO_AWAIT → co_await
```

### Zone-Based Architecture

Applications are organized into **zones** that communicate through service proxies:

```cpp
// Root service
auto root = std::make_shared<rpc::service>("root", rpc::zone{1});

// Child zone with automatic lifecycle management
rpc::shared_ptr<i_service> service_proxy;
auto error = CO_AWAIT root->connect_to_zone<rpc::local_child_service_proxy<...>>(
    "worker_zone", rpc::zone{2}, input, service_proxy, setup_callback);
// Zone automatically destroyed when service_proxy goes out of scope
```

---

## 🌐 Supported Transports

| Transport | Description | Requirements | Use Cases |
|-----------|-------------|--------------|-----------|
| **Local Child** | In-process communication | None | Microservices, modules |
| **SGX Enclave** | Secure enclave communication | SGX SDK | Secure computation |
| **SPSC Queue** | Inter-process shared memory | Coroutines | High-performance IPC |
| **TCP Network** | Network communication | Coroutines | Distributed systems |
| **Memory Arena** | Different memory spaces | None | Embedded, real-time |

---

## ⚙️ Build Configuration

### CMake Presets

```bash
# Standard builds
cmake --preset Debug          # Full-featured debug build
cmake --preset Release        # Optimized production build

# Specialized builds
cmake --preset Debug_SGX      # SGX enclave support
cmake --preset Debug_SGX_Sim  # SGX simulation mode
```

### Key Build Options

```cmake
# Execution mode
BUILD_COROUTINE=ON       # Enable async/await support

# Features
BUILD_ENCLAVE=ON         # SGX enclave support
BUILD_TEST=ON            # Test suite

# Development
USE_RPC_LOGGING=ON       # Comprehensive logging
DEBUG_RPC_GEN=ON         # Code generation debugging
```

### Bi-Modal Development

```bash
# Unit testing and debugging (blocking mode)
cmake --preset Debug -DBUILD_COROUTINE=OFF

# Production deployment (coroutine mode)
cmake --preset Debug -DBUILD_COROUTINE=ON
```

---

## 📝 Examples

### Error Handling

```cpp
auto error = CO_AWAIT proxy->risky_operation(params);
if (error == rpc::error::OK()) {
    // Success
} else if (error == rpc::error::TRANSPORT_ERROR()) {
    // Handle network issues
} else {
    RPC_ERROR("Operation failed: {}", rpc::error::to_string(error));
}
```

### Object Lifecycle Management

```cpp
// Objects automatically marshalled across zones
rpc::shared_ptr<i_business_object> obj;
factory->create_object(obj);

// Automatic reference counting across zone boundaries
worker->process_object(obj);  // add_ref/release handled automatically
```

### High-Performance Data Transfer

```cpp
// Move semantics for zero-copy transfers
interface i_processor {
    int process_bulk([in] std::vector<large_data>&& data, [out] result& output);
};

// Server receives moved object directly from deserialization
error_code process_bulk(std::vector<large_data>&& data, result& output) override {
    stored_data_ = std::move(data);  // No additional copies
    return process_internal();
}
```

---

## 📚 Documentation

- **[User Guide](docs/RPC++_User_Guide.md)** - Comprehensive documentation
- **[Build Instructions](docs/RPC++_User_Guide.md#build-system-and-configuration)** - Detailed build configuration
- **[IDL Reference](docs/RPC++_User_Guide.md#interface-definition-language-idl)** - Interface definition syntax

---

## 🛠️ Requirements

### Supported Platforms
- **Windows**: Visual Studio 2017+
- **Linux**: Ubuntu 18.04+, CentOS 8+
- **Embedded**: Any platform with C++17 support

### Compilers
- **Clang**: 10.0+
- **GCC**: 9.4+
- **MSVC**: Visual Studio 2017+

### Dependencies
Git submodule manages external dependencies
- **CMake**: 3.24+
- **YAS**: Serialization (included)
- **libcoro**: Coroutine support (when `BUILD_COROUTINE=ON`)

---

## 🤝 Contributing

RPC++ is actively maintained. While the repository is currently private, we welcome discussions about:

- Performance optimizations
- New transport implementations
- Platform ports
- Documentation improvements

---

## 📄 License

Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
MIT Licence

---

**SHA3 Implementation**: Credit to [brainhub/SHA3IUF](https://github.com/brainhub/SHA3IUF)

---

*For technical questions and detailed API documentation, see the [User Guide](docs/RPC++_User_Guide.md).*
