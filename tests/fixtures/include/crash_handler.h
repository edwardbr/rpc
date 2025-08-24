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
    class CrashHandler
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
        struct ThreadInfo
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
        struct CrashReport
        {
            int signal_number;
            std::string signal_name;
            void* crash_address;
            pid_t crashed_thread_id;
            std::vector<ThreadInfo> all_threads;
            std::vector<std::string> detected_patterns;
            std::string threading_debug_info;
            std::chrono::system_clock::time_point crash_time;
        };

        /**
         * Callback for custom crash analysis
         */
        using CrashAnalysisCallback = std::function<void(const CrashReport&)>;

    private:
        static CrashHandler* instance_;
        static Config config_;
        static CrashAnalysisCallback analysis_callback_;
        
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
        static bool Initialize(const Config& config);
        
        /**
         * Initialize the crash handler with default configuration
         */
        static bool Initialize();

        /**
         * Shutdown and restore original signal handlers
         */
        static void Shutdown();

        /**
         * Set custom crash analysis callback
         */
        static void SetAnalysisCallback(CrashAnalysisCallback callback);

        /**
         * Get current instance (nullptr if not initialized)
         */
        static CrashHandler* GetInstance() { return instance_; }

        /**
         * Manual crash report generation (for testing)
         */
        static CrashReport GenerateCrashReport(int signal = 0);

        /**
         * Print crash report to stdout
         */
        static void PrintCrashReport(const CrashReport& report);

    private:
        CrashHandler() = default;
        ~CrashHandler() = default;

        // Signal handlers
        static void HandleCrash(int signal, siginfo_t* info, void* context);

        // Stack trace collection
        static std::vector<void*> CollectStackTrace(int max_frames);
        static std::vector<ThreadInfo> CollectAllThreadStacks(int max_threads, int max_frames);
        static std::string GetThreadName(pid_t thread_id);
        static std::string GetThreadState(pid_t thread_id);

        // Symbol resolution
        static std::vector<std::string> ResolveSymbols(const std::vector<void*>& addresses);
        static std::string ResolveSymbolWithAddr2Line(void* address);

        // Pattern detection
        static std::vector<std::string> DetectCrashPatterns(const CrashReport& report);

        // Threading debug integration
        static std::string CollectThreadingDebugInfo();

        // Utility functions
        static std::string SignalToString(int signal);
        static std::string FormatAddress(void* address);
        static void SaveCrashDump(const CrashReport& report);

        // Thread enumeration using /proc filesystem
        static std::vector<pid_t> EnumerateThreads();
    };

} // namespace crash_handler