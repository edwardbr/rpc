#include "common/enclave_service_proxy.h"

#ifndef _IN_ENCLAVE
#include <sgx_urts.h>
#include <sgx_capable.h>

#include <untrusted/enclave_marshal_test_u.h>

#endif

namespace rpc
{
#ifndef _IN_ENCLAVE
    enclave_service_proxy::enclave_service_proxy(uint64_t zone_id, std::string filename, const rpc::shared_ptr<service>& operating_zone_service)
        : service_proxy(zone_id, operating_zone_service)
        , filename_(filename)
    {
        log_str("enclave_service_proxy",100);
    }

    enclave_service_proxy::~enclave_service_proxy()
    {
        log_str("~enclave_service_proxy",100);
        marshal_test_destroy_enclave(eid_);
        sgx_destroy_enclave(eid_);
    }

    int enclave_service_proxy::initialise_enclave(rpc::shared_ptr<object_proxy>& proxy)
    {
        sgx_launch_token_t token = {0};
        int updated = 0;
        #ifdef _WIN32
            auto status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);
        #else
            auto status = sgx_create_enclave(filename_.data(), 1, &token, &updated, &eid_, NULL);
        #endif
        if (status)
            return rpc::error::TRANSPORT_ERROR();
        int err_code = error::OK();
        uint64_t object_id = 0;
        status = marshal_test_init_enclave(eid_, &err_code, get_operating_zone_id(), get_zone_id(), &object_id);
        if (status)
            return rpc::error::TRANSPORT_ERROR();
        if (err_code)
            return err_code;
        proxy = object_proxy::create(object_id, get_zone_id(), shared_from_this());
        return rpc::error::OK();
    }

    int enclave_service_proxy::send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id,
                                           size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(zone_id != get_zone_id())
            return rpc::error::ZONE_NOT_SUPPORTED();        
        int err_code = 0;
        size_t data_out_sz = 0;
        void* tls = nullptr;
        sgx_status_t status = ::call_enclave(eid_, &err_code, zone_id, object_id, interface_id, method_id, in_size_, in_buf_,
                                             out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);

        if (status)
            return rpc::error::TRANSPORT_ERROR();

        if (err_code == rpc::error::NEED_MORE_MEMORY())
        {
            // data too small reallocate memory and try again
            out_buf_.resize(data_out_sz);
            status = ::call_enclave(eid_, &err_code, zone_id, object_id, interface_id, method_id, in_size_, in_buf_,
                                    out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);
            if (status)
                return rpc::error::TRANSPORT_ERROR();
        }

        return err_code;
    }

    int enclave_service_proxy::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        int err_code = 0;
        sgx_status_t status = ::try_cast_enclave(eid_, &err_code, zone_id, object_id, interface_id);
        if (status)
            return rpc::error::TRANSPORT_ERROR();
        return err_code;
    }

    uint64_t enclave_service_proxy::add_ref(uint64_t zone_id, uint64_t object_id)
    {
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_enclave(eid_, &ret, zone_id, object_id);
        if (status)
            return std::numeric_limits<uint64_t>::max();
        return ret;
    }

    uint64_t enclave_service_proxy::release(uint64_t zone_id, uint64_t object_id)
    {
        uint64_t ret = 0;
        sgx_status_t status = ::release_enclave(eid_, &ret, zone_id, object_id);
        if (status)
            return std::numeric_limits<uint64_t>::max();
        return ret;
    }
#endif
}