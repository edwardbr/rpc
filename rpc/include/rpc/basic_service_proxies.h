#pragma once

#include <rpc/types.h>
#include <rpc/proxy.h>
#include <rpc/error_codes.h>
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/service.h>

namespace rpc
{
    struct make_shared_local_service_proxy_enabler;
    //this is an equivelent to an enclave looking at its host
    class local_service_proxy : public service_proxy
    {
        rpc::weak_ptr<service> parent_service_;
        
        friend make_shared_local_service_proxy_enabler;

        local_service_proxy(const rpc::shared_ptr<child_service>& child_svc,
                            const rpc::shared_ptr<service>& parent_svc)
            : service_proxy(parent_svc->get_zone_id().as_destination(), child_svc),
            parent_service_(parent_svc)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }

        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::make_shared<local_service_proxy>(*this);}
        void clone_completed() override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }

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
        
        static rpc::shared_ptr<local_service_proxy> create(destination_zone parent_zone_id, 
                                                            const rpc::shared_ptr<child_service>& child_svc,
                                                            const rpc::shared_ptr<service>& parent_svc)
        {        
            return rpc::shared_ptr<local_service_proxy>(new local_service_proxy(child_svc, parent_svc));
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
            return parent_service_.lock()->send(protocol_version, encoding, tag, caller_channel_zone_id, caller_zone_id, destination_zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(            
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id) override
        {
            return parent_service_.lock()->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        uint64_t add_ref(
            uint64_t protocol_version 
            , destination_channel_zone destination_channel_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , add_ref_options build_out_param_channel) override
        {
            if (get_telemetry_service())
            {
                get_telemetry_service()->on_service_proxy_add_ref("local_service_proxy", get_zone_id(),
                                                                destination_zone_id, destination_channel_zone_id, get_caller_zone_id(), object_id);
            }
            RPC_ASSERT(((std::uint8_t)build_out_param_channel & (std::uint8_t)rpc::add_ref_options::build_caller_route) || destination_channel_zone_id == 0 || destination_channel_zone_id == get_destination_channel_zone_id());
            auto dest = parent_service_.lock();
            auto ret = dest->add_ref(
                protocol_version, 
                destination_channel_zone_id, 
                destination_zone_id, 
                object_id, 
                caller_channel_zone_id, 
                caller_zone_id, 
                build_out_param_channel);  
            
            //auto svc = rpc::static_pointer_cast<child_service>(get_operating_zone_service());
            return ret;
        }
        uint64_t release(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_zone caller_zone_id) override
        {
            auto ret = parent_service_.lock()->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            return ret;
        }
    };

    //this is an equivelent to an host looking at its enclave
    template<class CHILD_PTR_TYPE, class PARENT_PTR_TYPE>
    class local_child_service_proxy : public service_proxy
    {
        rpc::shared_ptr<child_service> child_service_;
        
        typedef std::function<int(const rpc::shared_ptr<PARENT_PTR_TYPE>&, rpc::shared_ptr<CHILD_PTR_TYPE>&, const rpc::shared_ptr<child_service>&)> connect_fn;
        connect_fn fn_;
        
        friend rpc::service;

        local_child_service_proxy(
            destination_zone destination_zone_id,
            const rpc::shared_ptr<service>& parent_svc,
            connect_fn fn)
            : service_proxy(destination_zone_id, parent_svc)
            , fn_(fn)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_child_service_proxy", get_zone_id(), get_destination_zone_id(), parent_svc->get_zone_id().as_caller());
            }
        }

        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::shared_ptr<service_proxy>(new local_child_service_proxy(*this));}
        void clone_completed() override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_child_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }

    public:
        local_child_service_proxy(const local_child_service_proxy& other) = default;
        virtual ~local_child_service_proxy()
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_deletion("local_child_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }

        static rpc::shared_ptr<local_child_service_proxy> create(
            destination_zone destination_zone_id, 
            const rpc::shared_ptr<service>& svc,
            connect_fn fn)
        {
            return rpc::shared_ptr<local_child_service_proxy>(new local_child_service_proxy(destination_zone_id, svc, fn));
        }
        
        int connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override
        {   
            //local_child_service_proxy nests a local_service_proxy back to the parent service
            return rpc::child_service::create_child_zone<rpc::local_service_proxy>(
                get_destination_zone_id().as_zone(),
                get_zone_id().as_destination(), 
                get_operating_zone_service()->get_telemetry_service(),
                input_descr, 
                output_descr,
                fn_,
                child_service_,
                get_operating_zone_service());
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
            return child_service_->send(protocol_version, encoding, tag, caller_channel_zone_id, caller_zone_id, destination_zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , interface_ordinal interface_id
        ) override
        {
            return child_service_->try_cast(protocol_version, destination_zone_id, object_id, interface_id);
        }
        uint64_t add_ref(
            uint64_t protocol_version 
            , destination_channel_zone destination_channel_zone_id 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_channel_zone caller_channel_zone_id 
            , caller_zone caller_zone_id 
            , add_ref_options build_out_param_channel 
        ) override
        {
            if (get_telemetry_service())
            {
                get_telemetry_service()->on_service_proxy_add_ref("local_child_service_proxy", get_zone_id(),
                                                                destination_zone_id, destination_channel_zone_id, get_caller_zone_id(), object_id);
            }
            auto ret = child_service_->add_ref(
                protocol_version, 
                destination_channel_zone_id, 
                destination_zone_id, 
                object_id, 
                caller_channel_zone_id, 
                caller_zone_id, 
                build_out_param_channel);       
            return ret;
        }
        uint64_t release(
            uint64_t protocol_version 
            , destination_zone destination_zone_id 
            , object object_id 
            , caller_zone caller_zone_id) override
        {
            auto ret = child_service_->release(protocol_version, destination_zone_id, object_id, caller_zone_id);
            return ret;            
        }
    };
}