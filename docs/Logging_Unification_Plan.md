# RPC++ Logging Unification Plan

**Version**: 1.0  
**Date**: 2025-09-08  
**Status**: Planning Phase  

## Executive Summary

This document outlines a comprehensive plan to unify the fractured logging experience in RPC++ by implementing a consistent spdlog-based architecture with logical thread tracking, crash-safe ring buffers, and flexible telemetry file separation.

## Current Issues Identified

### 1. **Fractured Logging Experience**
- `rpc_global_logger.cpp` uses `printf()` instead of spdlog (lines 103-128)
- `console_telemetry_service.cpp` properly uses spdlog but with inconsistent patterns
- `test_service_logger.cpp` uses `std::ostringstream` + `rpc_global_logger::info()`
- `rpc_log.cpp` contains `std::cerr` usage (line 156)

### 2. **Missing Logical Thread ID Support**
No mechanism for tracking logical thread contexts across zone boundaries, which is critical for enclave-to-enclave calls where the same logical operation may spawn different OS threads to prevent stack exhaustion.

### 3. **No Per-Thread/Per-Telemetry File Separation**
Everything goes to console or single file, making it difficult to isolate telemetry from different components or logical threads.

### 4. **No Crash-Safe Ring Buffer**
No mechanism to capture recent logs for crash analysis, making debugging difficult when crashes occur.

### 5. **No Client Logger Injection Support**
Client applications cannot inject their own global spdlog implementation that the library is unaware of.

## Comprehensive Solution Architecture

### 1. **Unified Logging Manager**

Create a central `rpc_logging_manager` that manages all logging concerns:

```cpp
class rpc_logging_manager {
private:
    // Logical thread ID management
    static thread_local uint64_t logical_thread_id_;
    static std::atomic<uint64_t> next_logical_thread_id_;
    
    // Per-thread ring buffers for crash safety
    static thread_local std::unique_ptr<circular_buffer<std::string>> thread_ring_buffer_;
    
    // Configurable spdlog instances
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    std::shared_ptr<spdlog::logger> default_logger_;
    std::shared_ptr<spdlog::logger> injected_global_logger_; // For client injection
    
public:
    // Logical thread management
    static uint64_t get_logical_thread_id();
    static void set_logical_thread_id(uint64_t id);
    static uint64_t spawn_child_logical_thread();
    
    // Logger creation and management
    std::shared_ptr<spdlog::logger> create_telemetry_logger(const std::string& name, const std::string& filepath);
    std::shared_ptr<spdlog::logger> create_per_thread_logger(const std::string& base_name);
    
    // Client logger injection
    void inject_global_logger(std::shared_ptr<spdlog::logger> logger);
    
    // Crash-safe ring buffer management
    void log_to_ring_buffer(const std::string& message);
    void dump_all_ring_buffers_to_file(const std::string& crash_dump_path);
    
    // Signal handler integration
    static void register_crash_handler();
};
```

### 2. **Enhanced RPC Logger Interface**

Replace the current `rpc_log()` function with an enhanced version:

```cpp
// New unified logging interface
extern "C" {
    void rpc_log_enhanced(
        int level, 
        const char* str, 
        size_t sz, 
        const char* source_component = nullptr,  // "telemetry", "service", "proxy", etc.
        uint64_t logical_thread_id = 0          // 0 = auto-detect
    );
}

// Enhanced macros with logical thread tracking
#define RPC_DEBUG_CTX(component, format_str, ...) do { \
    auto formatted = fmt::format("[T:{}] " format_str, \
        rpc_logging_manager::get_logical_thread_id(), ##__VA_ARGS__); \
    rpc_log_enhanced(0, formatted.c_str(), formatted.length(), component); \
} while(0)

#define RPC_INFO_CTX(component, format_str, ...) do { \
    auto formatted = fmt::format("[T:{}] " format_str, \
        rpc_logging_manager::get_logical_thread_id(), ##__VA_ARGS__); \
    rpc_log_enhanced(2, formatted.c_str(), formatted.length(), component); \
} while(0)

// Similar macros for TRACE, WARNING, ERROR, CRITICAL
```

### 3. **Telemetry Service Specialization**

Create specialized telemetry services with separate logging capabilities:

```cpp
class file_telemetry_service : public i_telemetry_service {
private:
    std::shared_ptr<spdlog::logger> telemetry_logger_;
    std::string log_file_path_;
    
public:
    // Constructor creates dedicated log file
    file_telemetry_service(const std::string& test_name, const std::string& base_path);
    
    // All telemetry methods write to dedicated file
    void on_service_creation(...) override;
    void on_service_proxy_creation(...) override;
    // ... other telemetry methods
};

class per_thread_telemetry_service : public i_telemetry_service {
private:
    std::unordered_map<uint64_t, std::shared_ptr<spdlog::logger>> thread_loggers_;
    std::string base_log_directory_;
    
public:
    // Creates separate log file per logical thread
    void ensure_thread_logger(uint64_t logical_thread_id);
    
    // Routes telemetry to appropriate thread-specific logger
    void on_service_creation(...) override;
};

class console_telemetry_service_enhanced : public console_telemetry_service {
private:
    bool enable_logical_thread_ids_ = true;
    
public:
    // Enhanced console output with logical thread information
    void on_service_creation(...) override;
    // Includes logical thread ID in all output
};
```

### 4. **Crash-Safe Ring Buffer System**

Implement thread-local circular buffers for crash analysis:

```cpp
template<typename T>
class circular_buffer {
private:
    std::vector<T> buffer_;
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t capacity_;
    std::atomic<bool> full_{false};
    
public:
    explicit circular_buffer(size_t capacity) : capacity_(capacity) {
        buffer_.resize(capacity);
    }
    
    void push(const T& item) {
        buffer_[head_] = item;
        head_ = (head_ + 1) % capacity_;
        if (full_) {
            tail_ = (tail_ + 1) % capacity_;
        }
        full_ = (head_ == tail_);
    }
    
    std::vector<T> dump_recent_entries() const;
    size_t size() const;
    bool empty() const;
};

// Signal handler integration
void crash_signal_handler(int sig) {
    // Dump all thread ring buffers to crash file
    rpc_logging_manager::dump_all_ring_buffers_to_file("./rpc_crash_dump.txt");
    
    // Re-raise signal for normal crash handling
    signal(sig, SIG_DFL);
    raise(sig);
}
```

### 5. **Logical Thread ID Management**

Support for enclave-to-enclave logical thread continuity:

```cpp
// In zone transition points
void zone_boundary_cross(uint64_t destination_zone) {
    uint64_t current_logical_id = rpc_logging_manager::get_logical_thread_id();
    
    // Pass logical thread ID through RPC metadata
    rpc_metadata metadata;
    metadata.logical_thread_id = current_logical_id;
    
    // On destination side, restore logical thread context
    rpc_logging_manager::set_logical_thread_id(metadata.logical_thread_id);
}

// For stack exhaustion scenarios, spawn child logical thread
uint64_t spawn_continuation_thread() {
    uint64_t child_id = rpc_logging_manager::spawn_child_logical_thread();
    
    // Log the relationship
    RPC_INFO_CTX("threading", "Spawning child logical thread {} from parent {}", 
                 child_id, rpc_logging_manager::get_logical_thread_id());
    
    return child_id;
}

// Thread ID inheritance patterns
enum class thread_id_inheritance {
    NEW_ROOT,        // Start new logical thread tree
    INHERIT_PARENT,  // Use parent's logical thread ID
    SPAWN_CHILD      // Create child of current logical thread
};
```

### 6. **Configuration and Factory Pattern**

Allow flexible logger configuration:

```cpp
struct logging_config {
    bool enable_ring_buffers = true;
    size_t ring_buffer_size = 1000;
    bool separate_telemetry_files = false;
    bool per_thread_files = false;
    std::string base_log_directory = "./logs";
    spdlog::level::level_enum log_level = spdlog::level::info;
    bool enable_logical_thread_ids = true;
    bool async_logging = true;
    size_t async_queue_size = 8192;
};

class logging_factory {
public:
    static void initialize(const logging_config& config);
    static void shutdown();
    
    static std::shared_ptr<i_telemetry_service> create_telemetry_service(
        const std::string& type,     // "console", "file", "per_thread" 
        const std::string& test_name,
        const std::string& base_path = "./logs"
    );
    
    // Factory methods for different logger types
    static std::shared_ptr<spdlog::logger> create_component_logger(const std::string& component_name);
    static std::shared_ptr<spdlog::logger> create_test_logger(const std::string& test_name);
};
```

## Implementation Priority

### Phase 1 (High Priority - Immediate Fixes)
**Target**: Fix critical inconsistencies and thread safety issues

1. **Replace printf() calls in rpc_global_logger.cpp**
   - Convert all `printf()` calls to spdlog equivalent
   - Ensure thread-safe logger initialization
   - Fix commented-out spdlog initialization code

2. **Implement basic logical thread ID tracking**
   - Add thread-local storage for logical thread IDs
   - Implement `get_logical_thread_id()` and `set_logical_thread_id()`
   - Update RPC macros to include logical thread ID

3. **Create unified rpc_log_enhanced() function**
   - Replace current `rpc_log()` with enhanced version
   - Add component tagging support
   - Maintain backward compatibility

4. **Fix std::cerr usage in crash handlers**
   - Replace `std::cerr` with proper logging in `rpc_log.cpp:156`
   - Ensure crash handlers are spdlog-safe

**Deliverables**:
- Consistent spdlog usage across all components
- Basic logical thread ID support
- Enhanced logging interface

### Phase 2 (Medium Priority - Enhanced Features)
**Target**: Add crash safety and advanced telemetry features

1. **Implement crash-safe ring buffers**
   - Thread-local circular buffers for recent log entries
   - Signal handler integration for crash dumps
   - Configurable buffer sizes

2. **Create file-based telemetry services**
   - `file_telemetry_service` for dedicated telemetry files
   - Integration with existing telemetry factory
   - Test-specific log file naming

3. **Add signal handler integration for crash dumps**
   - Register signal handlers for SIGSEGV, SIGABRT, etc.
   - Dump ring buffer contents on crash
   - Preserve crash dump files with timestamps

4. **Enhance console telemetry with logical thread IDs**
   - Update console output to include logical thread information
   - Color-coded logical thread identification
   - Hierarchical thread relationship display

**Deliverables**:
- Crash-safe logging with ring buffers
- File-based telemetry separation
- Enhanced crash analysis capabilities

### Phase 3 (Future Enhancement - Advanced Features)
**Target**: Production-ready advanced logging features

1. **Implement per-thread file separation**
   - `per_thread_telemetry_service` implementation
   - Automatic thread-specific log file creation
   - Thread lifecycle management

2. **Add client logger injection support**
   - API for clients to inject their own spdlog loggers
   - Transparent integration with existing logging
   - Configuration validation and fallback

3. **Create advanced telemetry filtering and routing**
   - Component-based log filtering
   - Dynamic log level adjustment
   - Log routing based on logical thread hierarchies

4. **Performance optimization**
   - Async logging with configurable queue sizes
   - Lock-free logging paths where possible
   - Memory-mapped log files for high throughput

**Deliverables**:
- Production-ready logging system
- Client integration capabilities
- High-performance async logging

## Benefits of This Architecture

### **Consistency**
- All logging goes through spdlog with unified formatting
- Consistent log levels and patterns across all components
- Elimination of printf/cout/cerr inconsistencies

### **Thread Safety**
- Logical thread IDs work across zone boundaries
- Thread-safe logger initialization and access
- Race condition elimination in multi-threaded scenarios

### **Flexibility** 
- Separate files for different telemetry types and threads
- Configurable logging levels and output destinations
- Support for both synchronous and asynchronous logging

### **Crash Safety**
- Ring buffers preserve recent log entries for crash analysis
- Signal handler integration for automatic crash dumps
- Persistent crash information for debugging

### **Client Integration**
- Support for injected global loggers from client applications
- Transparent integration without breaking existing functionality
- Configuration flexibility for different deployment scenarios

### **Performance**
- Async logging with minimal overhead
- Lock-free logging paths where possible
- Configurable buffer sizes for different performance requirements

## Migration Strategy

### Backward Compatibility
- Maintain existing `rpc_log()` function as wrapper around enhanced version
- Preserve current RPC logging macros while adding enhanced versions
- Gradual migration path for existing code

### Testing Strategy
- Unit tests for logical thread ID management
- Integration tests for crash-safe ring buffers
- Performance tests comparing old vs new logging systems
- Multi-threaded stress tests for thread safety validation

### Documentation Updates
- Update user guide with new logging capabilities
- Developer documentation for logical thread ID usage
- Configuration guide for different logging scenarios

## Conclusion

This logging unification plan addresses all identified issues in the current RPC++ logging system while providing a foundation for advanced features. The phased implementation approach allows for incremental improvements while maintaining system stability.

The logical thread ID system is particularly important for the enclave-to-enclave communication scenarios where OS threads may change but the logical operation context must be preserved for debugging and telemetry purposes.

Implementation of this plan will result in a production-ready, thread-safe, and highly configurable logging system that supports the complex multi-zone architecture of RPC++ while providing excellent debugging and monitoring capabilities.

---

*This document should be reviewed and updated as implementation progresses and new requirements are identified.*