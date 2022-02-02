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
    error_code try_cast(uint64_t zone_id_, uint64_t object_id, uint64_t interface_id) override
    {
        return stub_->try_cast(interface_id);
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
    {
        //create a foo and expose it as i_foo
        auto foo_ptr = std::shared_ptr<i_foo>(new foo());

        //wrap an interface stub around it
        auto stub = std::make_shared<i_foo_stub>(foo_ptr);

        //assign and object_stub to the interface stub
        auto os = std::make_shared<object_stub>(stub);

        //wrap a marshaller around the object_stub
        auto marshaller = rpc_cpp::shared_ptr<zone_base_proxy>(new local_marshaller(os));

        //assign the marshaller to the object_proxy
        auto op = rpc_cpp::shared_ptr<object_proxy>(new object_proxy(0, 0, marshaller));

        //get a remote pointer to ifoo
        rpc_cpp::shared_ptr<i_foo> i_foo_ptr;
        op->query_interface(i_foo_ptr);

        i_foo& foo = *i_foo_ptr;
        standard_tests(foo, false);

        auto i_barr = rpc_cpp::dynamic_pointer_cast<i_bar>(i_foo_ptr);
        if(i_barr)
            i_barr->do_something_else(33);
    }

    // an enclave marshalling of an object
    {
        error_code err_code = 0;
        auto ex = rpc_cpp::shared_ptr<example_proxy>(new example_proxy(
            "C:/Dev/experiments/enclave_marshaller/build/output/debug/marshal_test_enclave.signed.dll"));
        
        err_code = ex->load(zone_config());
        ASSERT(!err_code);

        auto root_ptr = ex->get_remote_interface();

        int val = 0;
        root_ptr->add(1,2,val);

        /*shared_ptr<marshalled_tests::i_foo> target;
        err_code = ex.create_foo(target);
        if(!err_code)
            std::cout << "aggggggg!";*/

        // work in progress create_foo should be passing back an instance of foo
        auto op = std::make_shared<object_proxy>(1, 0, rpc_cpp::static_pointer_cast<zone_base_proxy>(ex));
        i_foo_proxy proxy(op);
        standard_tests(proxy, true);
    }
    return 0;
}

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz) { puts(str); }
}