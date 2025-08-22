#pragma once

#ifndef LOG_STR_DEFINED
#ifdef USE_RPC_LOGGING
#define LOG_STR(str, sz) rpc_log(str, sz)
#define LOG_CSTR(str) rpc_log(str, strlen(str))
#ifdef _IN_ENCLAVE
#include <sgx_error.h>
extern "C"
{
    sgx_status_t rpc_log(const char* str, size_t sz);
}
#else
extern "C"
{
    void rpc_log(const char* str, size_t sz);
}
#endif
#else
#define LOG_STR(str, sz)
#define LOG_CSTR(str)
#endif
#define LOG_STR_DEFINED
#endif
