/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <memory>

namespace rpc {

// Forward declarations
struct thread_local_logger_config;
struct log_entry;
class thread_local_circular_buffer;
class thread_local_logger_manager;

// Configuration struct for circular buffer logging
struct thread_local_logger_config {
    static constexpr size_t default_buffer_size = 10000;  // Configurable number of messages per thread
    static constexpr size_t default_max_message_size = 4096;      // Maximum message size
    
    size_t buffer_size = default_buffer_size;
    size_t max_message_size = default_max_message_size;
    std::string dump_directory = "/tmp/rpc_debug_dumps";
};

// Individual log entry structure
struct log_entry {
    std::chrono::high_resolution_clock::time_point timestamp;
    int level;
    std::string message;
    const char* file;
    int line;
    const char* function;
    
    log_entry() = default;
    log_entry(int lvl, const std::string& msg, const char* f, int l, const char* func);
};

// Thread-local circular buffer for logging
class thread_local_circular_buffer {
private:
    std::vector<log_entry> buffer_;
    std::atomic<size_t> write_index_{0};
    std::atomic<size_t> entries_written_{0};
    std::atomic<bool> frozen_{false};
    std::thread::id thread_id_;
    
public:
    explicit thread_local_circular_buffer(size_t size);
    
    // Add a log entry to the circular buffer
    void add_entry(int level, const std::string& message, const char* file, int line, const char* function);
    
    // Freeze the buffer to prevent further writes
    void freeze();
    
    // Check if buffer is frozen
    bool is_frozen() const;
    
    // Get thread ID
    std::thread::id get_thread_id() const;
    
    // Dump buffer contents to a file
    void dump_to_file(const std::string& filename) const;
};

// Global manager for all thread-local buffers
class thread_local_logger_manager {
private:
    static thread_local_logger_manager* instance_;
    static std::mutex instance_mutex_;
    
    thread_local_logger_config config_;
    std::mutex buffers_mutex_;
    std::unordered_map<std::thread::id, std::unique_ptr<thread_local_circular_buffer>> buffers_;
    std::atomic<bool> global_freeze_{false};
    
    thread_local_logger_manager() = default;
    
public:
    static thread_local_logger_manager& get_instance();
    
    // Get or create buffer for current thread
    thread_local_circular_buffer* get_thread_buffer();
    
    // Freeze all buffers globally
    void freeze_all_buffers();
    
    // Dump all buffers to files with stack trace
    void dump_all_buffers_with_stacktrace(const std::string& assert_message, const char* file, int line);
    
    // Enhanced version that includes detailed stack trace information
    void dump_all_buffers_with_enhanced_stacktrace(const std::string& assert_message, const char* file, int line, const std::string& stack_trace);
    
    // Configure the logger
    void configure(const thread_local_logger_config& config);
    
    // Get current configuration
    const thread_local_logger_config& get_config() const;
};

// Global functions for logging (inline for performance)
inline void thread_local_log(int level, const std::string& message, const char* file, int line, const char* function) {
    auto& manager = thread_local_logger_manager::get_instance();
    auto* buffer = manager.get_thread_buffer();
    if (buffer) {
        buffer->add_entry(level, message, file, line, function);
    }
}

inline void thread_local_dump_on_assert(const std::string& assert_message, const char* file, int line) {
    auto& manager = thread_local_logger_manager::get_instance();
    manager.dump_all_buffers_with_stacktrace(assert_message, file, line);
}

// Enhanced version that accepts stack trace information
inline void thread_local_dump_on_assert_with_stacktrace(const std::string& assert_message, const char* file, int line, const std::string& stack_trace) {
    auto& manager = thread_local_logger_manager::get_instance();
    manager.dump_all_buffers_with_enhanced_stacktrace(assert_message, file, line, stack_trace);
}

} // namespace rpc

// Note: RPC_* macros are now defined in rpc/logger.h using the unified RPC_LOG_BACKEND approach

#endif // defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)