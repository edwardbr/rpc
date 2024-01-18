#include "rpc/proxy.h"
#include "rpc/logger.h"

namespace rpc
{
    object_proxy::object_proxy( object object_id, 
                                rpc::shared_ptr<service_proxy> service_proxy)
        : object_id_(object_id)
        , service_proxy_(service_proxy)
    {
#ifdef USE_RPC_LOGGING
        auto message = std::string("object_proxy::object_proxy zone_id ") + std::to_string(service_proxy->get_zone_id().get_val())
        + std::string(", object_id ") + std::to_string(object_id.get_val())
        + std::string(", destination_zone_id ") + std::to_string(service_proxy_->get_destination_zone_id().get_val()) 
        + std::string(", cloned from ") + std::to_string(service_proxy->get_destination_channel_zone_id().get_val());
        LOG_STR(message.c_str(), message.size());
#endif
    }

    rpc::shared_ptr<object_proxy> object_proxy::create(object object_id, 
                                            const rpc::shared_ptr<service_proxy>& sp, bool add_ref_done)
    {
        //a way to have private constructors in object_proxy but still available via enable shared from this
        struct make_shared_object_proxy_enabler : public object_proxy 
        {
            make_shared_object_proxy_enabler(object object_id, rpc::shared_ptr<service_proxy> sp) : object_proxy(object_id, sp){}
        };   

        auto ret = rpc::make_shared<make_shared_object_proxy_enabler>(object_id, sp);
        if(auto* telemetry_service = sp->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_creation(sp->get_zone_id(), sp->get_destination_zone_id(), object_id, add_ref_done);
        }        
        sp->add_object_proxy(ret);
        return ret;
    }

    object_proxy::~object_proxy() 
    { 
        if(auto* telemetry_service = service_proxy_->get_telemetry_service();telemetry_service)
        {
            telemetry_service->on_object_proxy_deletion(service_proxy_->get_zone_id(), service_proxy_->get_destination_zone_id(), object_id_);
        }

#ifdef USE_RPC_LOGGING
        auto message = std::string("object_proxy::~object_proxy zone_id ") + std::to_string(service_proxy_->get_zone_id().get_val())
        + std::string(", object_id ") + std::to_string(object_id_.get_val())
        + std::string(", destination_zone_id ") + std::to_string(service_proxy_->get_destination_zone_id().get_val()) 
        + std::string(", cloned from ") + std::to_string(service_proxy_->get_destination_channel_zone_id().get_val());
        LOG_STR(message.c_str(), message.size());
#endif

        service_proxy_->remove_object_proxy(object_id_);
        service_proxy_->sp_release(service_proxy_->get_destination_zone_id(), object_id_, service_proxy_->get_zone_id().as_caller()); 
        service_proxy_ = nullptr;
    }

    int object_proxy::send(uint64_t tag, std::function<interface_ordinal (uint64_t)> id_getter, method method_id, size_t in_size_, const char* in_buf_,
                                  std::vector<char>& out_buf_)
    {
        return service_proxy_->sp_call(
            encoding::enc_default,
            tag,
            object_id_, 
            id_getter, 
            method_id, 
            in_size_, 
            in_buf_, 
            out_buf_);
    }

    int object_proxy::try_cast(std::function<interface_ordinal (uint64_t)> id_getter)
    {
        return service_proxy_->sp_try_cast(service_proxy_->get_destination_zone_id(), object_id_, id_getter);
    }

    destination_zone object_proxy::get_destination_zone_id() const 
    {
        return service_proxy_->get_destination_zone_id();
    }
}
