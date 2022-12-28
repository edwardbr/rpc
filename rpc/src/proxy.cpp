#include "rpc/proxy.h"

namespace rpc
{
    object_proxy::object_proxy( uint64_t object_id, 
                                uint64_t zone_id, 
                                rpc::shared_ptr<service_proxy> service_proxy,
                                bool stub_needs_add_ref,
                                bool service_proxy_needs_add_ref)
        : object_id_(object_id)
        , zone_id_(zone_id)
        , service_proxy_(service_proxy)
    {
        if(auto* telemetry_service = service_proxy_->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_creation(service_proxy_->get_operating_zone_id(), zone_id, object_id);
        }
        if(stub_needs_add_ref)
            service_proxy_->add_ref(zone_id_, object_id_, false); 
        if(service_proxy_needs_add_ref)
            service_proxy_->add_external_ref();
    }

    rpc::shared_ptr<object_proxy> object_proxy::create(uint64_t object_id, 
                                            uint64_t zone_id,
                                            const rpc::shared_ptr<service_proxy>& service_proxy,
                                            bool stub_needs_add_ref,
                                            bool service_proxy_needs_add_ref)
    {
        rpc::shared_ptr<object_proxy> ret(new object_proxy(object_id, zone_id, service_proxy, stub_needs_add_ref, service_proxy_needs_add_ref));
        ret->weak_this_ = ret;
        service_proxy->add_object_proxy(ret);
        return ret;
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
        return service_proxy_->send(service_proxy_->get_operating_zone_id(), zone_id_, object_id_, interface_id, method_id, in_size_, in_buf_, out_buf_);
    }

    int object_proxy::try_cast(uint64_t interface_id)
    {
        return service_proxy_->try_cast(zone_id_, object_id_, interface_id);
    }
}