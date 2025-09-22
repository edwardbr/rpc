/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <memory>
#include <string>
#include <mutex>

namespace spdlog
{
    class logger;
}

class rpc_global_logger
{
private:
    inline static std::shared_ptr<spdlog::logger> logger_;
    inline static std::mutex logger_mutex_;
    inline static bool logger_created_ = false;

    static void init_logger();

public:
    static std::shared_ptr<spdlog::logger> get_logger();
    static void reset_logger();

    // Convenience logging methods
    static void debug(const std::string& message);
    static void trace(const std::string& message);
    static void info(const std::string& message);
    static void warn(const std::string& message);
    static void error(const std::string& message);
    static void critical(const std::string& message);
};
