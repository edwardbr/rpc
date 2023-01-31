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

        enclave_service_proxy(destination_zone destination_zone_id, std::string filename, const rpc::shared_ptr<service>& operating_zone_service, object host_id, const rpc::i_telemetry_service* telemetry_service);
        int initialise_enclave(object& object_id);
       
        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::make_shared<enclave_service_proxy>(*this);}
        
        std::shared_ptr<enclave_owner> enclave_owner_;
        uint64_t eid_ = 0;        
        std::string filename_;
        object host_id_ = {0};
    public:
        enclave_service_proxy(const enclave_service_proxy& other) = default;

        template<class Owner, class Child>
        static int create(destination_zone destination_zone_id, std::string filename, rpc::shared_ptr<service>& operating_zone_service, const rpc::shared_ptr<Owner>& owner, rpc::shared_ptr<Child>& root_object, const rpc::i_telemetry_service* telemetry_service)
        {
            assert(operating_zone_service);

            object owner_id = {0};
            if(owner)
                owner_id = {rpc::create_interface_stub(*operating_zone_service, owner).object_id};

            auto ret = rpc::shared_ptr<enclave_service_proxy>(new enclave_service_proxy(destination_zone_id, filename, operating_zone_service, owner_id, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);

            ret->weak_this_ = pthis;

            object object_id = {0};
            int err_code = ret->initialise_enclave(object_id);
            if(err_code)
                return err_code;
            auto error = rpc::demarshall_interface_proxy(ret, {object_id, destination_zone_id}, operating_zone_service->get_zone_id().as_caller(), root_object);
            if(error != rpc::error::OK())
                return error;
            operating_zone_service->add_zone_proxy(ret);
            ret->add_external_ref();
            return rpc::error::OK();
        }

        virtual ~enclave_service_proxy();


        int send(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(destination_zone destination_zone_id, object object_id, interface_ordinal interface_id) override;
        uint64_t add_ref(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id, bool proxy_add_ref) override;
        uint64_t release(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id) override;
    };
}