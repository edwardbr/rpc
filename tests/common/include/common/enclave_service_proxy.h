#pragma once

#include <sgx_urts.h>
#include <rpc/proxy.h>

namespace rpc
{
    struct make_shared_enclave_service_proxy_enabler;
    //This is for hosts to call services on an enclave
    class enclave_service_proxy : public service_proxy
    {
        struct enclave_owner
        {
        public:
            enclave_owner(uint64_t eid) : eid_(eid)
            {}
            uint64_t eid_ = 0;
            ~enclave_owner();
        };

        enclave_service_proxy(destination_zone destination_zone_id, std::string filename, const rpc::shared_ptr<service>& svc, object host_id, const rpc::i_telemetry_service* telemetry_service);
        int initialise_enclave(object& object_id);
       
        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::make_shared<enclave_service_proxy>(*this);}
        void clone_completed() override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("enclave_service_proxy", get_zone_id(), get_destination_zone_id(), get_caller_zone_id());
            }
        }
        
        std::shared_ptr<enclave_owner> enclave_owner_;
        uint64_t eid_ = 0;        
        std::string filename_;
        object host_id_ = {0};
        
        friend make_shared_enclave_service_proxy_enabler;
    public:
        enclave_service_proxy(const enclave_service_proxy& other) = default;

        template<class Owner, class Child>
        static int create(destination_zone destination_zone_id, std::string filename, rpc::shared_ptr<service>& svc, const rpc::shared_ptr<Owner>& owner, rpc::shared_ptr<Child>& root_object, const rpc::i_telemetry_service* telemetry_service)
        {
        
            struct make_shared_enclave_service_proxy_enabler : public enclave_service_proxy
            {
                virtual ~make_shared_enclave_service_proxy_enabler() = default;
                make_shared_enclave_service_proxy_enabler(
                    destination_zone destination_zone_id
                    , std::string filename
                    , rpc::shared_ptr<service>& svc
                    , object owner_id
                    , const rpc::i_telemetry_service* telemetry_service
                    ) 
                    : enclave_service_proxy(destination_zone_id, filename, svc, owner_id, telemetry_service){}
            };
            assert(svc);

            object owner_id = {0};
            if(owner)
                owner_id = {rpc::create_interface_stub(*svc, owner).object_id};

            auto ret = rpc::make_shared<make_shared_enclave_service_proxy_enabler>(destination_zone_id, filename, svc, owner_id, telemetry_service);
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);

            object object_id = {0};
            int err_code = ret->initialise_enclave(object_id);
            if(err_code)
                return err_code;
            auto error = rpc::demarshall_interface_proxy(rpc::get_version(), ret, {object_id, destination_zone_id}, svc->get_zone_id().as_caller(), root_object);
            if(error != rpc::error::OK())
                return error;
            svc->add_zone_proxy(ret);
            return rpc::error::OK();
        }   
        virtual ~enclave_service_proxy();

        int send(
            uint64_t protocol_version, 
			encoding encoding, 
			uint64_t tag, 
            caller_channel_zone caller_channel_zone_id, 
            caller_zone caller_zone_id, 
            destination_zone destination_zone_id, 
            object object_id, 
            interface_ordinal interface_id, 
            method method_id, 
            size_t in_size_,
            const char* in_buf_, 
            std::vector<char>& out_buf_)
            override;
        int try_cast(            
            uint64_t protocol_version, 
            destination_zone destination_zone_id, 
            object object_id, 
            interface_ordinal interface_id) override;
        uint64_t add_ref(
            uint64_t protocol_version, 
            destination_channel_zone destination_channel_zone_id, 
            destination_zone destination_zone_id, 
            object object_id, 
            caller_channel_zone caller_channel_zone_id, 
            caller_zone caller_zone_id, 
            add_ref_options build_out_param_channel, 
            bool proxy_add_ref) override;
        uint64_t release(
            uint64_t protocol_version, 
            destination_zone destination_zone_id, 
            object object_id, 
            caller_zone caller_zone_id) override;
    };
}