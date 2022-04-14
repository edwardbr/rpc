#include <iostream>

#include <sgx_urts.h>
#include <sgx_quote.h>
#include <sgx_capable.h>
#include <sgx_uae_epid.h>
#include <sgx_eid.h>
#include "untrusted/enclave_marshal_test_u.h"

#include <common/foo_impl.h>
#include <common/tests.h>
#include <common/enclave_service_proxy.h>

#include <example/example.h>
#include <example_proxy.cpp>
#include <example_stub.cpp>

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

    {
        error_code ret = 77;
        // STUB_ADD_REF_OUT
        uint64_t target_ = 66;
        // STUB_MARSHALL_OUT

        std::vector<char> out_buf_;

        const auto yas_mapping_ = YAS_OBJECT_NVP("out", ("_1", ret), ("_2", target_));
        yas::count_ostream counter_;
        yas::binary_oarchive<yas::count_ostream, yas::mem | yas::binary> oa(counter_);
        oa(yas_mapping_);
        out_buf_.resize(counter_.total_size);
        yas::mem_ostream writer_(out_buf_.data(), counter_.total_size);
        yas::save<yas::mem | yas::binary>(writer_, yas_mapping_);

        uint64_t new_target_ = 0;
        error_code new_ret_ = 0;
        yas::load<yas::mem | yas::binary>(yas::intrusive_buffer {out_buf_.data(), out_buf_.size()},
                                          YAS_OBJECT_NVP("out", ("_1", new_ret_), ("_2", new_target_)));
    }

    // an inprocess marshalling of an object
    do
    {
        auto service = rpc::make_shared<rpc::service>(1);

        auto marshaller = rpc::static_pointer_cast<rpc::i_marshaller>(service);

        auto other_service = rpc::make_shared<rpc::service>(2);
        // create the root object
        rpc::shared_ptr<yyy::i_example> ex(new example);
        error_code err_code = other_service->initialise<yyy::i_example, yyy::i_example_stub>(ex);
        if (err_code)
            return err_code;

        auto other_marshaller = rpc::static_pointer_cast<rpc::i_marshaller>(other_service);

        // create a proxy for the rpc::service hosting the example object
        auto service_proxy
            = rpc::local_service_proxy::create(service, 2, other_marshaller, other_service->get_root_object_id());
        auto example_ptr = service_proxy->get_interface<yyy::i_example>();

        // create a proxy for the other rpc::service, keep an instance going
        auto other_service_proxy = rpc::local_service_proxy::create(other_service, 1, marshaller, 0);

        rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
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

        auto ex = rpc::enclave_service_proxy::create(
            host_rpc_server, 2,
            "C:/Dev/experiments/enclave_marshaller/build/output/debug/marshal_test_enclave.signed.dll");

        err_code = ex->initialise();
        ASSERT(!err_code);

        auto example_ptr = ex->get_interface<yyy::i_example>();

        rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
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
                  size_t sz_out, char* data_out, size_t* data_out_sz)
    {
        thread_local std::vector<char> out_buf;
        error_code ret = 0;
        if (out_buf.empty())
        {
            ret = current_host_service->send(object_id, interface_id, method_id, sz_int, data_in, out_buf);
        }
        if (ret == 0)
        {
            *data_out_sz = out_buf.size();
            if (out_buf.size() <= sz_out)
            {
                memcpy(data_out, out_buf.data(), out_buf.size());
                out_buf.clear();
                return 0;
            }
            return -2;
        }
        return ret;
    }
    int try_cast_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        error_code ret = current_host_service->try_cast(zone_id, object_id, interface_id);
        return ret;
    }
    uint64_t add_ref_host(uint64_t zone_id, uint64_t object_id)
    {
        return current_host_service->add_ref(zone_id, object_id);
    }
    uint64_t release_host(uint64_t zone_id, uint64_t object_id)
    {
        return current_host_service->release(zone_id, object_id);
    }
}