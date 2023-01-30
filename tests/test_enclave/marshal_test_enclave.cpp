#include "stdio.h"
#include <string>
#include <cstring>
#include <cstdio>

#include <sgx_trts.h>
#include <trusted/enclave_marshal_test_t.h>

#include <rpc/error_codes.h>

#include <common/foo_impl.h>
#include <common/host_service_proxy.h>

#include <example/example.h>

#include <example_shared_proxy.cpp>
#include <example_shared_stub.cpp>

#include <example_import_proxy.cpp>
#include <example_import_stub.cpp>

#include <example_proxy.cpp>
#include <example_stub.cpp>
#include <rpc/remote_pointer.h>

using namespace marshalled_tests;

class enclave_telemetry_service : public rpc::i_telemetry_service
{
public:
    virtual ~enclave_telemetry_service() = default;

    virtual void on_service_creation(const char* name, rpc::zone zone_id) const
    {
        on_service_creation_host(name, zone_id.get_val());
    }

    virtual void on_service_deletion(const char* name, rpc::zone zone_id) const
    {
        on_service_deletion_host(name, zone_id.get_val());
    }
    virtual void on_service_proxy_creation(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id) const
    {
        on_service_proxy_creation_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val());
    }
    virtual void on_service_proxy_deletion(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id) const
    {
        on_service_proxy_deletion_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val());
    }
    virtual void on_service_proxy_try_cast(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_service_proxy_try_cast_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_service_proxy_add_ref(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_proxy_add_ref_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
    }
    virtual void on_service_proxy_release(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_proxy_release_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
    }

    virtual void on_impl_creation(const char* name, rpc::interface_ordinal interface_id) const
    {
        on_impl_creation_host(name, interface_id.get_val());
    }
    virtual void on_impl_deletion(const char* name, rpc::interface_ordinal interface_id) const
    {
        on_impl_deletion_host(name, interface_id.get_val());
    }

    virtual void on_stub_creation(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_stub_creation_host(name, zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_stub_deletion(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_stub_deletion_host(name, zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        on_stub_send_host(zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val());
    }
    virtual void on_stub_add_ref(rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
    {
        on_stub_add_ref_host(destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), count, caller_zone_id.get_val());
    }
    virtual void on_stub_release(rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
    {
        on_stub_release_host(destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), count, caller_zone_id.get_val());
    }

    virtual void on_object_proxy_creation(rpc::zone operating_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
    {
        on_object_proxy_creation_host(operating_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
    }
    virtual void on_object_proxy_deletion(rpc::zone operating_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
    {
        on_object_proxy_deletion_host(operating_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
    }

    virtual void on_interface_proxy_creation(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_proxy_creation_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_interface_proxy_deletion(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_proxy_deletion_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_interface_proxy_send(const char* name, rpc::zone caller_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        on_proxy_send_host(name, caller_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val());
    }

    void on_service_proxy_add_external_ref(const char* name, rpc::zone operating_zone_id, rpc::destination_zone destination_zone_id, int ref_count, rpc::caller_zone caller_zone_id) const
    {
        ::on_service_proxy_add_external_ref(name, operating_zone_id.get_val(), destination_zone_id.get_val(), ref_count, caller_zone_id.get_val());
    }

    void on_service_proxy_release_external_ref(const char* name, rpc::zone operating_zone_id, rpc::destination_zone destination_zone_id, int ref_count, rpc::caller_zone caller_zone_id) const
    {
        ::on_service_proxy_release_external_ref(name, operating_zone_id.get_val(), destination_zone_id.get_val(), ref_count, caller_zone_id.get_val());
    }

    virtual void message(rpc::i_telemetry_service::level_enum level, const char* message) const
    {
        message_host(level, message);
    }
};
 
enclave_telemetry_service telemetry_service;

rpc::shared_ptr<rpc::child_service> rpc_server;

int marshal_test_init_enclave(uint64_t host_zone_id, uint64_t host_id, uint64_t child_zone_id, uint64_t* example_object_id)
{
    //create a rpc::zone service for the enclave
    rpc_server = rpc::make_shared<rpc::child_service>(rpc::zone{child_zone_id}); 
    const rpc::i_telemetry_service* p_telemetry_service = &telemetry_service;

    rpc::shared_ptr<yyy::i_host> host;
    
    if(host_id)
    {
        auto host_proxy = rpc::host_service_proxy::create({host_zone_id}, {host_id}, rpc_server, p_telemetry_service);
        auto err_code = rpc::demarshall_interface_proxy<yyy::i_host>(host_proxy, {{host_id}, {host_zone_id}}, {child_zone_id}, host);
        if(err_code != rpc::error::OK())
            return err_code;
    }

    //create the root rpc::object
    rpc::shared_ptr<yyy::i_example> ex(new example(p_telemetry_service, host));
    
    auto example_encap = rpc::create_interface_stub(*rpc_server, ex);
    *example_object_id = example_encap.object_id.get_val();

    return rpc::error::OK();
}

void marshal_test_destroy_enclave()
{
    rpc_server.reset();
}

int call_enclave(
    uint64_t caller_channel_zone_id,
    uint64_t caller_zone_id,
    uint64_t zone_id, 
    uint64_t object_id, 
    uint64_t interface_id, 
    uint64_t method_id, 
    size_t sz_int, 
    const char* data_in,
    size_t sz_out, 
    char* data_out, 
    size_t* data_out_sz, 
    void** enclave_retry_buffer)
{
    //a retry cache using enclave_retry_buffer as thread local storage, leaky if the client does not retry with more memory
    if(!enclave_retry_buffer)
    {        
        return rpc::error::INVALID_DATA();
    }

    auto*& retry_buf = *reinterpret_cast<std::vector<char>**>(enclave_retry_buffer);
    if(retry_buf && !sgx_is_within_enclave(retry_buf, sizeof(std::vector<char>*)))
    {
        return rpc::error::SECURITY_ERROR();
    }


    if(retry_buf)
    {
        *data_out_sz = retry_buf->size();
        if(*data_out_sz > sz_out)
        {
            return rpc::error::NEED_MORE_MEMORY();
        }
    
        memcpy(data_out, retry_buf->data(), retry_buf->size());
        delete retry_buf;
        retry_buf = nullptr;
        return rpc::error::OK();
    }

    std::vector<char> tmp;
    int ret = rpc_server->send({caller_channel_zone_id}, {caller_zone_id}, {zone_id}, {object_id}, {interface_id}, {method_id}, sz_int, data_in, tmp);
    if(ret >= rpc::error::MIN() && ret <= rpc::error::MAX())
        return ret;

    *data_out_sz = tmp.size();
    if(*data_out_sz <= sz_out)
    {
        memcpy(data_out, tmp.data(), *data_out_sz);
        return rpc::error::OK();
    }

    retry_buf = new std::vector<char>(std::move(tmp));
    return rpc::error::NEED_MORE_MEMORY();
}

int try_cast_enclave(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    int ret = rpc_server->try_cast({zone_id}, {object_id}, {interface_id});
    return ret;
}

uint64_t add_ref_enclave(uint64_t zone_id, uint64_t object_id, uint64_t caller_zone_id)
{
    return rpc_server->add_ref({zone_id}, {object_id}, {caller_zone_id}, false);
}

uint64_t release_enclave(uint64_t zone_id, uint64_t object_id, uint64_t caller_zone_id)
{
    return rpc_server->release({zone_id}, {object_id}, {caller_zone_id});
}