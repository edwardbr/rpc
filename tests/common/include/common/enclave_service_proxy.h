#pragma once

#include <sgx_urts.h>
#include <rpc/proxy.h>

namespace rpc
{
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

        enclave_service_proxy(
            destination_zone destination_zone_id
            , std::string filename
            , const rpc::shared_ptr<service>& svc);
            
        enclave_service_proxy(const enclave_service_proxy& other) = default;
       
        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override;
        void clone_completed() override;
        
        static rpc::shared_ptr<enclave_service_proxy> create(
            destination_zone destination_zone_id
            , const rpc::shared_ptr<service>& svc
            , std::string filename);
        
        int connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override;

        std::shared_ptr<enclave_owner> enclave_owner_;
        uint64_t eid_ = 0;        
        std::string filename_;

        friend rpc::service;        
    public:
        
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