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

rpc::shared_ptr<rpc::child_service> rpc_server;

int marshal_test_init_enclave(uint64_t host_zone_id, uint64_t child_zone_id, uint64_t* root_object)
{
    //create a zone service for the enclave
    rpc_server = rpc::make_shared<rpc::child_service>(child_zone_id); 

    //create the root object
    rpc::shared_ptr<yyy::i_example> ex(new example);
    int err_code = rpc_server->initialise<yyy::i_example, yyy::i_example_stub>(ex, rpc::host_service_proxy::create(rpc_server, host_zone_id));
    if (err_code)
        return err_code;

    *root_object = rpc_server->get_root_object_id();

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