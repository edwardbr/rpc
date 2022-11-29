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

        enclave_service_proxy(uint64_t zone_id, std::string filename, const rpc::shared_ptr<service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service);
        int initialise_enclave(rpc::shared_ptr<object_proxy>& proxy);

       
        rpc::shared_ptr<service_proxy> clone_for_zone(uint64_t zone_id) override
        {
            auto ret = rpc::make_shared<enclave_service_proxy>(*this);
            ret->set_zone_id(zone_id);
            ret->weak_this_ = ret;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("enclave_service_proxy", ret->get_operating_zone_id(), ret->get_zone_id());
            }
            get_operating_zone_service()->inner_add_zone_proxy(ret);
            return ret;
        }
        std::shared_ptr<enclave_owner> enclave_owner_;
        uint64_t eid_ = 0;        
        std::string filename_;
    public:
        enclave_service_proxy(const enclave_service_proxy& other) = default;

        template<class T>
        static int create(uint64_t zone_id, std::string filename, const rpc::shared_ptr<service>& operating_zone_service, rpc::shared_ptr<T>& root_object, const rpc::i_telemetry_service* telemetry_service)
        {
            assert(operating_zone_service);
            auto ret = rpc::shared_ptr<enclave_service_proxy>(new enclave_service_proxy(zone_id, filename, operating_zone_service, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);

            ret->weak_this_ = pthis;

            rpc::shared_ptr<object_proxy> proxy;
            int err_code = ret->initialise_enclave(proxy);
            if(err_code)
                return err_code;
            auto error = proxy->query_interface(root_object);
            if(error != rpc::error::OK())
                return error;
            operating_zone_service->add_zone_proxy(ret);
            ret->add_external_ref();
            return rpc::error::OK();
        }

        virtual ~enclave_service_proxy();


        int send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}