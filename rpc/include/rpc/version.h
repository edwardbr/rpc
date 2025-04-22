/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once
#include <cstdint>

namespace rpc
{
    // 128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
#ifndef NO_RPC_V2
#define RPC_V2
    constexpr std::uint64_t VERSION_2 = 2;
#endif
    std::uint64_t get_version();
}
