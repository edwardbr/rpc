#pragma once

#include <assert.h>
#include <rpc/error_codes.h>
#include <rpc/logger.h>

#ifdef RPC_HANG_ON_FAILED_ASSERT
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


#define RPC_ASSERT(x) if(!(x))(hang())
#else
#ifdef _DEBUG
#define RPC_ASSERT(x) if(!(x))assert(!"error failed " #x);
#else
#define RPC_ASSERT(x) if(!(x)){std::abort();}
#endif
#endif
