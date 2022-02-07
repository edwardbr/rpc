#pragma once

#include <marshaller/proxy.h>

namespace rpc
{
    class enclave_service_proxy : public service_proxy
    {
        enclave_service_proxy(std::string filename);

    public:
        static rpc::shared_ptr<enclave_service_proxy> create(std::string filename)
        {
            auto ret = rpc::shared_ptr<enclave_service_proxy>(new enclave_service_proxy(filename));
            ret->weak_this_ = rpc::static_pointer_cast<service_proxy>(ret);
            return ret;
        }
        uint64_t eid_ = 0;
        std::string filename_;

    public:
        ~enclave_service_proxy();
        error_code initialise();

        error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, size_t out_size_, char* out_buf_) override;
        error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}