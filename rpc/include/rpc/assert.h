#pragma once

#include <assert.h>
#include <rpc/error_codes.h>

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
#define RPC_ASSERT(x) assert(x)
#endif

#define ASSERT_ERROR_CODE(x) RPC_ASSERT(x == rpc::error::OK())