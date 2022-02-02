#include "marshaller/proxy.h"

#ifndef _IN_ENCLAVE
#include <sgx_urts.h>
#include <sgx_capable.h>

#include "untrusted/enclave_marshal_test_u.h"
#endif

error_code object_proxy::send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                              size_t out_size_, char* out_buf_)
{
    return marshaller_->send(object_id_, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
}

void object_proxy::register_interface(uint64_t interface_id, rpc_cpp::weak_ptr<i_proxy_impl>& value)
{
    std::lock_guard guard(insert_control);
    auto item = proxy_map.find(interface_id);
    if (item != proxy_map.end())
    {
        auto tmp = item->second.lock();
        if (tmp == nullptr)
        {
            item->second = value;
            return;
        }
        value = item->second;
        return;
    }
    proxy_map[interface_id] = value;
}


#ifndef _IN_ENCLAVE
enclave_zone_proxy::enclave_zone_proxy(std::string filename)
    : filename_(filename)
{
}

enclave_zone_proxy::~enclave_zone_proxy()
{
    enclave_marshal_test_destroy(eid_);
    sgx_destroy_enclave(eid_);
}

error_code enclave_zone_proxy::load(zone_config& config)
{
    sgx_launch_token_t token = {0};
    int updated = 0;
    sgx_status_t status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);
    if (status)
        return -1;
    error_code err_code = 0;
    enclave_marshal_test_init(eid_, &err_code, &config);
    return err_code;
}

error_code enclave_zone_proxy::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                           const char* in_buf_, size_t out_size_, char* out_buf_)
{
    error_code err_code = 0;
    sgx_status_t status
        = ::call(eid_, &err_code, object_id, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
    if (status)
        err_code = -1;
    return err_code;
}

error_code enclave_zone_proxy::try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id)
{
    error_code err_code = 0;
    sgx_status_t status = ::try_cast(eid_, &err_code, zone_id_, object_id, interface_id);
    if (status)
        err_code = -1;
    return err_code;
}

#endif
