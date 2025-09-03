/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "crash_handler.h"
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

// Threading debug integration
#ifdef RPC_THREADING_DEBUG
#include <rpc/threading_debug.h>
#endif

namespace crash_handler
{
    // Static member definitions
    CrashHandler* CrashHandler::instance_ = nullptr;
    CrashHandler::Config CrashHandler::config_;
    CrashHandler::CrashAnalysisCallback CrashHandler::analysis_callback_;
    
    struct sigaction CrashHandler::old_sigsegv_handler_;
    struct sigaction CrashHandler::old_sigabrt_handler_;
    struct sigaction CrashHandler::old_sigfpe_handler_;
    struct sigaction CrashHandler::old_sigill_handler_;
    struct sigaction CrashHandler::old_sigterm_handler_;

    bool CrashHandler::Initialize()
    {
        Config default_config;
        return Initialize(default_config);
    }

    bool CrashHandler::Initialize(const Config& config)
    {
        if (instance_ != nullptr) {
            return false; // Already initialized
        }

        config_ = config;
        instance_ = new CrashHandler();

        // Install signal handlers
        struct sigaction sa;
        sa.sa_sigaction = HandleCrash;
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

        std::cout << "[CrashHandler] Initialized with multi-threaded stack trace support" << std::endl;
        return true;
    }

    void CrashHandler::Shutdown()
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

        std::cout << "[CrashHandler] Shutdown complete" << std::endl;
    }

    void CrashHandler::SetAnalysisCallback(CrashAnalysisCallback callback)
    {
        analysis_callback_ = callback;
    }

    void CrashHandler::HandleCrash(int signal, siginfo_t* info, void* context)
    {
        // Generate comprehensive crash report
        auto report = GenerateCrashReport(signal);
        
        // Print the crash report
        PrintCrashReport(report);

        // Call custom analysis callback if set
        if (analysis_callback_) {
            try {
                analysis_callback_(report);
            } catch (...) {
                std::cerr << "[CrashHandler] Exception in analysis callback" << std::endl;
            }
        }

        // Save crash dump if requested
        if (config_.save_crash_dump) {
            SaveCrashDump(report);
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

    CrashHandler::CrashReport CrashHandler::GenerateCrashReport(int signal)
    {
        CrashReport report;
        report.signal_number = signal;
        report.signal_name = SignalToString(signal);
        report.crash_time = std::chrono::system_clock::now();
        report.crashed_thread_id = getpid(); // For single-threaded, this is the main thread

        // Collect stack traces from all threads
        if (config_.enable_multithreaded_traces) {
            report.all_threads = CollectAllThreadStacks(config_.max_threads, config_.max_stack_frames);
        } else {
            // Just collect current thread stack
            ThreadInfo current_thread;
            current_thread.thread_id = getpid();
            current_thread.thread_name = "main";
            current_thread.stack_frames = CollectStackTrace(config_.max_stack_frames);
            if (config_.enable_symbol_resolution) {
                current_thread.symbols = ResolveSymbols(current_thread.stack_frames);
            }
            report.all_threads.push_back(current_thread);
        }

        // Detect crash patterns
        if (config_.enable_pattern_detection) {
            report.detected_patterns = DetectCrashPatterns(report);
        }

        // Collect threading debug info
        if (config_.enable_threading_debug_info) {
            report.threading_debug_info = CollectThreadingDebugInfo();
        }

        return report;
    }

    void CrashHandler::PrintCrashReport(const CrashReport& report)
    {
        /*std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "CRASH DETECTED! Signal: " << report.signal_number;
        if (!report.signal_name.empty()) {
            std::cout << " (" << report.signal_name << ")";
        }
        std::cout << std::endl;

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
                          << FormatAddress(thread.stack_frames[j]) << " " 
                          << frames[j] << std::endl;
            }
            
            if (thread.stack_frames.empty()) {
                std::cout << "  (no stack trace available)" << std::endl;
            }
        }

        std::cout << std::string(80, '=') << std::endl;*/
    }

    std::vector<void*> CrashHandler::CollectStackTrace(int max_frames)
    {
        std::vector<void*> frames(max_frames);
        int count = backtrace(frames.data(), max_frames);
        frames.resize(count);
        return frames;
    }

    std::vector<CrashHandler::ThreadInfo> CrashHandler::CollectAllThreadStacks(int max_threads, int max_frames)
    {
        std::vector<ThreadInfo> thread_infos;
        auto thread_ids = EnumerateThreads();

        for (size_t i = 0; i < thread_ids.size() && i < static_cast<size_t>(max_threads); ++i) {
            ThreadInfo info;
            info.thread_id = thread_ids[i];
            info.thread_name = GetThreadName(thread_ids[i]);
            info.state = GetThreadState(thread_ids[i]);

            // For now, we can only get the stack trace of the current thread
            // In a real implementation, you'd need to pause other threads and read their stacks
            if (thread_ids[i] == getpid()) {
                info.stack_frames = CollectStackTrace(max_frames);
                if (config_.enable_symbol_resolution) {
                    info.symbols = ResolveSymbols(info.stack_frames);
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

    std::vector<pid_t> CrashHandler::EnumerateThreads()
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

    std::string CrashHandler::GetThreadName(pid_t thread_id)
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

    std::string CrashHandler::GetThreadState(pid_t thread_id)
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

    std::vector<std::string> CrashHandler::ResolveSymbols(const std::vector<void*>& addresses)
    {
        std::vector<std::string> symbols;
        
        // Try backtrace_symbols first (fast but limited)
        char** symbol_strings = backtrace_symbols(addresses.data(), addresses.size());
        
        for (size_t i = 0; i < addresses.size(); ++i) {
            std::string symbol;
            
            if (symbol_strings && symbol_strings[i]) {
                symbol = symbol_strings[i];
                
                // Try to enhance with addr2line if available
                if (config_.enable_symbol_resolution) {
                    std::string addr2line_result = ResolveSymbolWithAddr2Line(addresses[i]);
                    if (!addr2line_result.empty() && addr2line_result != "??:0") {
                        symbol += " [" + addr2line_result + "]";
                    }
                }
            } else {
                symbol = FormatAddress(addresses[i]) + " <?>";
            }
            
            symbols.push_back(symbol);
        }
        
        if (symbol_strings) {
            free(symbol_strings);
        }
        
        return symbols;
    }

    std::string CrashHandler::ResolveSymbolWithAddr2Line(void* address)
    {
        // Use addr2line to get source file and line number
        std::stringstream cmd;
        cmd << "addr2line -e /proc/" << getpid() << "/exe " << address << " 2>/dev/null";
        
        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) return "";
        
        char buffer[256];
        std::string result;
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result = buffer;
            // Remove trailing newline
            if (!result.empty() && result.back() == '\n') {
                result.pop_back();
            }
        }
        
        pclose(pipe);
        return result;
    }

    std::vector<std::string> CrashHandler::DetectCrashPatterns(const CrashReport& report)
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

    std::string CrashHandler::CollectThreadingDebugInfo()
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

    std::string CrashHandler::SignalToString(int signal)
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

    std::string CrashHandler::FormatAddress(void* address)
    {
        std::stringstream ss;
        ss << "0x" << std::hex << std::setfill('0') << std::setw(16) << 
              reinterpret_cast<uintptr_t>(address);
        return ss.str();
    }

    void CrashHandler::SaveCrashDump(const CrashReport& report)
    {
        auto time_t = std::chrono::system_clock::to_time_t(report.crash_time);
        std::stringstream filename;
        filename << config_.crash_dump_path << "/crash_" << getpid() << "_" 
                 << time_t << ".txt";
        
        std::ofstream file(filename.str());
        if (file.is_open()) {
            // Redirect cout to file temporarily
            std::streambuf* orig = std::cout.rdbuf();
            std::cout.rdbuf(file.rdbuf());
            
            PrintCrashReport(report);
            
            std::cout.rdbuf(orig);
            std::cout << "[CrashHandler] Crash dump saved to: " << filename.str() << std::endl;
        }
    }

} // namespace crash_handler