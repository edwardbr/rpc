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

#include <example_shared_proxy.cpp>
#include <example_shared_stub.cpp>

#include <example_import_proxy.cpp>
#include <example_import_stub.cpp>

#include <example_proxy.cpp>
#include <example_stub.cpp>

#include <rpc/basic_service_proxies.h>

using namespace marshalled_tests;

rpc::weak_ptr<rpc::service> current_host_service;

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
        int zone_gen = 0;
        auto root_service = rpc::make_shared<rpc::service>(++zone_gen);
        auto branch_service = rpc::make_shared<rpc::child_service>(++zone_gen);
        {
            uint64_t stub_id = 0;

            // create a proxy for the rpc::service hosting the example object
            auto service_proxy = rpc::root_service_proxy::create(root_service);
            // create a proxy for the remote rpc::service, keep an instance going
            auto branch_service_proxy = rpc::branch_service_proxy::create(branch_service, root_service);
            branch_service->add_zone_proxy(rpc::static_pointer_cast<rpc::service_proxy>(service_proxy));
            {
                {
                    // create the remote root object
                    rpc::shared_ptr<yyy::i_example> remote_ex(new example);
                    branch_service->create_stub<yyy::i_example, yyy::i_example_stub>(remote_ex, &stub_id);
                }

                rpc::shared_ptr<yyy::i_example> i_example_ptr;
                ASSERT(!branch_service_proxy->create_proxy(stub_id, i_example_ptr));

                rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
                int err_code = i_example_ptr->create_foo(i_foo_ptr);
                if (err_code)
                {
                    std::cout << "create_foo failed\n";
                    break;
                }

                ASSERT(!i_foo_ptr->do_something_in_val(33));
                standard_tests(*i_foo_ptr, true);

                remote_tests(i_example_ptr);
            }
        }
    } while (0);

    // an enclave marshalling of an object
    {
        int err_code = rpc::error::OK();
        int zone_gen = 0;
        auto root_service = rpc::make_shared<rpc::service>(++zone_gen);
        current_host_service = root_service;

        {
            rpc::shared_ptr<yyy::i_example> example_ptr;
            err_code = rpc::enclave_service_proxy::create(++zone_gen, "./marshal_test_enclave.signed.dll", root_service, example_ptr);
            ASSERT(!err_code);

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

            // relay test

            rpc::shared_ptr<yyy::i_example> example_relay_ptr;
            err_code = rpc::enclave_service_proxy::create(++zone_gen, "./marshal_test_enclave.signed.dll", root_service, example_relay_ptr);
            ASSERT(!err_code);

            rpc::shared_ptr<marshalled_tests::xxx::i_baz> i_baz;
            i_foo_ptr->create_baz_interface(i_baz);

            rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
            example_relay_ptr->create_foo(i_foo_relay_ptr);

            i_foo_relay_ptr->call_baz_interface(i_baz);
        }
    }
    return 0;
}

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz) { puts(str); }

    int call_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t sz_int,
                  const char* data_in, size_t sz_out, char* data_out, size_t* data_out_sz)
    {
        thread_local std::vector<char> out_buf;
        int ret = 0;

        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        if (out_buf.empty())
        {
            ret = root_service->send(zone_id, object_id, interface_id, method_id, sz_int, data_in, out_buf);
        }
        if (ret == rpc::error::OK())
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
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        int ret = root_service->try_cast(zone_id, object_id, interface_id);
        return ret;
    }
    uint64_t add_ref_host(uint64_t zone_id, uint64_t object_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->add_ref(zone_id, object_id);
    }
    uint64_t release_host(uint64_t zone_id, uint64_t object_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->release(zone_id, object_id);
    }
}