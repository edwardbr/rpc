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

class enclave_telemetry_service : public i_telemetry_service
{
public:
    virtual ~enclave_telemetry_service() = default;

    virtual void on_service_creation(const char* name, uint64_t zone_id) const
    {
        on_service_creation_host(name, zone_id);
    }

    virtual void on_service_deletion(const char* name, uint64_t zone_id) const
    {
        on_service_deletion_host(name, zone_id);
    }
    virtual void on_service_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const
    {
        on_service_proxy_creation_host(name, originating_zone_id, zone_id);
    }
    virtual void on_service_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const
    {
        on_service_proxy_deletion_host(name, originating_zone_id, zone_id);
    }
    virtual void on_service_proxy_try_cast(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        on_service_proxy_try_cast_host(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    virtual void on_service_proxy_add_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        on_service_proxy_add_ref_host(name, originating_zone_id, zone_id, object_id);
    }
    virtual void on_service_proxy_release(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        on_service_proxy_release_host(name, originating_zone_id, zone_id, object_id);
    }

    virtual void on_impl_creation(const char* name, uint64_t interface_id) const
    {
        on_impl_creation_host(name, interface_id);
    }
    virtual void on_impl_deletion(const char* name, uint64_t interface_id) const
    {
        on_impl_deletion_host(name, interface_id);
    }

    virtual void on_stub_creation(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        on_stub_creation_host(name, zone_id, object_id, interface_id);
    }
    virtual void on_stub_deletion(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        on_stub_deletion_host(name, zone_id, object_id, interface_id);
    }
    virtual void on_stub_send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const
    {
        on_stub_send_host(zone_id, object_id, interface_id, method_id);
    }
    virtual void on_stub_add_ref(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const
    {
        on_stub_add_ref_host(zone_id, object_id, interface_id, count);
    }
    virtual void on_stub_release(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const
    {
        on_stub_release_host(zone_id, object_id, interface_id, count);
    }

    virtual void on_object_proxy_creation(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        on_object_proxy_creation_host(originating_zone_id, zone_id, object_id);
    }
    virtual void on_object_proxy_deletion(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        on_object_proxy_deletion_host(originating_zone_id, zone_id, object_id);
    }

    virtual void on_interface_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        on_proxy_creation_host(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    virtual void on_interface_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        on_proxy_deletion_host(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    virtual void on_interface_proxy_send(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const
    {
        on_proxy_send_host(name, originating_zone_id, zone_id, object_id, interface_id, method_id);
    }

    virtual void message(i_telemetry_service::level_enum level, const char* message) const
    {
        message_host(level, message);
    }
};

enclave_telemetry_service telemetry_service;

rpc::shared_ptr<rpc::child_service> rpc_server;

int marshal_test_init_enclave(uint64_t host_zone_id, uint64_t child_zone_id, uint64_t* root_object_id)
{
    //create a zone service for the enclave
    rpc_server = rpc::make_shared<rpc::child_service>(child_zone_id); 
    const i_telemetry_service* p_telemetry_service = &telemetry_service;
    auto host_proxy = rpc::host_service_proxy::create(host_zone_id, rpc_server, p_telemetry_service);
    rpc_server->set_parent(host_proxy);

    //create the root object
    rpc::shared_ptr<yyy::i_example> ex(new example(p_telemetry_service));
    rpc_server->create_stub<yyy::i_example, yyy::i_example_stub>(ex, root_object_id);

    return 0;
}

void marshal_test_destroy_enclave()
{
    rpc_server.reset();
}

int call_enclave(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t sz_int, const char* data_in,
         size_t sz_out, char* data_out, size_t* data_out_sz, void** tls)
{
    //a retry cache using thread local storage, perhaps leaky if the client does not retry with more memory
    std::vector<char>** out_buf = nullptr;
    if(tls)
    {
        out_buf = (std::vector<char>**)tls;
        if(out_buf && *out_buf && !sgx_is_within_enclave(*out_buf, sizeof(std::vector<char>*)))
        {
            return rpc::error::SECURITY_ERROR();
        }
    }


    if(out_buf && *out_buf)
    {
        if((*out_buf)->size() <= sz_out)
        {
            memcpy(data_out, (*out_buf)->data(), (*out_buf)->size());
            *data_out_sz = (*out_buf)->size();
            delete (*out_buf);
            (*out_buf) = nullptr;
            return 0;
        }
        return rpc::error::NEED_MORE_MEMORY();
    }
    
    std::vector<char> tmp;
    int ret = rpc_server->send(zone_id, object_id, interface_id, method_id, sz_int, data_in, tmp);
    if(ret == rpc::error::OK())
    {
        *data_out_sz = tmp.size();
        if(tmp.size() <= sz_out)
        {
            memcpy(data_out, tmp.data(), tmp.size());
            return rpc::error::OK();
        }

        //not enough memory so cache the results into the thread local storage
        if(!out_buf)
            return rpc::error::OUT_OF_MEMORY();
            
        (*out_buf) = new std::vector<char>(std::move(tmp));
        return rpc::error::NEED_MORE_MEMORY();
    }
    return ret;
}

int try_cast_enclave(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    int ret = rpc_server->try_cast(zone_id, object_id, interface_id);
    return ret;
}

uint64_t add_ref_enclave(uint64_t zone_id, uint64_t object_id)
{
    return rpc_server->add_ref(zone_id, object_id);
}

uint64_t release_enclave(uint64_t zone_id, uint64_t object_id)
{
    return rpc_server->release(zone_id, object_id);
}