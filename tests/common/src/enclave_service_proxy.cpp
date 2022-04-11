#include "common/enclave_service_proxy.h"

#ifndef _IN_ENCLAVE
#include <sgx_urts.h>
#include <sgx_capable.h>

#include <untrusted/enclave_marshal_test_u.h>

#endif

namespace rpc
{
#ifndef _IN_ENCLAVE
    enclave_service_proxy::enclave_service_proxy(const rpc::shared_ptr<service>& serv, uint64_t zone_id,
                                                 std::string filename)
        : service_proxy(serv, zone_id)
        , filename_(filename)
    {
    }

    enclave_service_proxy::~enclave_service_proxy()
    {
        marshal_test_destroy_enclave(eid_);
        sgx_destroy_enclave(eid_);
    }

    error_code enclave_service_proxy::initialise()
    {
        sgx_launch_token_t token = {0};
        int updated = 0;
        sgx_status_t status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);
        if (status)
            return -1;
        error_code err_code = 0;
        uint64_t object_id = 0;
        status = marshal_test_init_enclave(eid_, &err_code, get_zone_id(), &object_id);
        if (status)
            return -1;
        set_root_object(object_id);
        return err_code;
    }

    error_code enclave_service_proxy::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id,
                                           size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
    {
        error_code err_code = 0;
        size_t data_out_sz = 0;
        void* tls = nullptr;
        sgx_status_t status = ::call_enclave(eid_, &err_code, object_id, interface_id, method_id, in_size_, in_buf_,
                                             out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);

        if (status)
            return -1;

        out_buf_.resize(data_out_sz);

        if (err_code == -2)
        {
            // data too small reallocate memory and try again
            status = ::call_enclave(eid_, &err_code, object_id, interface_id, method_id, in_size_, in_buf_,
                                    out_buf_.size(), out_buf_.data(), &data_out_sz, &tls);
        }

        return err_code;
    }

    error_code enclave_service_proxy::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        error_code err_code = 0;
        sgx_status_t status = ::try_cast_enclave(eid_, &err_code, zone_id, object_id, interface_id);
        if (status)
            err_code = -1;
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