#include "marshaller/proxy.h"

namespace rpc
{
    object_proxy::~object_proxy() { marshaller_->release(zone_id_, object_id_); }

    error_code object_proxy::send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                                  std::vector<char>& out_buf_)
    {
        return marshaller_->send(object_id_, interface_id, method_id, in_size_, in_buf_, out_buf_);
    }

    void object_proxy::register_interface(uint64_t interface_id, rpc::weak_ptr<proxy_base>& value)
    {
        std::lock_guard guard(insert_control);
        auto item = proxy_map.find(interface_id);
        if (item != proxy_map.end())
        {
            auto tmp = item->second.lock();
            if (tmp == nullptr)
            {
                item->second = value;
                return;
            }
            value = item->second;
            return;
        }
        proxy_map[interface_id] = value;
    }

    error_code object_proxy::try_cast(uint64_t interface_id)
    {
        return marshaller_->try_cast(zone_id_, object_id_, interface_id);
    }

    error_code service_proxy::set_root_object(uint64_t object_id)
    {
        if (root_object_proxy_)
            return -1;
        root_object_proxy_ = object_proxy::create(object_id, zone_id_, shared_from_this());
        return 0;
    }
}