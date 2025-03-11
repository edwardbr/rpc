#include <common/foo_impl.h>

void log(const std::string& data)
{
    std::ignore = data;
#ifdef USE_RPC_LOGGING
    rpc_log(data.data(), data.size());
#endif
}