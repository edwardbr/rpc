#include <iostream>

#include <sgx_urts.h>
#include <sgx_quote.h>
#include <sgx_uae_service.h>
//#include <sgx_tae_service.h>
#include <sgx_capable.h>
#include <sgx_uae_service.h>
#include <sgx_eid.h>
#include "untrusted/enclave_marshal_test_u.h"

#include "../common/foo_impl.h"
#include "../common/tests.h"

#include <example/example.h>
#include <example_proxy.cpp>
#include <example_stub.cpp>

#include <marshaller/enclave_service_proxy.h>
#include <marshaller/local_service_proxy.h>

using namespace marshalled_tests;

rpc::service* current_host_service = nullptr;

int main()
{
    // conventional c++ object on stack
    {
        foo f;
        standard_tests(f, false);
    }

    // an inprocess marshalling of an object
    do
    {
        auto service = rpc::make_shared<rpc::service>(1);

        auto marshaller = rpc::static_pointer_cast<rpc::i_marshaller>(service);

        auto other_service = rpc::make_shared<rpc::service>(2); 
        //create the root object
        rpc::shared_ptr<i_example> ex(new example);
        error_code err_code = other_service->initialise<i_example, i_example_stub>(ex);
        if (err_code)
            return err_code;

        auto other_marshaller = rpc::static_pointer_cast<rpc::i_marshaller>(other_service);

        //create a proxy for the rpc::service hosting the example object
        auto service_proxy = rpc::local_service_proxy::create(service, 2, other_marshaller, other_service->get_root_object_id());
        auto example_ptr = service_proxy->get_interface<i_example>();

        //create a proxy for the other rpc::service, keep an instance going
        auto other_service_proxy = rpc::local_service_proxy::create(other_service, 1, marshaller, 0);

        rpc::shared_ptr<marshalled_tests::i_foo> i_foo_ptr;
        err_code = example_ptr->create_foo(i_foo_ptr);
        if (err_code)
        {
            std::cout << "create_foo failed\n";
            break;
        }
        standard_tests(*i_foo_ptr, true);

        remote_tests(example_ptr);

    } while (0);
    

    // an enclave marshalling of an object
    {
        error_code err_code = 0;
        auto host_rpc_server = rpc::make_shared<rpc::service>(1);
        current_host_service = host_rpc_server.get();

        auto ex = rpc::enclave_service_proxy::create(host_rpc_server, 2,
            "C:/Dev/experiments/enclave_marshaller/build/output/debug/marshal_test_enclave.signed.dll");

        err_code = ex->initialise();
        ASSERT(!err_code);

        auto example_ptr = ex->get_interface<i_example>();

        rpc::shared_ptr<marshalled_tests::i_foo> i_foo_ptr;
        err_code = example_ptr->create_foo(i_foo_ptr);
        if (err_code)
        {
            std::cout << "create_foo failed\n";
        }
        else
        {
            standard_tests(*i_foo_ptr, true);

            remote_tests(example_ptr);
        }

        current_host_service = nullptr;
    }
    return 0;
}

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz) { puts(str); }

    int call_host(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t sz_int, const char* data_in,
                  size_t sz_out, char* data_out)
    {
        error_code ret = current_host_service->send(object_id, interface_id, method_id, sz_int, data_in, sz_out, data_out);
        return ret;
    }
    int try_cast_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        error_code ret = current_host_service->try_cast(zone_id, object_id, interface_id);
        return ret;
    }
    uint64_t add_ref_host(uint64_t zone_id, uint64_t object_id) { return current_host_service->add_ref(zone_id, object_id); }
    uint64_t release_host(uint64_t zone_id, uint64_t object_id) { return current_host_service->release(zone_id, object_id); }
}