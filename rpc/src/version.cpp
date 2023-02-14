#include <rpc/version.h>

namespace rpc
{
    //128 versions should be enough, if we need more we can use the eighth bit to carry to the next bit
    std::uint8_t get_version() { return 1;}
}