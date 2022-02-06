#include "marshaller/proxy.h"

#include <limits>
#ifndef _IN_ENCLAVE
#include <sgx_urts.h>
#include <sgx_capable.h>

#include "untrusted/enclave_marshal_test_u.h"
#endif

object_proxy::~object_proxy()
{
    marshaller_->release(zone_id_, object_id_);
}

error_code object_proxy::send(uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_,
                              size_t out_size_, char* out_buf_)
{
    return marshaller_->send(object_id_, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
}

void object_proxy::register_interface(uint64_t interface_id, rpc_cpp::weak_ptr<proxy_base>& value)
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


error_code rpc_proxy::set_root_object(uint64_t object_id)
{
    if(root_object_proxy_)
        return -1;
    root_object_proxy_ = object_proxy::create(object_id, zone_id_ , shared_from_this());
    return 0;
}


#ifndef _IN_ENCLAVE
enclave_rpc_proxy::enclave_rpc_proxy(std::string filename)
    : filename_(filename)
{
}

enclave_rpc_proxy::~enclave_rpc_proxy()
{
    enclave_marshal_test_destroy(eid_);
    sgx_destroy_enclave(eid_);
}

error_code enclave_rpc_proxy::load(zone_config& config)
{
    sgx_launch_token_t token = {0};
    int updated = 0;
    sgx_status_t status = sgx_create_enclavea(filename_.data(), 1, &token, &updated, &eid_, NULL);
    if (status)
        return -1;
    error_code err_code = 0;
    uint64_t object_id = 0;
    status = enclave_marshal_test_init(eid_, &err_code, &config, &object_id);
    if (status)
        return -1;
    set_root_object(object_id);
    return err_code;
}

error_code enclave_rpc_proxy::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                           const char* in_buf_, size_t out_size_, char* out_buf_)
{
    error_code err_code = 0;
    sgx_status_t status
        = ::call(eid_, &err_code, object_id, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
    if (status)
        err_code = -1;
    return err_code;
}

error_code enclave_rpc_proxy::try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    error_code err_code = 0;
    sgx_status_t status = ::try_cast(eid_, &err_code, zone_id, object_id, interface_id);
    if (status)
        err_code = -1;
    return err_code;
}


uint64_t enclave_rpc_proxy::add_ref(uint64_t zone_id, uint64_t object_id)
{
    uint64_t ret = 0;
    sgx_status_t status = ::add_ref(eid_, &ret, zone_id, object_id);
    if (status)
        return std::numeric_limits<uint64_t>::max();
    return ret;
}

uint64_t enclave_rpc_proxy::release(uint64_t zone_id, uint64_t object_id) 
{
    uint64_t ret = 0;
    sgx_status_t status = ::release(eid_, &ret, zone_id, object_id);
    if (status)
        return std::numeric_limits<uint64_t>::max();
    return ret;
}

#endif