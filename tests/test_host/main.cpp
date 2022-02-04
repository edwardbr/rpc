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

class local_marshaller : public zone_base_proxy
{
    std::shared_ptr<object_stub>& stub_;

public:
    local_marshaller(std::shared_ptr<object_stub>& stub)
        : stub_(stub)
    {
    }

    error_code send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                            const char* in_buf_, size_t out_size_, char* out_buf_) override
    {
        return stub_->call(interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
    }
    error_code try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
    {
        return stub_->try_cast(interface_id);
    }
    
    uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
    {
        return stub_->add_ref();
    }
    uint64_t release(uint64_t zone_id, uint64_t object_id) override
    {
        return stub_->release([](){});
    }
};

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
        auto rpc_server = rpc_cpp::make_shared<zone_stub>();

        rpc_cpp::shared_ptr<i_example> ex(new example);
        error_code err_code = rpc_server->initialise<i_example, i_example_stub>(ex);
        if (err_code)
        {
            std::cout << "init failed\n";
            continue;
        }

        auto lzp = rpc_cpp::make_shared<local_zone_proxy>(rpc_server, rpc_server->get_root_object_id());
        lzp->set_root_object(rpc_server->get_root_object_id());

        auto example_ptr = lzp->get_remote_interface<i_example>();

        rpc_cpp::shared_ptr<marshalled_tests::i_foo> i_foo_ptr;
        err_code = example_ptr->create_foo(i_foo_ptr);
        if (err_code)
        {
            std::cout << "create_foo failed\n";
            continue;
        }

        i_foo& foo = *i_foo_ptr;
        standard_tests(foo, false);

        auto i_barr = rpc_cpp::dynamic_pointer_cast<i_bar>(i_foo_ptr);
        if(i_barr)
            i_barr->do_something_else(33);

        
        rpc_cpp::shared_ptr<marshalled_tests::i_foo> target;
        example_ptr->create_foo(target);
        target->do_something_in_val(22);

        //rpc_server->shutdown();
    }while(0);

    // an enclave marshalling of an object
    {
        error_code err_code = 0;
        auto ex = rpc_cpp::make_shared<enclave_zone_proxy>(
            "C:/Dev/experiments/enclave_marshaller/build/output/debug/marshal_test_enclave.signed.dll");
        
        err_code = ex->load(zone_config());
        ASSERT(!err_code);

        auto root_ptr = ex->get_remote_interface<i_example>();

        int val = 0;
        root_ptr->add(1,2,val);

        // work in progress create_foo should be passing back an instance of foo
        auto op = std::make_shared<object_proxy>(1, 0, rpc_cpp::static_pointer_cast<zone_base_proxy>(ex));
        i_foo_proxy proxy(op);
        standard_tests(proxy, true);

        rpc_cpp::shared_ptr<marshalled_tests::i_foo> target;
        root_ptr->create_foo(target);
        target->do_something_in_val(22);
    }
    return 0;
}

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz) { puts(str); }
}