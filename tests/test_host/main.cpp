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

using namespace marshalled_tests;

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
        auto service = rpc::make_shared<rpc::service>();

        rpc::shared_ptr<i_example> ex(new example);
        error_code err_code = service->initialise<i_example, i_example_stub>(ex);
        if (err_code)
        {
            std::cout << "init failed\n";
            break;
        }

        auto marshalled_rpc_service = rpc::static_pointer_cast<rpc::i_marshaller>(service);
        auto service_proxy = rpc::local_rpc_proxy::create(marshalled_rpc_service, service->get_root_object_id());
        
        auto example_ptr = service_proxy->get_interface<i_example>();
        
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
        auto ex = rpc::enclave_rpc_proxy::create(
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
    }
    return 0;
}

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz) { puts(str); }
}