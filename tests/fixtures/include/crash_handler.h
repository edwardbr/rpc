/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include <csignal>
#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <sys/types.h>

namespace crash_handler
{
    /**
     * Comprehensive crash handling system with multi-threaded stack trace support
     * 
     * Features:
     * - Multi-threaded stack trace collection
     * - Symbol resolution with addr2line integration
     * - Thread state analysis
     * - Custom crash pattern detection
     * - Integration with RPC debugging systems
     */
    class crash_handler
    {
    public:
        /**
         * Configuration options for crash handling
         */
        struct Config
        {
            Config() = default;
            
            bool enable_multithreaded_traces = true;
            bool enable_symbol_resolution = true;
            bool enable_threading_debug_info = true;
            bool enable_pattern_detection = true;
            int max_stack_frames = 64;
            int max_threads = 100;
            bool save_crash_dump = false;
            std::string crash_dump_path = "/tmp";
        };

        /**
         * Thread information for crash analysis
         */
        struct thread_info
        {
            pid_t thread_id;
            std::string thread_name;
            std::vector<void*> stack_frames;
            std::vector<std::string> symbols;
            std::string state;
        };

        /**
         * Comprehensive crash report
         */
        struct crash_report
        {
            int signal_number;
            std::string signal_name;
            void* crash_address;
            pid_t crashed_thread_id;
            std::vector<thread_info> all_threads;
            std::vector<std::string> detected_patterns;
            std::string threading_debug_info;
            std::chrono::system_clock::time_point crash_time;
        };

        /**
         * Callback for custom crash analysis
         */
        using crash_analysis_callback = std::function<void(const crash_report&)>;

    private:
        static crash_handler* instance_;
        static Config config_;
        static crash_analysis_callback analysis_callback_;
        
        // Signal handling
        static struct sigaction old_sigsegv_handler_;
        static struct sigaction old_sigabrt_handler_;
        static struct sigaction old_sigfpe_handler_;
        static struct sigaction old_sigill_handler_;
        static struct sigaction old_sigterm_handler_;

    public:
        /**
         * Initialize the crash handler with given configuration
         */
        static bool initialize(const Config& config);
        
        /**
         * Initialize the crash handler with default configuration
         */
        static bool initialize();

        /**
         * Shutdown and restore original signal handlers
         */
        static void shutdown();

        /**
         * Set custom crash analysis callback
         */
        static void set_analysis_callback(crash_analysis_callback callback);

        /**
         * Get current instance (nullptr if not initialized)
         */
        static crash_handler* get_instance() { return instance_; }

        /**
         * Manual crash report generation (for testing)
         */
        static crash_report generate_crash_report(int signal = 0);

        /**
         * Print crash report to stdout
         */
        static void print_crash_report(const crash_report& report);

    private:
        crash_handler() = default;
        ~crash_handler() = default;

        // Signal handlers
        static void handle_crash(int signal, siginfo_t* info, void* context);

        // Stack trace collection
        static std::vector<void*> collect_stack_trace(int max_frames);
        static std::vector<thread_info> collect_all_thread_stacks(int max_threads, int max_frames);
        static std::string get_thread_name(pid_t thread_id);
        static std::string get_thread_state(pid_t thread_id);

        // Symbol resolution
        static std::vector<std::string> resolve_symbols(const std::vector<void*>& addresses);
        static std::string resolve_symbol_with_addr2_line(void* address);

        // Pattern detection
        static std::vector<std::string> detect_crash_patterns(const crash_report& report);

        // Threading debug integration
        static std::string collect_threading_debug_info();

        // Utility functions
        static std::string signal_to_string(int signal);
        static std::string format_address(void* address);
        static std::string format_stack_trace_for_file(const crash_report& report);
        static void save_crash_dump(const crash_report& report);

        // Thread enumeration using /proc filesystem
        static std::vector<pid_t> enumerate_threads();
    };

} // namespace crash_handler