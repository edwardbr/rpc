#pragma once

#include <rpc/types.h>
#include <rpc/proxy.h>
#include <rpc/error_codes.h>
#include <rpc/i_telemetry_service.h>

namespace rpc
{
    //this is an equivelent to an enclave looking at its host
    class local_service_proxy : public service_proxy
    {
        rpc::weak_ptr<service> destination_service_;

        local_service_proxy(const rpc::shared_ptr<service>& destination_svc,
                            const rpc::shared_ptr<child_service>& svc,
                            const rpc::i_telemetry_service* telemetry_service)
            : service_proxy(destination_svc->get_zone_id().as_destination(), svc, svc->get_zone_id().as_caller(), telemetry_service),
            destination_service_(destination_svc)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }

        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::make_shared<local_service_proxy>(*this);}

    public:
        local_service_proxy(const local_service_proxy& other) = default;

        virtual ~local_service_proxy()
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_deletion("local_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }

        //if there is no use of a local_service_proxy in the zone requires_parent_release must be set to true so that the zones service can clean things ups
        static rpc::shared_ptr<local_service_proxy> create(const rpc::shared_ptr<service>& destination_svc,
                                                           const rpc::shared_ptr<child_service>& svc,
                                                           const rpc::i_telemetry_service* telemetry_service,
                                                           bool child_does_not_use_parents_interface)
        {
            auto ret = rpc::shared_ptr<local_service_proxy>(new local_service_proxy(destination_svc, svc, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            svc->add_zone_proxy(ret);
            ret->add_external_ref();
            svc->set_parent(pthis, child_does_not_use_parents_interface);
            return ret;
        }

        int send(
            uint64_t protocol_version 
			, encoding encoding 
			, uint64_t tag 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id 
            , method method_id 
            , size_t in_size_
            , const char* in_buf_ 
            , std::vector<char>& out_buf_)
            override
        {
            return destination_service_.lock()->send(protocol_version, encoding, tag, caller_channel_zone_id, caller_zone_id, destination_zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(            
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast("local_service_proxy", get_zone_id(), destination_zone_id,
                                                             object_id, interface_id);
            }
            return destination_service_.lock()->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        uint64_t add_ref(
            uint64_t protocol_version 
            , destination_channel_zone destination_channel_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , add_ref_options build_out_param_channel 
            , bool proxy_add_ref) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref("local_service_proxy", get_zone_id(), destination_zone_id,
                                                            object_id, caller_zone_id);
            }
            auto ret = destination_service_.lock()->add_ref(protocol_version, destination_channel_zone_id, destination_zone_id, object_id, caller_channel_zone_id, caller_zone_id, build_out_param_channel, proxy_add_ref);            
            if(proxy_add_ref && ret != std::numeric_limits<uint64_t>::max())
            {
                add_external_ref();
            }
            return ret;
        }
        uint64_t release(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_zone caller_zone_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release("local_service_proxy", get_zone_id(), destination_zone_id,
                                                            object_id, caller_zone_id);
            }
            auto ret = destination_service_.lock()->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            if(ret != std::numeric_limits<uint64_t>::max())
            {
                release_external_ref();
            }  
            return ret;
        }
    };

    //this is an equivelent to an host looking at its enclave
    class local_child_service_proxy : public service_proxy
    {
        rpc::shared_ptr<service> destination_service_;

        local_child_service_proxy(const rpc::shared_ptr<service>& destination_svc,
                                  const rpc::shared_ptr<service>& svc,
                                  const rpc::i_telemetry_service* telemetry_service)
            : service_proxy(destination_svc->get_zone_id().as_destination(), svc, svc->get_zone_id().as_caller(), telemetry_service),
            destination_service_(destination_svc)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_child_service_proxy", get_zone_id(), get_destination_zone_id(), svc->get_zone_id().as_caller());
            }
        }

        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::make_shared<local_child_service_proxy>(*this);}

    public:
        local_child_service_proxy(const local_child_service_proxy& other) = default;
        virtual ~local_child_service_proxy()
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_deletion("local_child_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }
        static rpc::shared_ptr<local_child_service_proxy> create(const rpc::shared_ptr<service>& destination_svc,
                                                                 const rpc::shared_ptr<service>& svc,
                                                                 const rpc::i_telemetry_service* telemetry_service)
        {
            auto ret = rpc::shared_ptr<local_child_service_proxy>(
                new local_child_service_proxy(destination_svc, svc, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            svc->add_zone_proxy(ret);
            ret->add_external_ref();
            return ret;
        }

        int send(
            uint64_t protocol_version 
			, encoding encoding 
			, uint64_t tag 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id 
            , method method_id 
            , size_t in_size_
            , const char* in_buf_ 
            , std::vector<char>& out_buf_
        ) override
        {
            return destination_service_->send(protocol_version, encoding, tag, caller_channel_zone_id, caller_zone_id, destination_zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id
        ) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast("local_child_service_proxy", get_zone_id(),
                                                             destination_zone_id, object_id, interface_id);
            }
            return destination_service_->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        uint64_t add_ref(
            uint64_t protocol_version 
            , destination_channel_zone destination_channel_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , add_ref_options build_out_param_channel 
            , bool proxy_add_ref
        ) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref("local_child_service_proxy", get_zone_id(),
                                                            destination_zone_id, object_id, caller_zone_id);
            }
            auto ret = destination_service_->add_ref(protocol_version, destination_channel_zone_id, destination_zone_id, object_id, caller_channel_zone_id, caller_zone_id, build_out_param_channel, proxy_add_ref);            
            if(proxy_add_ref && ret != std::numeric_limits<uint64_t>::max())
            {
                add_external_ref();
            }
            return ret;
        }
        uint64_t release(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_zone caller_zone_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release("local_child_service_proxy", get_zone_id(),
                                                            destination_zone_id, object_id, caller_zone_id);
            }
            auto ret = destination_service_->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            if(ret != std::numeric_limits<uint64_t>::max())
            {
                release_external_ref();
            }  
            return ret;            
        }
    };
}