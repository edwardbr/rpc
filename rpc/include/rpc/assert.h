#include <assert.h>

#ifdef _IN_ENCLAVE
#include <sgx_error.h>
#endif

#ifdef RPC_HANG_ON_FAILED_ASSERT
extern "C"
{
#ifdef _IN_ENCLAVE
    sgx_status_t hang();
#else
    void hang();
#endif
}

#define RPC_ASSERT(x) if(!(x))(hang())
#else
#define RPC_ASSERT(x) assert(x)
#endif