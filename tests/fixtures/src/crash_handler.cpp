/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "crash_handler.h"

// Crash handler implementation only available on POSIX systems
#ifndef _WIN32
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <execinfo.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <cstring>
#include <cstdlib>
#include <dlfcn.h>
#include <link.h>
#include <gtest/gtest.h>
#include <cxxabi.h>

// Threading debug integration
#ifdef RPC_THREADING_DEBUG
#include <rpc/threading_debug.h>
#endif

// Thread-local logging integration
#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
#include <rpc/thread_local_logger.h>
#endif

namespace crash_handler
{
    // Static member definitions
    crash_handler* crash_handler::instance_ = nullptr;
    crash_handler::Config crash_handler::config_;
    crash_handler::crash_analysis_callback crash_handler::analysis_callback_;
    
    struct sigaction crash_handler::old_sigsegv_handler_;
    struct sigaction crash_handler::old_sigabrt_handler_;
    struct sigaction crash_handler::old_sigfpe_handler_;
    struct sigaction crash_handler::old_sigill_handler_;
    struct sigaction crash_handler::old_sigterm_handler_;

    bool crash_handler::initialize()
    {
        Config default_config;
        return initialize(default_config);
    }

    bool crash_handler::initialize(const Config& config)
    {
        if (instance_ != nullptr) {
            return false; // Already initialized
        }

        config_ = config;
        instance_ = new crash_handler();

        // Install signal handlers
        struct sigaction sa;
        sa.sa_sigaction = handle_crash;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO | SA_RESTART;

        // Install handlers and save old ones
        if (sigaction(SIGSEGV, &sa, &old_sigsegv_handler_) != 0) {
            std::cerr << "Failed to install SIGSEGV handler" << std::endl;
            return false;
        }
        sigaction(SIGABRT, &sa, &old_sigabrt_handler_);
        sigaction(SIGFPE, &sa, &old_sigfpe_handler_);
        sigaction(SIGILL, &sa, &old_sigill_handler_);
        sigaction(SIGTERM, &sa, &old_sigterm_handler_);

        std::cout << "[crash_handler] Initialized with multi-threaded stack trace support" << std::endl;
        return true;
    }

    void crash_handler::shutdown()
    {
        if (instance_ == nullptr) {
            return;
        }

        // Restore original signal handlers
        sigaction(SIGSEGV, &old_sigsegv_handler_, nullptr);
        sigaction(SIGABRT, &old_sigabrt_handler_, nullptr);
        sigaction(SIGFPE, &old_sigfpe_handler_, nullptr);
        sigaction(SIGILL, &old_sigill_handler_, nullptr);
        sigaction(SIGTERM, &old_sigterm_handler_, nullptr);

        delete instance_;
        instance_ = nullptr;
        analysis_callback_ = nullptr;

        std::cout << "[crash_handler] Shutdown complete" << std::endl;
    }

    void crash_handler::set_analysis_callback(crash_analysis_callback callback)
    {
        analysis_callback_ = callback;
    }

    void crash_handler::handle_crash(int signal, siginfo_t* info, void* context)
    {
        // Generate comprehensive crash report first
        auto report = generate_crash_report(signal);
        
        // Dump thread-local circular buffers with enhanced stack trace
#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
        try {
            std::cout << "\n=== DUMPING THREAD-LOCAL CIRCULAR BUFFERS ===" << std::endl;
            std::string formatted_stack_trace = format_stack_trace_for_file(report);
            rpc::thread_local_dump_on_assert_with_stacktrace("CRASH SIGNAL: " + signal_to_string(signal), __FILE__, __LINE__, formatted_stack_trace);
            std::cout << "=== END THREAD-LOCAL BUFFER DUMP ===" << std::endl;
        } catch (...) {
            std::cerr << "[crash_handler] Exception while dumping thread-local buffers" << std::endl;
        }
#endif
        
        // Print the crash report
        print_crash_report(report);

        // Call custom analysis callback if set
        if (analysis_callback_) {
            try {
                analysis_callback_(report);
            } catch (...) {
                std::cerr << "[crash_handler] Exception in analysis callback" << std::endl;
            }
        }

        // Save crash dump if requested
        if (config_.save_crash_dump) {
            save_crash_dump(report);
        }

        // Restore original handler and re-raise signal
        switch (signal) {
            case SIGSEGV: sigaction(SIGSEGV, &old_sigsegv_handler_, nullptr); break;
            case SIGABRT: sigaction(SIGABRT, &old_sigabrt_handler_, nullptr); break;
            case SIGFPE:  sigaction(SIGFPE, &old_sigfpe_handler_, nullptr); break;
            case SIGILL:  sigaction(SIGILL, &old_sigill_handler_, nullptr); break;
            case SIGTERM: sigaction(SIGTERM, &old_sigterm_handler_, nullptr); break;
        }
        
        raise(signal);
    }

    crash_handler::crash_report crash_handler::generate_crash_report(int signal)
    {
        crash_report report;
        report.signal_number = signal;
        report.signal_name = signal_to_string(signal);
        report.crash_time = std::chrono::system_clock::now();
        report.crashed_thread_id = getpid(); // For single-threaded, this is the main thread

        // Collect stack traces from all threads
        if (config_.enable_multithreaded_traces) {
            report.all_threads = collect_all_thread_stacks(config_.max_threads, config_.max_stack_frames);
        } else {
            // Just collect current thread stack
            thread_info current_thread;
            current_thread.thread_id = getpid();
            current_thread.thread_name = "main";
            current_thread.stack_frames = collect_stack_trace(config_.max_stack_frames);
            if (config_.enable_symbol_resolution) {
                current_thread.symbols = resolve_symbols(current_thread.stack_frames);
            }
            report.all_threads.push_back(current_thread);
        }

        // Detect crash patterns
        if (config_.enable_pattern_detection) {
            report.detected_patterns = detect_crash_patterns(report);
        }

        // Collect threading debug info
        if (config_.enable_threading_debug_info) {
            report.threading_debug_info = collect_threading_debug_info();
        }

        return report;
    }

    void crash_handler::print_crash_report(const crash_report& report)
    {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "CRASH DETECTED! Signal: " << report.signal_number;
        if (!report.signal_name.empty()) {
            std::cout << " (" << report.signal_name << ")";
        }
        std::cout << std::endl;

        // Print current test information if available
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if (test_info != nullptr) {
            std::cout << "Current Test: " << test_info->test_suite_name() 
                      << "." << test_info->name() << std::endl;
        }

        // Print thread-local logging information if enabled
#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
        std::cout << "Thread-Local Logs: Check /tmp/rpc_debug_dumps/ for per-thread message history" << std::endl;
#endif

        // Print threading debug info
        if (!report.threading_debug_info.empty()) {
            std::cout << "Threading Debug Info: " << report.threading_debug_info << std::endl;
        }

        // Print detected patterns
        if (!report.detected_patterns.empty()) {
            std::cout << "\nDetected Crash Patterns:" << std::endl;
            for (const auto& pattern : report.detected_patterns) {
                std::cout << "  >> " << pattern << std::endl;
            }
        }

        // Print all thread stacks
        std::cout << "\n=== THREAD STACK TRACES (" << report.all_threads.size() << " threads) ===" << std::endl;
        
        for (size_t i = 0; i < report.all_threads.size(); ++i) {
            const auto& thread = report.all_threads[i];
            std::cout << "\nThread " << (i+1) << "/" << report.all_threads.size() 
                      << " - PID: " << thread.thread_id;
            
            if (!thread.thread_name.empty()) {
                std::cout << " (" << thread.thread_name << ")";
            }
            if (!thread.state.empty()) {
                std::cout << " [" << thread.state << "]";
            }
            std::cout << std::endl;

            // Print stack frames
            const auto& frames = thread.symbols.empty() ? 
                std::vector<std::string>(thread.stack_frames.size(), "<??>") : thread.symbols;
            
            for (size_t j = 0; j < thread.stack_frames.size() && j < frames.size(); ++j) {
                std::cout << "  " << std::setw(2) << j << ": " 
                          << format_address(thread.stack_frames[j]) << " " 
                          << frames[j] << std::endl;
            }
            
            if (thread.stack_frames.empty()) {
                std::cout << "  (no stack trace available)" << std::endl;
            }
        }

        std::cout << std::string(80, '=') << std::endl;
    }

    std::vector<void*> crash_handler::collect_stack_trace(int max_frames)
    {
        std::vector<void*> frames(max_frames);
        int count = backtrace(frames.data(), max_frames);
        frames.resize(count);
        return frames;
    }

    std::vector<crash_handler::thread_info> crash_handler::collect_all_thread_stacks(int max_threads, int max_frames)
    {
        std::vector<thread_info> thread_infos;
        auto thread_ids = enumerate_threads();

        for (size_t i = 0; i < thread_ids.size() && i < static_cast<size_t>(max_threads); ++i) {
            thread_info info;
            info.thread_id = thread_ids[i];
            info.thread_name = get_thread_name(thread_ids[i]);
            info.state = get_thread_state(thread_ids[i]);

            // For now, we can only get the stack trace of the current thread
            // In a real implementation, you'd need to pause other threads and read their stacks
            if (thread_ids[i] == getpid()) {
                info.stack_frames = collect_stack_trace(max_frames);
                if (config_.enable_symbol_resolution) {
                    info.symbols = resolve_symbols(info.stack_frames);
                }
            } else {
                // For other threads, we'd need more advanced techniques
                // This is a simplified implementation
                info.stack_frames = {}; // Empty for non-current threads
                info.symbols = {"[Thread stack trace not available - would require ptrace or similar]"};
            }

            thread_infos.push_back(info);
        }

        return thread_infos;
    }

    std::vector<pid_t> crash_handler::enumerate_threads()
    {
        std::vector<pid_t> thread_ids;
        pid_t pid = getpid();
        
        // Read /proc/[pid]/task/ to enumerate threads
        std::string task_dir = "/proc/" + std::to_string(pid) + "/task";
        DIR* dir = opendir(task_dir.c_str());
        
        if (dir != nullptr) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_DIR && isdigit(entry->d_name[0])) {
                    pid_t tid = static_cast<pid_t>(std::stoi(entry->d_name));
                    thread_ids.push_back(tid);
                }
            }
            closedir(dir);
        } else {
            // Fallback: just return current process ID
            thread_ids.push_back(pid);
        }
        
        return thread_ids;
    }

    std::string crash_handler::get_thread_name(pid_t thread_id)
    {
        std::string comm_path = "/proc/" + std::to_string(thread_id) + "/comm";
        std::ifstream file(comm_path);
        std::string name;
        
        if (file.is_open()) {
            std::getline(file, name);
            // Remove trailing newline if present
            if (!name.empty() && name.back() == '\n') {
                name.pop_back();
            }
        } else {
            name = "unknown";
        }
        
        return name;
    }

    std::string crash_handler::get_thread_state(pid_t thread_id)
    {
        std::string stat_path = "/proc/" + std::to_string(thread_id) + "/stat";
        std::ifstream file(stat_path);
        std::string line;
        
        if (file.is_open() && std::getline(file, line)) {
            // Parse the stat line to get the state (3rd field after PID and comm)
            std::istringstream iss(line);
            std::string pid, comm, state;
            iss >> pid >> comm >> state;
            return state;
        }
        
        return "?";
    }

    std::vector<std::string> crash_handler::resolve_symbols(const std::vector<void*>& addresses)
    {
        std::vector<std::string> symbols;
        
        // Try backtrace_symbols first (fast but limited)
        char** symbol_strings = backtrace_symbols(addresses.data(), addresses.size());
        
        for (size_t i = 0; i < addresses.size(); ++i) {
            std::string symbol;
            std::string addr2line_result;
            
            // Get enhanced symbol info with addr2line if enabled
            if (config_.enable_symbol_resolution) {
                addr2line_result = resolve_symbol_with_addr2_line(addresses[i]);
            }
            
            if (!addr2line_result.empty()) {
                // Use the enhanced addr2line result as primary
                symbol = format_address(addresses[i]) + " " + addr2line_result;
            } else if (symbol_strings && symbol_strings[i]) {
                // Fall back to backtrace_symbols
                std::string backtrace_symbol = symbol_strings[i];
                
                // Try to extract and clean up the backtrace symbol
                size_t start_paren = backtrace_symbol.find('(');
                size_t plus_pos = backtrace_symbol.find('+');
                size_t end_paren = backtrace_symbol.find(')', start_paren);
                
                if (start_paren != std::string::npos && plus_pos != std::string::npos && 
                    end_paren != std::string::npos && start_paren < plus_pos && plus_pos < end_paren) {
                    // Extract the function name between ( and +
                    std::string func_name = backtrace_symbol.substr(start_paren + 1, plus_pos - start_paren - 1);
                    
                    if (!func_name.empty() && func_name != "_start") {
                        // Try to demangle C++ names
                        int status = 0;
                        char* demangled = abi::__cxa_demangle(func_name.c_str(), nullptr, nullptr, &status);
                        if (status == 0 && demangled) {
                            symbol = format_address(addresses[i]) + " " + std::string(demangled);
                            free(demangled);
                        } else {
                            symbol = format_address(addresses[i]) + " " + func_name;
                        }
                    } else {
                        symbol = backtrace_symbol;
                    }
                } else {
                    symbol = backtrace_symbol;
                }
            } else {
                symbol = format_address(addresses[i]) + " <?>";
            }
            
            symbols.push_back(symbol);
        }
        
        if (symbol_strings) {
            free(symbol_strings);
        }
        
        return symbols;
    }

    std::string crash_handler::resolve_symbol_with_addr2_line(void* address)
    {
        // Get the executable path
        char exe_path[1024];
        ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (len == -1) {
            return "";
        }
        exe_path[len] = '\0';
        
        // Get the base address of the executable to calculate relative address
        // For PIE executables, we need to subtract the base address
        Dl_info info;
        if (dladdr(address, &info) == 0 || info.dli_fbase == nullptr) {
            return "";
        }
        
        // Calculate relative address
        uintptr_t relative_addr = (uintptr_t)address - (uintptr_t)info.dli_fbase;
        
        // Use addr2line to get function name, source file and line number
        std::stringstream cmd;
        cmd << "addr2line -f -C -e " << exe_path << " 0x" << std::hex << relative_addr << " 2>/dev/null";
        
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) return "";
        
        char function_buffer[512];
        char location_buffer[512];
        std::string result;
        
        // First line: function name
        if (fgets(function_buffer, sizeof(function_buffer), pipe) != nullptr) {
            std::string function = function_buffer;
            if (!function.empty() && function.back() == '\n') {
                function.pop_back();
            }
            
            // Second line: file:line
            if (fgets(location_buffer, sizeof(location_buffer), pipe) != nullptr) {
                std::string location = location_buffer;
                if (!location.empty() && location.back() == '\n') {
                    location.pop_back();
                }
                
                // Skip if we get default "??" responses
                if (function != "??" && location != "??:0" && location != "??:?") {
                    // Extract just the filename from full path
                    size_t last_slash = location.find_last_of('/');
                    if (last_slash != std::string::npos && last_slash + 1 < location.length()) {
                        location = location.substr(last_slash + 1);
                    }
                    
                    result = function + " at " + location;
                }
            }
        }
        
        pclose(pipe);
        return result;
    }

    std::vector<std::string> crash_handler::detect_crash_patterns(const crash_report& report)
    {
        std::vector<std::string> patterns;
        
        // Analyze stack traces for known patterns
        for (const auto& thread : report.all_threads) {
            for (const auto& symbol : thread.symbols) {
                // Pattern detection from original test_host.cpp
                if (symbol.find("on_object_proxy_released") != std::string::npos) {
                    patterns.push_back("PROXY CLEANUP CRASH DETECTED in thread " + std::to_string(thread.thread_id));
                }
                if (symbol.find("unable to find object") != std::string::npos) {
                    patterns.push_back("OBJECT NOT FOUND CRASH DETECTED in thread " + std::to_string(thread.thread_id));
                }
                
                // Additional patterns for threading issues
                if (symbol.find("remove_zone_proxy") != std::string::npos) {
                    patterns.push_back("ZONE PROXY REMOVAL CRASH DETECTED - possible threading bug");
                }
                if (symbol.find("threading_debug") != std::string::npos) {
                    patterns.push_back("THREADING DEBUG ASSERTION - race condition detected");
                }
                if (symbol.find("mutex") != std::string::npos || symbol.find("lock") != std::string::npos) {
                    patterns.push_back("SYNCHRONIZATION CRASH DETECTED - mutex/lock related");
                }
            }
        }
        
        return patterns;
    }

    std::string crash_handler::collect_threading_debug_info()
    {
        std::stringstream info;
        
#ifdef RPC_THREADING_DEBUG
        // If threading debug is enabled, we could add more specific info here
        info << " | Threading debug: ENABLED";
        // Note: We'd need to extend the threading_debug API to get activity counts
        // For now, just indicate it's enabled
#else
        info << " | Threading debug: DISABLED";
#endif

        return info.str();
    }

    std::string crash_handler::signal_to_string(int signal)
    {
        switch (signal) {
            case SIGSEGV: return "SIGSEGV (Segmentation fault)";
            case SIGABRT: return "SIGABRT (Abort)";
            case SIGFPE:  return "SIGFPE (Floating point exception)";
            case SIGILL:  return "SIGILL (Illegal instruction)";
            case SIGTERM: return "SIGTERM (Termination)";
            case SIGINT:  return "SIGINT (Interrupt)";
            case SIGKILL: return "SIGKILL (Kill)";
            case SIGSTOP: return "SIGSTOP (Stop)";
            default: return "Unknown signal";
        }
    }

    std::string crash_handler::format_address(void* address)
    {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << 
              reinterpret_cast<uintptr_t>(address);
        return ss.str();
    }

    std::string crash_handler::format_stack_trace_for_file(const crash_report& report)
    {
        std::stringstream ss;
        
        // Print current test information if available
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if (test_info != nullptr) {
            ss << "Current Test: " << test_info->test_suite_name() 
               << "." << test_info->name() << "\n";
        }
        
        ss << "Signal: " << report.signal_number;
        if (!report.signal_name.empty()) {
            ss << " (" << report.signal_name << ")";
        }
        ss << "\n";
        
        ss << "Thread Count: " << report.all_threads.size() << "\n\n";
        
        // Add all thread stack traces
        for (size_t i = 0; i < report.all_threads.size(); ++i) {
            const auto& thread = report.all_threads[i];
            ss << "Thread " << (i+1) << "/" << report.all_threads.size() 
               << " - PID: " << thread.thread_id;
            
            if (!thread.thread_name.empty()) {
                ss << " (" << thread.thread_name << ")";
            }
            if (!thread.state.empty()) {
                ss << " [" << thread.state << "]";
            }
            ss << "\n";

            // Print stack frames
            const auto& frames = thread.symbols.empty() ? 
                std::vector<std::string>(thread.stack_frames.size(), "<??>") : thread.symbols;
            
            for (size_t j = 0; j < thread.stack_frames.size() && j < frames.size(); ++j) {
                ss << "  " << std::setw(2) << j << ": " 
                   << format_address(thread.stack_frames[j]) << " " 
                   << frames[j] << "\n";
            }
            
            if (thread.stack_frames.empty()) {
                ss << "  (no stack trace available)\n";
            }
            
            ss << "\n";
        }
        
        // Add pattern detection results
        if (!report.detected_patterns.empty()) {
            ss << "=== DETECTED CRASH PATTERNS ===\n";
            for (const auto& pattern : report.detected_patterns) {
                ss << "  >> " << pattern << "\n";
            }
            ss << "\n";
        }
        
        return ss.str();
    }

    void crash_handler::save_crash_dump(const crash_report& report)
    {
        auto time_t = std::chrono::system_clock::to_time_t(report.crash_time);
        std::stringstream filename;
        filename << config_.crash_dump_path << "/crash_" << getpid() << "_" 
                 << time_t << ".txt";
        
        std::ofstream file(filename.str());
        if (file.is_open()) {
            file << "CRASH HANDLER DIAGNOSTIC REPORT\n";
            file << "================================\n\n";
            
            // Print current test information if available
            const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
            if (test_info != nullptr) {
                file << "Current Test: " << test_info->test_suite_name() 
                     << "." << test_info->name() << "\n";
            }

#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
            file << "Thread-Local Logs: Check /tmp/rpc_debug_dumps/ for per-thread message history\n";
#endif
            
            file << "Signal: " << report.signal_number;
            if (!report.signal_name.empty()) {
                file << " (" << report.signal_name << ")";
            }
            file << "\n";
            
            // Print threading debug info
            if (!report.threading_debug_info.empty()) {
                file << "Threading Debug Info: " << report.threading_debug_info << "\n";
            }

            // Print detected patterns
            if (!report.detected_patterns.empty()) {
                file << "\nDetected Crash Patterns:\n";
                for (const auto& pattern : report.detected_patterns) {
                    file << "  >> " << pattern << "\n";
                }
            }

            file << "\n=== THREAD STACK TRACES (" << report.all_threads.size() << " threads) ===\n\n";
            
            for (size_t i = 0; i < report.all_threads.size(); ++i) {
                const auto& thread = report.all_threads[i];
                file << "Thread " << (i+1) << "/" << report.all_threads.size() 
                     << " - PID: " << thread.thread_id;
                
                if (!thread.thread_name.empty()) {
                    file << " (" << thread.thread_name << ")";
                }
                if (!thread.state.empty()) {
                    file << " [" << thread.state << "]";
                }
                file << "\n";

                // Print stack frames with enhanced symbols
                const auto& frames = thread.symbols.empty() ? 
                    std::vector<std::string>(thread.stack_frames.size(), "<??>") : thread.symbols;
                
                for (size_t j = 0; j < thread.stack_frames.size() && j < frames.size(); ++j) {
                    file << "  " << std::setw(2) << j << ": " 
                         << format_address(thread.stack_frames[j]) << " " 
                         << frames[j] << "\n";
                }
                
                if (thread.stack_frames.empty()) {
                    file << "  (no stack trace available)\n";
                }
                
                file << "\n";
            }
            
            file << "=== TELEMETRY INFORMATION ===\n";
            file << "Note: If telemetry is enabled, additional logs may be available in:\n";
            file << "- Console output (if USE_CONSOLE_TELEMETRY=ON)\n";
            file << "- Telemetry service logs (if USE_RPC_TELEMETRY=ON)\n";
            file << "- Check application logs for telemetry topology diagrams\n\n";
            
            file << "=== END REPORT ===\n";
            file.close();
            
            std::cout << "[crash_handler] Enhanced crash dump saved to: " << filename.str() << std::endl;
        }
    }

} // namespace crash_handler

#endif // _WIN32