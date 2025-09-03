#pragma once

#include <string>
#include <cstring>

#ifndef RPC_LOGGING_DEFINED
#ifdef USE_RPC_LOGGING

#ifdef _IN_ENCLAVE
#include <fmt/format-inl.h>
#else
#include <fmt/format.h>
#endif

// Logging macros with levels (0=DEBUG, 1=TRACE, 2=INFO, 3=WARNING, 4=ERROR, 5=CRITICAL)
#define RPC_DEBUG(format_str, ...) do { \
    auto formatted = fmt::format(format_str, ##__VA_ARGS__); \
    rpc_log(0, formatted.c_str(), formatted.length()); \
} while(0)

#define RPC_TRACE(format_str, ...) do { \
    auto formatted = fmt::format(format_str, ##__VA_ARGS__); \
    rpc_log(1, formatted.c_str(), formatted.length()); \
} while(0)

#define RPC_INFO(format_str, ...) do { \
    auto formatted = fmt::format(format_str, ##__VA_ARGS__); \
    rpc_log(2, formatted.c_str(), formatted.length()); \
} while(0)

#define RPC_WARNING(format_str, ...) do { \
    auto formatted = fmt::format(format_str, ##__VA_ARGS__); \
    rpc_log(3, formatted.c_str(), formatted.length()); \
} while(0)

#define RPC_ERROR(format_str, ...) do { \
    auto formatted = fmt::format(format_str, ##__VA_ARGS__); \
    rpc_log(4, formatted.c_str(), formatted.length()); \
} while(0)

#define RPC_CRITICAL(format_str, ...) do { \
    auto formatted = fmt::format(format_str, ##__VA_ARGS__); \
    rpc_log(5, formatted.c_str(), formatted.length()); \
} while(0)

#ifdef _IN_ENCLAVE
#include <sgx_error.h>
extern "C"
{
    sgx_status_t rpc_log(int level, const char* str, size_t sz);
}
#else
extern "C"
{
    void rpc_log(int level, const char* str, size_t sz);
}
#endif
#else
// Disabled logging - all macros are no-ops
#define RPC_DEBUG(format_str, ...)
#define RPC_TRACE(format_str, ...)
#define RPC_INFO(format_str, ...)
#define RPC_WARNING(format_str, ...)
#define RPC_ERROR(format_str, ...)
#define RPC_CRITICAL(format_str, ...)
#endif
#define RPC_LOGGING_DEFINED
#endif
