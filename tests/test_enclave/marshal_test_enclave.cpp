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
    void on_service_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_service_try_cast_host(name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }

    void on_service_add_ref(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_channel_zone caller_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::add_ref_options options) const
    {
        on_service_add_ref_host(name, zone_id.get_val(), destination_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_channel_zone_id.get_val(), caller_zone_id.get_val(), (uint64_t)options);
    }

    void on_service_release(const char* name, rpc::zone zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_release_host(name, zone_id.get_val(), destination_channel_zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), caller_zone_id.get_val());
    }      
    virtual void on_service_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_proxy_creation_host(name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    virtual void on_service_proxy_deletion(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_proxy_deletion_host(name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    virtual void on_service_proxy_try_cast(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_service_proxy_try_cast_host(name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_service_proxy_add_ref(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id) const
    {
        on_service_proxy_add_ref_host(name, zone_id.get_val(), destination_zone_id.get_val(), destination_channel_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val());
    }
    virtual void on_service_proxy_release(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::caller_zone caller_zone_id, rpc::object object_id) const
    {
        on_service_proxy_release_host(name, zone_id.get_val(), destination_zone_id.get_val(), destination_channel_zone_id.get_val(), caller_zone_id.get_val(), object_id.get_val());
    }

    virtual void on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        on_impl_creation_host(name, address, zone_id.get_val());
    }
    virtual void on_impl_deletion(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        on_impl_deletion_host(name, address, zone_id.get_val());
    }

    virtual void on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
    {
        on_stub_creation_host(zone_id.get_val(), object_id.get_val(), address);
    }
    virtual void on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const
    {
        on_stub_deletion_host(zone_id.get_val(), object_id.get_val());
    }
    virtual void on_stub_send(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        on_stub_send_host(zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val());
    }
    virtual void on_stub_add_ref(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
    {
        on_stub_add_ref_host(zone_id.get_val(), object_id.get_val(), interface_id.get_val(), count, caller_zone_id.get_val());
    }
    virtual void on_stub_release(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id) const
    {
        on_stub_release_host(zone_id.get_val(), object_id.get_val(), interface_id.get_val(), count, caller_zone_id.get_val());
    }

    virtual void on_object_proxy_creation(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const
    {
        on_object_proxy_creation_host(zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), add_ref_done);
    }
    virtual void on_object_proxy_deletion(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
    {
        on_object_proxy_deletion_host(zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
    }

    virtual void on_interface_proxy_creation(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_proxy_creation_host(name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_interface_proxy_deletion(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id) const
    {
        on_proxy_deletion_host(name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    virtual void on_interface_proxy_send(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        on_proxy_send_host(name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val());
    }

    void on_service_proxy_add_external_ref(const char* name, rpc::zone operating_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, int ref_count) const
    {
        on_service_proxy_add_external_ref_host(name, operating_zone_id.get_val(), destination_channel_zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), ref_count);
    }

    void on_service_proxy_release_external_ref(const char* name, rpc::zone operating_zone_id, rpc::destination_channel_zone destination_channel_zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id, int ref_count) const
    {
        on_service_proxy_release_external_ref_host(name, operating_zone_id.get_val(), destination_channel_zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val(), ref_count);
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
    const rpc::i_telemetry_service* p_telemetry_service = &telemetry_service;

    //create a rpc::zone service for the enclave
    rpc_server = rpc::make_shared<rpc::child_service>(rpc::zone{child_zone_id}, rpc::destination_zone{host_zone_id}, p_telemetry_service); 
    example_import_idl_register_stubs(rpc_server);
    example_shared_idl_register_stubs(rpc_server);
    example_idl_register_stubs(rpc_server);

    rpc::shared_ptr<yyy::i_host> host;
    
    if(host_id)
    {
        auto host_proxy = rpc::host_service_proxy::create({host_zone_id}, {host_id}, rpc_server, p_telemetry_service);
        auto err_code = rpc::demarshall_interface_proxy<yyy::i_host>(rpc::get_version(), host_proxy, {{host_id}, {host_zone_id}}, {child_zone_id}, host);
        if(err_code != rpc::error::OK())
            return err_code;
    }

    //create the root rpc::object
    rpc::shared_ptr<yyy::i_example> ex(new example(p_telemetry_service, rpc_server, host));
    
    auto example_encap = rpc::create_interface_stub(*rpc_server, ex);
    *example_object_id = example_encap.object_id.get_val();

    return rpc::error::OK();
}

void marshal_test_destroy_enclave()
{
    rpc_server.reset();
}

int call_enclave(
    uint64_t protocol_version,                          //version of the rpc call protocol
    uint64_t encoding,                                  //format of the serialised data
    uint64_t tag,                                       //info on the type of the call passed from the idl generator 
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
    if(protocol_version > rpc::get_version())
    {
        return rpc::error::INVALID_VERSION();
    }
    //a retry cache using enclave_retry_buffer as thread local storage, leaky if the client does not retry with more memory
    if(!enclave_retry_buffer)
    {        
        return rpc::error::INVALID_DATA();
    }

    auto*& retry_buf = *reinterpret_cast<rpc::retry_buffer**>(enclave_retry_buffer);
    if(retry_buf && !sgx_is_within_enclave(retry_buf, sizeof(rpc::retry_buffer*)))
    {
        return rpc::error::SECURITY_ERROR();
    }


    if(retry_buf)
    {
        *data_out_sz = retry_buf->data.size();
        if(*data_out_sz > sz_out)
        {
            return rpc::error::NEED_MORE_MEMORY();
        }
    
        memcpy(data_out, retry_buf->data.data(), retry_buf->data.size());
        auto ret = retry_buf->return_value;
        delete retry_buf;
        retry_buf = nullptr;
        return ret;
    }

    std::vector<char> tmp;
    tmp.resize(sz_out);
    int ret = rpc_server->send(
        protocol_version,                          //version of the rpc call protocol
        rpc::encoding(encoding),                                  //format of the serialised data
        tag,
        {caller_channel_zone_id}, 
        {caller_zone_id}, 
        {zone_id}, 
        {object_id}, 
        {interface_id}, 
        {method_id}, 
        sz_int, 
        data_in, 
        tmp);
    if(ret >= rpc::error::MIN() && ret <= rpc::error::MAX())
        return ret;

    *data_out_sz = tmp.size();
    if(*data_out_sz <= sz_out)
    {
        memcpy(data_out, tmp.data(), *data_out_sz);
        return ret;
    }

    retry_buf = new rpc::retry_buffer{std::move(tmp), ret};
    return rpc::error::NEED_MORE_MEMORY();
}

int try_cast_enclave(uint64_t protocol_version, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    if(protocol_version > rpc::get_version())
    {
        return rpc::error::INVALID_VERSION();
    }
    int ret = rpc_server->try_cast(protocol_version, {zone_id}, {object_id}, {interface_id});
    return ret;
}

uint64_t add_ref_enclave(uint64_t protocol_version, uint64_t destination_channel_zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_channel_zone_id, uint64_t caller_zone_id, char build_out_param_channel)
{
    if(protocol_version > rpc::get_version())
    {
        return std::numeric_limits<uint64_t>::max();
    }
    return rpc_server->add_ref(
        protocol_version, 
        {destination_channel_zone_id}, 
        {destination_zone_id}, 
        {object_id}, 
        {caller_channel_zone_id}, 
        {caller_zone_id}, 
        static_cast<rpc::add_ref_options>(build_out_param_channel), 
        false);
}

uint64_t release_enclave(uint64_t protocol_version, uint64_t zone_id, uint64_t object_id, uint64_t caller_zone_id)
{
    if(protocol_version > rpc::get_version())
    {
        return std::numeric_limits<uint64_t>::max();
    }
    return rpc_server->release(protocol_version, {zone_id}, {object_id}, {caller_zone_id});
}