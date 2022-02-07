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

using namespace marshalled_tests;

rpc::service rpc_server;

int enclave_marshal_test_init(uint64_t* root_object)
{
    rpc::shared_ptr<i_example> ex(new example);
    error_code err_code = rpc_server.initialise<i_example, i_example_stub>(ex);
    if (err_code)
        return err_code;

    *root_object = rpc_server.get_root_object_id();

    return 0;
}

void enclave_marshal_test_destroy()
{
    rpc_server.shutdown();
}

int call(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t sz_int, const char* data_in,
         size_t sz_out, char* data_out)
{
    error_code ret = rpc_server.send(object_id, interface_id, method_id, sz_int, data_in, sz_out, data_out);
    return ret;
}

int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
{
    error_code ret = rpc_server.try_cast(zone_id, object_id, interface_id);
    return ret;
}

uint64_t add_ref(uint64_t zone_id, uint64_t object_id)
{
    return rpc_server.add_ref(zone_id, object_id);
}

uint64_t release(uint64_t zone_id, uint64_t object_id)
{
    return rpc_server.release(zone_id, object_id);
}