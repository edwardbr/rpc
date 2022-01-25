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

error_code i_marshaller_impl::send(uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_, const char* in_buf_, size_t out_size_, char* out_buf_)
{
    error_code err_code = 0;
    sgx_status_t status = call(eid_, &err_code, object_id, interface_id, method_id, in_size_, in_buf_, out_size_, out_buf_);
    if(status)
        err_code = -1;
    return err_code;
}
error_code i_marshaller_impl::try_cast(i_unknown& from, uint64_t interface_id, remote_shared_ptr<i_unknown>& to)
{
    return 1;
}

int main()
{
    //conventional c++ object on stack
    {
        foo f;
        standard_tests(f, false);
    }

    //an inprocess marshalling of an object
    {
        i_foo_stub stub(remote_shared_ptr<i_foo>(new foo()));
        i_foo_proxy proxy(stub, 0);
        i_foo& foo = proxy;
        standard_tests(foo, false);
    }

    //an enclave marshalling of an object
    {
        error_code err_code = 0;
        example ex("C:/Dev/experiments/enclave_marshaller/build/output/debug/marshal_test_enclave.signed.dll");
        err_code = ex.load();
        ASSERT(err_code);

        /*remote_shared_ptr<marshalled_tests::i_foo> target;
        err_code = ex.create_foo(target);
        if(!err_code)
            std::cout << "aggggggg!";*/


        //work in progress create_foo should be passing back an instance of foo
        i_foo_proxy proxy(ex, 1);
        standard_tests(proxy, true);
    }
    return 0;
}

//an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz)
    {
        puts(str);
    }
}