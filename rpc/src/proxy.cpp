#include "rpc/proxy.h"


#ifndef LOG_STR_DEFINED
# ifdef USE_RPC_LOGGING
#  define LOG_STR(str, sz) log_str(str, sz)
   extern "C"
   {
       void log_str(const char* str, size_t sz);
   }
# else
#  define LOG_STR(str, sz)
# endif
#define LOG_STR_DEFINED
#endif

namespace rpc
{
    object_proxy::object_proxy( object object_id, 
                                rpc::shared_ptr<service_proxy> service_proxy,
                                bool stub_needs_add_ref,
                                bool service_proxy_needs_add_ref)
        : object_id_(object_id)
        , service_proxy_(service_proxy)
    {
        if(auto* telemetry_service = service_proxy_->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_creation(service_proxy_->get_operating_zone_id(), service_proxy_->get_zone_id(), object_id);
        }

        auto message = std::string("object_proxy::object_proxy zone ") + std::to_string(*service_proxy_->get_zone_id()) 
        + std::string(", object_id ") + std::to_string(*object_id)
        + std::string(", operating_zone_id ") + std::to_string(*service_proxy->get_operating_zone_id())
        + std::string(", cloned from ") + std::to_string(*service_proxy->get_cloned_from_zone_id());
        LOG_STR(message.c_str(), message.size());

        if(stub_needs_add_ref)
            service_proxy_->add_ref(service_proxy_->get_zone_id(), object_id_, {*service_proxy_->get_operating_zone_id()}); 
        if(service_proxy_needs_add_ref)
            service_proxy_->add_external_ref();
    }

    rpc::shared_ptr<object_proxy> object_proxy::create(object object_id, 
                                            const rpc::shared_ptr<service_proxy>& service_proxy,
                                            bool stub_needs_add_ref,
                                            bool service_proxy_needs_add_ref)
    {
        rpc::shared_ptr<object_proxy> ret(new object_proxy(object_id, service_proxy, stub_needs_add_ref, service_proxy_needs_add_ref));
        ret->weak_this_ = ret;
        service_proxy->add_object_proxy(ret);
        return ret;
    }

    object_proxy::~object_proxy() 
    { 
        if(auto* telemetry_service = service_proxy_->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_deletion(service_proxy_->get_operating_zone_id(), service_proxy_->get_zone_id(), object_id_);
        }

        auto message = std::string("object_proxy::~object_proxy zone ") + std::to_string(*service_proxy_->get_zone_id()) 
        + std::string(", object_id ") + std::to_string(*object_id_)
        + std::string(", operating_zone_id ") + std::to_string(*service_proxy_->get_operating_zone_id())
        + std::string(", cloned from ") + std::to_string(*service_proxy_->get_cloned_from_zone_id());
        LOG_STR(message.c_str(), message.size());

        service_proxy_->remove_object_proxy(object_id_);
        service_proxy_->release(service_proxy_->get_zone_id(), object_id_, {*service_proxy_->get_operating_zone_id()}); 
        service_proxy_ = nullptr;
    }

    int object_proxy::send(interface_ordinal interface_id, method method_id, size_t in_size_, const char* in_buf_,
                                  std::vector<char>& out_buf_)
    {
        return service_proxy_->send(
            {*service_proxy_->get_operating_zone_id()}, 
            {*service_proxy_->get_operating_zone_id()}, 
            service_proxy_->get_zone_id(), 
            object_id_, 
            interface_id, 
            method_id, 
            in_size_, 
            in_buf_, 
            out_buf_);
    }

    int object_proxy::try_cast(interface_ordinal interface_id)
    {
        return service_proxy_->try_cast(service_proxy_->get_zone_id(), object_id_, interface_id);
    }

    destination_zone object_proxy::get_zone_id() const 
    {
        return service_proxy_->get_zone_id();
    }
}