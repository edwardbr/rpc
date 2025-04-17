#include <common/foo_impl.h>

void log([[maybe_unused]] const std::string& data)
{
#ifdef USE_RPC_LOGGING
    rpc_log(data.data(), data.size());
#endif
}