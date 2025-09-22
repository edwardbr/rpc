/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>

#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)

#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>

#ifdef _IN_ENCLAVE
#include <fmt/format-inl.h>
#else
#include <fmt/format.h>
#endif

namespace rpc
{

    // Static member definitions
    thread_local_logger_manager* thread_local_logger_manager::instance_ = nullptr;
    std::mutex thread_local_logger_manager::instance_mutex_;

    // log_entry implementation
    log_entry::log_entry(int lvl, const std::string& msg, const char* f, int l, const char* func)
        : timestamp(std::chrono::high_resolution_clock::now())
        , level(lvl)
        , message(msg)
        , file(f)
        , line(l)
        , function(func)
    {
    }

    // thread_local_circular_buffer implementation
    thread_local_circular_buffer::thread_local_circular_buffer(size_t size)
        : buffer_(size)
        , thread_id_(std::this_thread::get_id())
    {
    }

    void thread_local_circular_buffer::add_entry(
        int level, const std::string& message, const char* file, int line, const char* function)
    {
        if (frozen_.load(std::memory_order_acquire))
        {
            return; // Buffer is frozen, don't write
        }

        size_t index = write_index_.fetch_add(1, std::memory_order_relaxed) % buffer_.size();
        buffer_[index] = log_entry(level, message, file, line, function);
        entries_written_.fetch_add(1, std::memory_order_relaxed);
    }

    void thread_local_circular_buffer::freeze()
    {
        frozen_.store(true, std::memory_order_release);
    }

    bool thread_local_circular_buffer::is_frozen() const
    {
        return frozen_.load(std::memory_order_acquire);
    }

    std::thread::id thread_local_circular_buffer::get_thread_id() const
    {
        return thread_id_;
    }

    void thread_local_circular_buffer::dump_to_file(const std::string& filename) const
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            return;
        }

        file << "Thread ID: " << thread_id_ << "\n";
        file << "Total entries written: " << entries_written_.load() << "\n";
        file << "Buffer size: " << buffer_.size() << "\n";
        file << "Buffer frozen: " << (frozen_.load() ? "true" : "false") << "\n";
        file << "\n=== LOG ENTRIES ===\n\n";

        // Determine start position for circular buffer
        size_t entries = std::min(entries_written_.load(), buffer_.size());
        size_t start_index = 0;

        if (entries_written_.load() > buffer_.size())
        {
            // Buffer wrapped around, start from current write position
            start_index = write_index_.load() % buffer_.size();
        }

        // Dump entries in chronological order
        for (size_t i = 0; i < entries; ++i)
        {
            size_t index = (start_index + i) % buffer_.size();
            const auto& entry = buffer_[index];

            if (!entry.message.empty())
            { // Only dump valid entries
                auto time_t = std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now()
                    + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                        entry.timestamp - std::chrono::high_resolution_clock::now()));

                struct tm tm_buf;
#ifdef _WIN32
                localtime_s(&tm_buf, &time_t);
#else
                localtime_r(&time_t, &tm_buf);
#endif
                file << "[" << std::put_time(&tm_buf, "%H:%M:%S") << "] ";
                file << "Level " << entry.level << ": " << entry.message;
                if (entry.file && entry.function)
                {
                    file << " (" << entry.file << ":" << entry.line << " in " << entry.function << ")";
                }
                file << "\n";
            }
        }

        file.close();
    }

    // thread_local_logger_manager implementation
    thread_local_logger_manager& thread_local_logger_manager::get_instance()
    {
        std::lock_guard<std::mutex> lock(instance_mutex_);
        if (!instance_)
        {
            instance_ = new thread_local_logger_manager();
        }
        return *instance_;
    }

    thread_local_circular_buffer* thread_local_logger_manager::get_thread_buffer()
    {
        if (global_freeze_.load(std::memory_order_acquire))
        {
            return nullptr; // All logging frozen
        }

        std::thread::id tid = std::this_thread::get_id();

        {
            std::lock_guard<std::mutex> lock(buffers_mutex_);
            auto it = buffers_.find(tid);
            if (it != buffers_.end())
            {
                return it->second.get();
            }

            // Create new buffer for this thread
            auto buffer = std::make_unique<thread_local_circular_buffer>(config_.buffer_size);
            auto* buffer_ptr = buffer.get();
            buffers_[tid] = std::move(buffer);
            return buffer_ptr;
        }
    }

    void thread_local_logger_manager::freeze_all_buffers()
    {
        global_freeze_.store(true, std::memory_order_release);

        std::lock_guard<std::mutex> lock(buffers_mutex_);
        for (auto& [tid, buffer] : buffers_)
        {
            buffer->freeze();
        }
    }

    void thread_local_logger_manager::dump_all_buffers_with_stacktrace(
        const std::string& assert_message, const char* file, int line)
    {
        dump_all_buffers_with_enhanced_stacktrace(assert_message, file, line, "");
    }

    void thread_local_logger_manager::dump_all_buffers_with_enhanced_stacktrace(
        const std::string& assert_message, const char* file, int line, const std::string& stack_trace)
    {
        freeze_all_buffers();

        // Create dump directory
        std::string dump_dir = config_.dump_directory;
        std::system(("mkdir -p " + dump_dir).c_str());

        // Generate unique timestamp for this crash
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        struct tm tm_buf;
#ifdef _WIN32
        localtime_s(&tm_buf, &time_t);
#else
        localtime_r(&time_t, &tm_buf);
#endif
        ss << std::put_time(&tm_buf, "%Y%m%d_%H%M%S");
        std::string timestamp = ss.str();

        // Create main crash report
        std::string crash_report_file = dump_dir + "/crash_report_" + timestamp + ".txt";
        std::ofstream crash_report(crash_report_file);

        crash_report << "RPC++ CRASH DIAGNOSTIC REPORT\n";
        crash_report << "==============================\n\n";
        crash_report << "Timestamp: " << timestamp << "\n";
        crash_report << "Assert Message: " << assert_message << "\n";
        crash_report << "Location: " << file << ":" << line << "\n";
        crash_report << "Thread Count: " << buffers_.size() << "\n\n";

        // Include enhanced stack trace if provided
        if (!stack_trace.empty())
        {
            crash_report << "=== ENHANCED STACK TRACE ===\n";
            crash_report << stack_trace << "\n";
        }
        else
        {
            crash_report << "=== CALL STACK ===\n";
            crash_report << "Stack trace functionality would go here\n";
            crash_report << "(requires additional implementation with backtrace or similar)\n\n";
        }

        crash_report << "=== THREAD BUFFER FILES ===\n";

        // Dump each thread buffer to separate file
        {
            std::lock_guard<std::mutex> lock(buffers_mutex_);
            int thread_counter = 0;
            for (auto& [tid, buffer] : buffers_)
            {
                std::stringstream thread_ss;
                thread_ss << tid;
                std::string thread_id_str = thread_ss.str();

                std::string buffer_file = dump_dir + "/thread_" + std::to_string(thread_counter) + "_" + thread_id_str
                                          + "_" + timestamp + ".log";

                buffer->dump_to_file(buffer_file);
                crash_report << "Thread " << thread_counter << " (ID: " << thread_id_str << "): " << buffer_file << "\n";
                thread_counter++;
            }
        }

        crash_report << "\n=== TELEMETRY INFORMATION ===\n";
        crash_report << "Note: If telemetry is enabled, additional logs may be available in:\n";
        crash_report << "- Console output (if USE_CONSOLE_TELEMETRY=ON)\n";
        crash_report << "- Telemetry service logs (if USE_RPC_TELEMETRY=ON)\n";
        crash_report << "- Check application logs for telemetry topology diagrams\n\n";

        crash_report << "=== END REPORT ===\n";
        crash_report.close();

        // Print summary to stderr
        std::fprintf(stderr, "\n*** RPC_ASSERT FAILURE ***\n");
        std::fprintf(stderr, "Assert: %s\n", assert_message.c_str());
        std::fprintf(stderr, "Location: %s:%d\n", file, line);
        std::fprintf(stderr, "Diagnostic files created in: %s\n", dump_dir.c_str());
        std::fprintf(stderr, "Main report: %s\n", crash_report_file.c_str());
        std::fprintf(stderr, "Thread buffers: %zu threads dumped\n", buffers_.size());
        std::fprintf(stderr, "****************************\n\n");
    }

    void thread_local_logger_manager::configure(const thread_local_logger_config& config)
    {
        config_ = config;
    }

    const thread_local_logger_config& thread_local_logger_manager::get_config() const
    {
        return config_;
    }

} // namespace rpc

#endif // defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
