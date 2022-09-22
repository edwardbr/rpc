#include <iostream>
#include <unordered_map>

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
#include <host_telemetry_service.h>

using namespace marshalled_tests;

rpc::weak_ptr<rpc::service> current_host_service;

const rpc::i_telemetry_service* telemetry_service = nullptr;

int main()
{
    // conventional c++ object on stack
    {
        host_telemetry_service tm;
        telemetry_service = &tm;
        {
            foo f(telemetry_service);
            standard_tests(f, false, telemetry_service);
        }
        telemetry_service = nullptr;
    }

    // an inprocess marshalling of an object
    do
    {
        host_telemetry_service tm;
        telemetry_service = &tm;
        {
            int zone_gen = 0;
            auto root_service = rpc::make_shared<rpc::service>(++zone_gen);
            auto branch_service = rpc::make_shared<rpc::child_service>(++zone_gen);
            {
                uint64_t stub_id = 0;

                // create a proxy for the rpc::service hosting the example object
                auto service_proxy = rpc::local_service_proxy::create(root_service, telemetry_service);
                // create a proxy for the remote rpc::service, keep an instance going
                auto local_child_service_proxy = rpc::local_child_service_proxy::create(branch_service, root_service, telemetry_service);
                branch_service->add_zone_proxy(rpc::static_pointer_cast<rpc::service_proxy>(service_proxy));
                {
                    {
                        // create the remote root object
                        rpc::shared_ptr<yyy::i_example> remote_ex(new example(telemetry_service));
                        branch_service->create_stub<yyy::i_example, yyy::i_example_stub>(remote_ex, &stub_id);

                        //simple test to check that we can get a usefule local interface based on type and object id
                        auto example_from_cast = branch_service->get_local_interface<yyy::i_example>(stub_id);
                        assert(example_from_cast == remote_ex);
                    }

                    rpc::shared_ptr<yyy::i_example> i_example_ptr;
                    ASSERT(!local_child_service_proxy->create_proxy(stub_id, i_example_ptr));

                    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
                    int err_code = i_example_ptr->create_foo(i_foo_ptr);
                    if (err_code)
                    {
                        std::cout << "create_foo failed\n";
                        break;
                    }

                    ASSERT(!i_foo_ptr->do_something_in_val(33));
                    standard_tests(*i_foo_ptr, true, telemetry_service);

                    remote_tests(i_example_ptr, telemetry_service);
                }
            }
        }
        telemetry_service = nullptr;
    } while (0);

    // an enclave marshalling of an object
    {
        host_telemetry_service tm;
        telemetry_service = &tm;
        {
            int err_code = rpc::error::OK();
            int zone_gen = 0;
            auto root_service = rpc::make_shared<rpc::service>(++zone_gen);
            current_host_service = root_service;

            {
                rpc::shared_ptr<yyy::i_example> example_ptr;
                #ifdef _WIN32 // windows
                err_code = rpc::enclave_service_proxy::create(++zone_gen, "./marshal_test_enclave.signed.dll", root_service, example_ptr, telemetry_service);
                #else // Linux
                err_code = rpc::enclave_service_proxy::create(++zone_gen, "./libmarshal_test_enclave.signed.so", root_service, example_ptr, telemetry_service);
                #endif
                ASSERT(!err_code);

                rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
                err_code = example_ptr->create_foo(i_foo_ptr);
                if (err_code)
                {
                    std::cout << "create_foo failed\n";
                }
                else
                {
                    standard_tests(*i_foo_ptr, true, telemetry_service);

                    remote_tests(example_ptr, telemetry_service);
                }

                // relay test

                rpc::shared_ptr<yyy::i_example> example_relay_ptr;
                #ifdef _WIN32 // windows
                err_code = rpc::enclave_service_proxy::create(++zone_gen, "./marshal_test_enclave.signed.dll", root_service, example_relay_ptr, telemetry_service);
                #else // Linux
                err_code = rpc::enclave_service_proxy::create(++zone_gen, "./libmarshal_test_enclave.signed.so", root_service, example_relay_ptr, telemetry_service);
                #endif
                ASSERT(!err_code);

                rpc::shared_ptr<marshalled_tests::xxx::i_baz> i_baz;
                i_foo_ptr->create_baz_interface(i_baz);

                rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
                example_relay_ptr->create_foo(i_foo_relay_ptr);

                i_foo_relay_ptr->call_baz_interface(i_baz);
            }
        }
        telemetry_service = nullptr;
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




    void on_service_creation_host(const char* name, uint64_t zone_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_creation(name, zone_id);
    }
    void on_service_deletion_host(const char* name, uint64_t zone_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_deletion(name, zone_id);
    }
    void on_service_proxy_creation_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_creation(name, originating_zone_id, zone_id);
    }
    void on_service_proxy_deletion_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_deletion(name, originating_zone_id, zone_id);
    }
    void on_service_proxy_try_cast_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_try_cast(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_service_proxy_add_ref_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_add_ref(name, originating_zone_id, zone_id, object_id);
    }
    void on_service_proxy_release_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_release(name, originating_zone_id, zone_id, object_id);
    }  

    void on_impl_creation_host(const char* name, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_impl_creation(name, interface_id);
    }
    void on_impl_deletion_host(const char* name, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_impl_deletion(name, interface_id);
    }

    void on_stub_creation_host(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_stub_creation(name, zone_id, object_id, interface_id);
    }    
    void on_stub_deletion_host(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_stub_deletion(name, zone_id, object_id, interface_id);
    }
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if(telemetry_service)
            telemetry_service->on_stub_send(zone_id, object_id, interface_id, method_id);
    }
    void on_stub_add_ref_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count)
    {
        if(telemetry_service)
            telemetry_service->on_stub_add_ref(zone_id, object_id, interface_id, count);
    }
    void on_stub_release_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count)
    {
        if(telemetry_service)
            telemetry_service->on_stub_release(zone_id, object_id, interface_id, count);
    }

    void on_object_proxy_creation_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if(telemetry_service)
            telemetry_service->on_object_proxy_creation(originating_zone_id, zone_id, object_id);
    }
    void on_object_proxy_deletion_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if(telemetry_service)
            telemetry_service->on_object_proxy_deletion(originating_zone_id, zone_id, object_id);
    }

    void on_proxy_creation_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_interface_proxy_creation(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_proxy_deletion_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_interface_proxy_deletion(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_proxy_send_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if(telemetry_service)
            telemetry_service->on_interface_proxy_send(name, originating_zone_id, zone_id, object_id, interface_id, method_id);
    }

    void message_host(uint64_t level, const char* name)
    {
        if(telemetry_service)
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }  

}