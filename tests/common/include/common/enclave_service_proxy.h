#pragma once

#include <sgx_urts.h>
#include <marshaller/proxy.h>

namespace rpc
{
    //This is for hosts to call services on an enclave
    class enclave_service_proxy : public service_proxy
    {
        enclave_service_proxy(const rpc::shared_ptr<service>& serv, uint64_t zone_id, std::string filename);
        int inner_initialise(rpc::shared_ptr<object_proxy>& proxy);

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
            rpc::shared_ptr<object_proxy> proxy;
            int err_code = inner_initialise(proxy);
            if(err_code)
                return err_code;
            proxy->query_interface(root_object);
            return rpc::error::OK();
        }

        int send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}