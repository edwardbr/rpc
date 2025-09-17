/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once
#include <cstdint>

namespace rpc
{
    // 128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
#ifndef NO_RPC_V3
#define RPC_V3
    constexpr std::uint64_t VERSION_3 = 3;
#endif

#ifndef NO_RPC_V2
#define RPC_V2
    constexpr std::uint64_t VERSION_2 = 2;
#endif

    constexpr std::uint64_t LOWEST_SUPPORTED_VERSION =
#ifdef RPC_V2
        VERSION_2
#elif defined(RPC_V3)
        VERSION_3
#else
        0
#endif
        ;

    constexpr std::uint64_t HIGHEST_SUPPORTED_VERSION =
#ifdef RPC_V3
        VERSION_3
#elif defined(RPC_V2)
        VERSION_2
#else
        0
#endif
        ;

    std::uint64_t get_version();
}
