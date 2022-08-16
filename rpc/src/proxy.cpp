#include "rpc/proxy.h"

namespace rpc
{
    object_proxy::object_proxy(uint64_t object_id, uint64_t zone_id, rpc::shared_ptr<service_proxy> service_proxy)
        : object_id_(object_id)
        , zone_id_(zone_id)
        , service_proxy_(service_proxy)
    {
        if(auto* telemetry_service = service_proxy_->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_creation(service_proxy_->get_operating_zone_id(), zone_id, object_id);
        }
    }
    object_proxy::~object_proxy() 
    { 
        if(auto* telemetry_service = service_proxy_->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_deletion(service_proxy_->get_operating_zone_id(), zone_id_, object_id_);
        }
        service_proxy_->release(zone_id_, object_id_); 
        service_proxy_ = nullptr;
    }

    int object_proxy::send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                                  std::vector<char>& out_buf_)
    {
        return service_proxy_->send(zone_id_, object_id_, interface_id, method_id, in_size_, in_buf_, out_buf_);
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

    int object_proxy::try_cast(uint64_t interface_id)
    {
        return service_proxy_->try_cast(zone_id_, object_id_, interface_id);
    }
}