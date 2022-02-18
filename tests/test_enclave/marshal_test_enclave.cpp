#include "stdio.h"
#include <string>
#include <cstring>
#include <cstdio>

#include "sgx_trts.h"
#include "trusted/enclave_marshal_test_t.h"

#include "../common/foo_impl.h"

#include <example/example.h>
#include <example_stub.cpp>
#include <example_proxy.cpp>
#include <marshaller/host_service_proxy.h>

using namespace marshalled_tests;

rpc::shared_ptr<rpc::service> rpc_server;
rpc::shared_ptr<rpc::host_service_proxy> host_service;

int marshal_test_init_enclave(uint64_t zone_id, uint64_t* root_object)
{
    //create a zone service for the enclave
    rpc_server = rpc::make_shared<rpc::service>(zone_id); 

    //create the root object
    rpc::shared_ptr<yyy::i_example> ex(new example);
    error_code err_code = rpc_server->initialise<yyy::i_example, yyy::i_example_stub>(ex);
    if (err_code)
        return err_code;

    //create a zone proxy for the host
    host_service = rpc::host_service_proxy::create(rpc_server, 1);

    //wire in the ocalls
    rpc_server->add_zone(host_service->get_zone_id(), host_service);

    *root_object = rpc_server->get_root_object_id();

    return 0;
}

void marshal_test_destroy_enclave()
{
    host_service.reset();
    rpc_server.reset();
}

int call_enclave(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t sz_int, const char* data_in,
         size_t sz_out, char* data_out)
{
    error_code ret = rpc_server->send(object_id, interface_id, method_id, sz_int, data_in, sz_out, data_out);
    return ret;
}

int try_cast_enclave(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    error_code ret = rpc_server->try_cast(zone_id, object_id, interface_id);
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