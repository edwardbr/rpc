#include <rpc/version.h>

namespace rpc
{
    //128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
    std::uint8_t get_version() 
    {
#ifndef NO_RPC_V2
        return VERSION_2;
#elif !defined(NO_RPC_V1)
        return VERSION_1;
#else
        asset(false);
#endif
    }
}