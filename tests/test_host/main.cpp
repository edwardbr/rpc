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

void remote_tests(rpc_cpp::shared_ptr<i_example> example_ptr)
{
    int val = 0;
    example_ptr->add(1, 2, val);

    // check the creation of an object that is passed back via interface
    rpc_cpp::shared_ptr<marshalled_tests::i_foo> foo;
    example_ptr->create_foo(foo);
    foo->do_something_in_val(22);

    // test casting logic
    auto i_bar_ptr = rpc_cpp::dynamic_pointer_cast<i_bar>(foo);
    if (i_bar_ptr)
        i_bar_ptr->do_something_else(33);

    // test recursive interface passing
    rpc_cpp::shared_ptr<i_foo> other_foo;
    error_code err_code = foo->recieve_interface(other_foo);
    if (err_code)
    {
        std::cout << "create_foo failed\n";
    }
    else
    {
        other_foo->do_something_in_val(22);
    }
}

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
        auto rpc_server = rpc_cpp::make_shared<rpc_service>();

        rpc_cpp::shared_ptr<i_example> ex(new example);
        error_code err_code = rpc_server->initialise<i_example, i_example_stub>(ex);
        if (err_code)
        {
            std::cout << "init failed\n";
            break;
        }

        auto service_proxy = local_rpc_proxy::create(rpc_cpp::static_pointer_cast<i_marshaller>(rpc_server), rpc_server->get_root_object_id());
        
        auto example_ptr = service_proxy->get_remote_interface<i_example>();
        {
            rpc_cpp::shared_ptr<marshalled_tests::i_foo> i_foo_ptr;
            err_code = example_ptr->create_foo(i_foo_ptr);
            if (err_code)
            {
                std::cout << "create_foo failed\n";
                break;
            }
            standard_tests(*i_foo_ptr, true);
        }
        
        remote_tests(example_ptr);

    } while (0);

    // an enclave marshalling of an object
    {
        error_code err_code = 0;
        auto ex = enclave_rpc_proxy::create(
            "C:/Dev/experiments/enclave_marshaller/build/output/debug/marshal_test_enclave.signed.dll");

        err_code = ex->load(zone_config());
        ASSERT(!err_code);

        auto example_ptr = ex->get_remote_interface<i_example>();

        rpc_cpp::shared_ptr<marshalled_tests::i_foo> i_foo_ptr;
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