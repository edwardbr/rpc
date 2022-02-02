#include "marshaller/stub.h"

object_stub::object_stub(std::shared_ptr<i_interface_marshaller> initial_interface)
{
    proxy_map[initial_interface->get_interface_id()] = initial_interface;
}
error_code object_stub::call(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                             size_t out_size_, char* out_buf_)
{
    error_code ret = -1;
    std::lock_guard l(insert_control);
    auto item = proxy_map.find(interface_id);
    if (item != proxy_map.end())
    {
        ret = item->second->call(method_id, in_size_, in_buf_, out_size_, out_buf_);
    }
    return ret;
}
error_code object_stub::try_cast(uint64_t interface_id)
{
    error_code ret = 0;
    std::lock_guard l(insert_control);
    auto item = proxy_map.find(interface_id);
    if (item == proxy_map.end())
    {
        std::shared_ptr<i_interface_marshaller> new_stub;
        ret = proxy_map.begin()->second->cast(interface_id, new_stub);
        if (!ret)
        {
            proxy_map.emplace(interface_id, std::move(new_stub));
        }
    }
    return ret;
}