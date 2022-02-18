#include "marshaller/host_service_proxy.h"


#ifdef _IN_ENCLAVE

#include "trusted/enclave_marshal_test_t.h"


namespace rpc
{
    host_service_proxy::host_service_proxy(const rpc::shared_ptr<service>& serv, uint64_t zone_id)
        : service_proxy(serv, zone_id)
    {
    }

    host_service_proxy::~host_service_proxy()
    {
    }

    error_code host_service_proxy::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                                       const char* in_buf_, std::vector<char>& out_buf_)
    {
        error_code err_code = 0;
        size_t data_out_sz = 0;
        sgx_status_t status
            = ::call_host(&err_code, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);

        if (status)
            return -1;

        out_buf_.resize(data_out_sz);
        if (status == -2)
        {
            //data too small reallocate memory and try again

            status
                = ::call_host(&err_code, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_.size(), out_buf_.data(), &data_out_sz);
        }
        return err_code;
    }

    error_code host_service_proxy::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        error_code err_code = 0;
        sgx_status_t status = ::try_cast_host(&err_code, zone_id, object_id, interface_id);
        if (status)
            err_code = -1;
        return err_code;
    }

    uint64_t host_service_proxy::add_ref(uint64_t zone_id, uint64_t object_id)
    {
        uint64_t ret = 0;
        sgx_status_t status = ::add_ref_host(&ret, zone_id, object_id);
        if (status)
            return std::numeric_limits<uint64_t>::max();
        return ret;
    }

    uint64_t host_service_proxy::release(uint64_t zone_id, uint64_t object_id)
    {
        uint64_t ret = 0;
        sgx_status_t status = ::release_host(&ret, zone_id, object_id);
        if (status)
            return std::numeric_limits<uint64_t>::max();
        return ret;
    }
}

#endif
