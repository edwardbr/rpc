#pragma once
#include <cstdint>

namespace rpc
{
    //128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
#ifndef NO_RPC_V1
    constexpr std::uint8_t VERSION_1 = 1;
#endif
#ifndef NO_RPC_V2
    constexpr std::uint8_t VERSION_2 = 2;
#endif
    std::uint8_t get_version();
}