/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>

namespace rpc
{
    // 128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
    // /!\ actually switched to uint64_t for consistency with other parts of the code that were using that
    std::uint64_t get_version()
    {
#if defined(RPC_V3)
        return VERSION_3;
#elif defined(RPC_V2)
        return VERSION_2;
#else
        static_assert(!sizeof(std::uint64_t), "No RPC protocol versions available");
        return 0;
#endif
    }
}
