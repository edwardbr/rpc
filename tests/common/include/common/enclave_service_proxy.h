#pragma once

#include <marshaller/proxy.h>

namespace rpc
{
    //This is for hosts to call services on an enclave
    class enclave_service_proxy : public service_proxy
    {
        enclave_service_proxy(const rpc::shared_ptr<service>& serv, uint64_t zone_id, std::string filename);

    public:
        static rpc::shared_ptr<enclave_service_proxy> create(const rpc::shared_ptr<service>& host_serv, uint64_t zone_id, std::string filename)
        {
            auto ret = rpc::shared_ptr<enclave_service_proxy>(new enclave_service_proxy(host_serv, zone_id, filename));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            return ret;
        }
        uint64_t eid_ = 0;
        std::string filename_;

    public:
        virtual ~enclave_service_proxy();
        
        template<class T>
        int initialise(rpc::shared_ptr<T>& root_object)
        {
            sgx_launch_token_t token = {0};
            int updated = 0;
            #ifdef _WIN32
                auto status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);
            #else
                auto status = sgx_create_enclave(filename_.data(), 1, &token, &updated, &eid_, NULL);
            #endif
            if (status)
                return -1;
            int err_code = error::OK();
            uint64_t object_id = 0;
            status = marshal_test_init_enclave(eid_, &err_code, get_service().get_zone_id(), get_zone_id(), &object_id);
            if (status)
                return -1;
            auto proxy = object_proxy::create(object_id, get_zone_id(), shared_from_this());
            proxy->query_interface(root_object);
            return err_code;
        }

        int send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}