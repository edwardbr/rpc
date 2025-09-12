/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "rpc_global_logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <thread>
#include <chrono>

void rpc_global_logger::init_logger()
{
/*    std::lock_guard<std::mutex> lock(logger_mutex_);
    if (!logger_created_)
    {
        try
        {
            // Use unique logger name to avoid conflicts
            std::string logger_name = "rpc_global_logger";
            
            // Try to get existing logger first
            logger_ = spdlog::get(logger_name);
            if (!logger_) {
                // Create async logger using spdlog's async factory with file output
                logger_ = spdlog::basic_logger_mt<spdlog::async_factory>(logger_name, "./conversation.txt", true);
            }

            // Use simple pattern 
            // logger_->set_pattern("[%^%l%$] %v");
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
            logger_->set_level(spdlog::level::trace);
            logger_created_ = true;
        }
        catch (...)
        {
            try {
                // Fallback to synchronous logging if async fails
                std::string fallback_name = "rpc_global_logger_sync";
                logger_ = spdlog::get(fallback_name);
                if (!logger_) {
                    logger_ = spdlog::basic_logger_mt(fallback_name, "./conversation.txt", true);
                }
                logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
                logger_created_ = true;
            } catch (...) {
                // Last resort - use default logger
                logger_ = spdlog::default_logger();
                logger_created_ = true;
            }
        }
    }*/
}

std::shared_ptr<spdlog::logger> rpc_global_logger::get_logger()
{
    if (!logger_created_)
    {
        init_logger();
    }
    return logger_;
}

void rpc_global_logger::reset_logger()
{
/*    std::lock_guard<std::mutex> lock(logger_mutex_);
    if (logger_created_ && logger_) {
        // Flush any pending async messages
        logger_->flush();
        
        // Get reference to the thread pool before dropping logger
        auto tp = spdlog::thread_pool();
        
        // Drop the logger reference
        logger_.reset();
        
        // Drop all loggers from spdlog registry
        spdlog::drop_all();
        
        // Wait for thread pool to finish processing if it exists and has work
        if (tp) {
            constexpr int max_wait_ms = 100;
            constexpr int check_interval_ms = 1;
            
            for (int waited = 0; waited < max_wait_ms; waited += check_interval_ms) {
                if (tp->queue_size() == 0) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
            }
            
            // Small additional delay to ensure worker thread processes the empty queue check
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        logger_created_ = false;
    }*/
}

void rpc_global_logger::debug(const std::string& message)
{
    printf("[DEBUG] %s\n", message.c_str());
}

void rpc_global_logger::trace(const std::string& message)
{
    printf("[TRACE] %s\n", message.c_str());
}

void rpc_global_logger::info(const std::string& message)
{
    printf("[INFO] %s\n", message.c_str());
}

void rpc_global_logger::warn(const std::string& message)
{
    printf("[WARN] %s\n", message.c_str());
}

void rpc_global_logger::error(const std::string& message)
{
    printf("[ERROR] %s\n", message.c_str());
}

void rpc_global_logger::critical(const std::string& message)
{
    printf("[CRITICAL] %s\n", message.c_str());
}