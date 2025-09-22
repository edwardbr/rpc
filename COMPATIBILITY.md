<!--
Copyright (c) 2025 Edward Boggis-Rolfe
All rights reserved.
-->

# RPC++ Compatibility Grid

**Version 2.2.0 Compatibility Matrix**

Note this was written by Claude so there may be some errors
---

## Platform Support

| Platform | Version 2 | Version 3 | Notes |
|----------|-----------|-----------|-------|
| **Linux** | ✅ Full | ✅ Full | Complete support for all features |
| **Windows** | ✅ Full | 🔶 Partial | Coroutines not yet supported in v3 |
| **SGX Enclaves** | ✅ Sync Only | 🔶 Sync Only | Coroutines not yet supported in v3 |

---

## Execution Modes

### Version 2 (Legacy)
| Mode | Linux | Windows | SGX Enclaves |
|------|-------|---------|--------------|
| **Synchronous RPC** | ✅ | ✅ | ✅ |
| **Fire and Forget** | ✅ | ✅ | ✅ |
| **Coroutines** | ❌ | ❌ | ❌ |

### Version 3 (Currently in development)
| Mode | Linux | Windows | SGX Enclaves |
|------|-------|---------|--------------|
| **Synchronous RPC** | ✅ | ✅ | ✅ |
| **Fire and Forget** | ✅ | ✅ | ✅ |
| **Coroutines** | ✅ | 🚧 | 🚧 |

**Legend:**
- ✅ **Fully Supported** - Production ready
- 🔶 **Partial Support** - Some limitations
- 🚧 **In Development** - Actively being developed
- ❌ **Not Supported** - Not available

---

## Transport Layer Compatibility

### Local Transports
| Transport | Linux | Windows | SGX Enclaves | Coroutine Mode |
|-----------|-------|---------|--------------|----------------|
| **Local Child Service** | ✅ | ✅ | ✅ | Both modes |
| **Memory Arena** | ✅ | ✅ | ✅ | Both modes |

### Secure Transports
| Transport | Linux | Windows | SGX Enclaves | Coroutine Mode |
|-----------|-------|---------|--------------|----------------|
| **SGX Enclave** | ✅ | ✅ | ✅ | Sync only |
| **Enclave to Host Service** | ✅ | ✅ | ✅ | Sync only |

### Network Transports (Coroutine-Only)
| Transport | Linux | Windows | SGX Enclaves | Notes |
|-----------|-------|---------|--------------|-------|
| **TCP Network** | ✅ | 🚧 | ❌ | Requires some socket or IO_URING support |
| **SPSC Queue** | ✅ | 🚧 | ❌ | Inter-process communication |

---

## Compiler Support

### Version 3 Requirements
| Compiler | Version | Linux | Windows | SGX | Coroutines |
|----------|---------|-------|---------|-----|------------|
| **Clang** | 10.0+ | ✅ | ✅ | ✅ | ✅ (Linux only) |
| **GCC** | 9.4+ | ✅ | ➖ | ✅ | ✅ |
| **MSVC** | VS 2017+ | ➖ | ✅ | ✅ | 🚧 |

### Version 2 Requirements (Legacy)
| Compiler | Version | Linux | Windows | SGX | Notes |
|----------|---------|-------|---------|-----|-------|
| **CMake** | 3.24+ | ✅ | ✅ | ✅ | Required |
| **Clang** | 8.0+ | ✅ | ✅ | ✅ | Sync/Fire-and-forget only |
| **GCC** | 7.0+ | ✅ | ➖ | ✅ | Sync/Fire-and-forget only |
| **MSVC** | VS 2015+ | ➖ | ✅ | ✅ | Sync/Fire-and-forget only |

---

## Build System Compatibility

| Build Tool | Linux | Windows | SGX | Version |
|-------------|-------|---------|-----|---------|
| **CMake** | 3.24+ | 3.24+ | 3.24+ | Required |
| **Ninja** | ✅ | ✅ | ✅ | Recommended |
| **Make** | ✅ | ➖ | ✅ | Alternative |
| **Visual Studio** | ➖ | ✅ | ✅ | Windows native |

---

## Development Status

### Version 3 Coroutine Support Roadmap

**Currently Supported:**
- ✅ Linux with Clang 10+ and GCC 9.4+
- ✅ TCP and SPSC transports on Linux
- ✅ Full async/await programming model

**In Active Development:**
- 🚧 Windows coroutine support (MSVC and Clang)
- 🚧 SGX enclave coroutine support
- 🚧 Cross-platform coroutine transport layers

---

## Migration Guidelines

### From Version 2 to Version 3

**Immediate Benefits (All Platforms):**
- Enhanced error handling and automatic fallback
- JSON schema generation for API documentation
- Improved telemetry and monitoring capabilities
- Coroutines

**Platform-Specific Considerations:**

**Linux Users:**
- Can immediately adopt coroutine mode for high-performance scenarios
- TCP and SPSC transports available for distributed applications
- Full feature parity with synchronous mode

**Windows Users:**
- Start with synchronous mode (BUILD_COROUTINE=OFF)
- Prepare code for coroutine migration using bi-modal macros
- Monitor development updates for coroutine support

**SGX Enclave Users:**
- Continue using synchronous mode for now
- Benefit from improved error handling and telemetry
- Plan for future coroutine integration

---

## Testing Recommendations

### Platform-Specific Testing

**Linux Development:**
```bash
# Test both execution modes
cmake --preset Debug -DBUILD_COROUTINE=OFF  # Sync mode
cmake --preset Debug -DBUILD_COROUTINE=ON   # Coroutine mode
```

**Windows Development:**
```bash
# Current recommendation
cmake --preset Debug -DBUILD_COROUTINE=OFF  # Sync mode only
```

**SGX Development:**
```bash
# Enclave builds
cmake --preset Debug_SGX -DBUILD_COROUTINE=OFF
```

### Cross-Platform Validation

For applications targeting multiple platforms:

1. **Develop** using bi-modal macros (CORO_TASK, CO_AWAIT, CO_RETURN)
2. **Test** in synchronous mode on all target platforms
3. **Enable** coroutines on Linux for performance validation
4. **Monitor** Windows/SGX coroutine support for future migration

---

*This compatibility matrix is updated with each release. For the latest status, check the project repository and release notes.*