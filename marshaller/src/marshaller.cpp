#include "marshaller/marshaller.h"

error_code object_proxy::send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                            size_t out_size_, char* out_buf_)
{
    return marshaller_->send(object_id_,  interface_id,  method_id,  in_size_,  in_buf_,  out_size_,  out_buf_);
}


void object_proxy::register_interface(uint64_t interface_id, rpc_cpp::weak_ptr<i_proxy_impl>& value)
{
    std::lock_guard guard(insert_control);
    auto item = proxy_map.find(interface_id);
    if(item != proxy_map.end())
    {
        auto tmp = item->second.lock();
        if(tmp == nullptr)
        {
            item->second = value;
            return;
        }
        value = item->second;
        return;
    }
    proxy_map[interface_id] = value;
}

