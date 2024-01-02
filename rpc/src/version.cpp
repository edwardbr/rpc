/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/version.h>

namespace rpc
{
    // 128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
    // /!\ actually switched to uint64_t for consistency with other parts of the code that were using that
    std::uint64_t get_version()
    {
#ifdef RPC_V2
        return VERSION_2;
#elif defined(RPC_V1)
        return VERSION_1;
#else
        assert(false);
#endif
    }
}
