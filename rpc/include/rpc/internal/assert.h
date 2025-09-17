/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <assert.h>
#include <rpc/internal/error_codes.h>
#include <rpc/internal/logger.h>

#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
#include <rpc/internal/thread_local_logger.h>
// Enhanced RPC_ASSERT with thread-local buffer dumping
#define RPC_ASSERT(x)                                                                                                  \
    if (!(x))                                                                                                          \
    {                                                                                                                  \
        rpc::thread_local_dump_on_assert("RPC_ASSERT failed: " #x, __FILE__, __LINE__);                              \
        std::abort();                                                                                                  \
    }
#elif defined(RPC_HANG_ON_FAILED_ASSERT)
#ifdef _IN_ENCLAVE
#include <sgx_error.h>
extern "C"
{
    sgx_status_t hang();
}
#else
extern "C"
{
    void hang();
}
#endif

#define RPC_ASSERT(x)                                                                                                  \
    if (!(x))                                                                                                          \
    (hang())
#else
#ifdef _DEBUG
#define RPC_ASSERT(x)                                                                                                  \
    if (!(x))                                                                                                          \
        assert(!"error failed " #x);
#else
#define RPC_ASSERT(x)                                                                                                  \
    if (!(x))                                                                                                          \
    {                                                                                                                  \
        std::abort();                                                                                                  \
    }
#endif
#endif
