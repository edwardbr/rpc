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

#ifdef _WIN32 // windows
auto enclave_path = "./marshal_test_enclave.signed.dll";
#else         // Linux
auto enclave_path = "./libmarshal_test_enclave.signed.so"
#endif

rpc::weak_ptr<rpc::service> current_host_service;

const rpc::i_telemetry_service* telemetry_service = nullptr;
int zone_gen = 0;

std::vector<rpc::shared_ptr<yyy::i_example>> cached;

class host : public yyy::i_host
{
    void* get_address() const override { return (void*)this; }
    const rpc::casting_interface* query_interface(uint64_t interface_id) const override
    {
        if (yyy::i_host::id == interface_id)
            return static_cast<const yyy::i_host*>(this);
        return nullptr;
    }
    error_code create_enclave(rpc::shared_ptr<yyy::i_example>& target) override
    {
        auto err_code = rpc::enclave_service_proxy::create(++zone_gen, enclave_path, current_host_service.lock(),
                                                           target, telemetry_service);
        // cached.push_back(target);
        return err_code;
    };
};

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
            auto root_service = rpc::make_shared<rpc::service>(++zone_gen);
            auto child_service = rpc::make_shared<rpc::child_service>(++zone_gen);
            {
                rpc::interface_descriptor host_encap {};
                rpc::interface_descriptor example_encap {};

                // create a proxy to the rpc::service hosting the child service
                auto service_proxy_to_host
                    = rpc::local_service_proxy::create(root_service, child_service, telemetry_service);

                // create a proxy to the rpc::service that contains the example object
                auto service_proxy_to_child
                    = rpc::local_child_service_proxy::create(child_service, root_service, telemetry_service);

                // give the child a link to the service
                child_service->add_zone_proxy(rpc::static_pointer_cast<rpc::service_proxy>(service_proxy_to_host));

                {
                    {
                        // create a host implementation object
                        rpc::shared_ptr<yyy::i_host> hst(new host());

                        // register implementation to the root service and held by a stub
                        // note! There is a memory leak if the interface_descriptor object is not bound to a proxy
                        host_encap = rpc::create_interface_stub(*root_service, hst);

                        // simple test to check that we can get a useful local interface based on type and object id
                        auto example_from_cast = root_service->get_local_interface<yyy::i_host>(host_encap.object_id);
                        assert(example_from_cast == hst);
                    }

                    {
                        // create the example object implementation
                        rpc::shared_ptr<yyy::i_example> remote_example(new example(telemetry_service));

                        example_encap
                            = rpc::create_interface_stub(*child_service, remote_example);

                        // simple test to check that we can get a usefule local interface based on type and object id
                        auto example_from_cast
                            = child_service->get_local_interface<yyy::i_example>(example_encap.object_id);
                        assert(example_from_cast == remote_example);
                    }

                    rpc::shared_ptr<yyy::i_host> i_host_ptr;
                    ASSERT(!rpc::create_interface_proxy(service_proxy_to_host, host_encap, i_host_ptr));

                    rpc::shared_ptr<yyy::i_example> i_example_ptr;
                    ASSERT(!rpc::create_interface_proxy(service_proxy_to_child, example_encap, i_example_ptr));

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
            auto root_service = rpc::make_shared<rpc::service>(++zone_gen);
            current_host_service = root_service;

            {
                rpc::shared_ptr<yyy::i_example> example_ptr;
                err_code = rpc::enclave_service_proxy::create(++zone_gen, enclave_path, root_service, example_ptr,
                                                              telemetry_service);
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
                err_code = rpc::enclave_service_proxy::create(++zone_gen, enclave_path, root_service, example_relay_ptr,
                                                              telemetry_service);

                {
                    rpc::shared_ptr<marshalled_tests::xxx::i_baz> baz;
                    i_foo_ptr->create_baz_interface(baz);
                    i_foo_ptr->call_baz_interface(nullptr); // feed in a nullptr
                    i_foo_ptr->call_baz_interface(baz);     // feed back to the implementation

                    auto x = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_baz>(baz);
                    auto y = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_bar>(baz);
                    y->do_something_else(1);
                    auto z = rpc::dynamic_pointer_cast<marshalled_tests::xxx::i_foo>(baz);

                    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
                    example_relay_ptr->create_foo(i_foo_relay_ptr);

                    i_foo_relay_ptr->call_baz_interface(baz);
                }
                {
                    rpc::shared_ptr<marshalled_tests::xxx::i_baz> c;
                    // check for null
                    i_foo_ptr->get_interface(c);
                    assert(c == nullptr);
                }
                {
                    auto b = rpc::make_shared<marshalled_tests::baz>(telemetry_service);
                    // set
                    i_foo_ptr->set_interface(b);
                    // reset
                    i_foo_ptr->set_interface(nullptr);
                    // set
                    i_foo_ptr->set_interface(b);
                    // reset
                    i_foo_ptr->set_interface(nullptr);
                }
                {
                    rpc::shared_ptr<marshalled_tests::xxx::i_baz> c;
                    auto b = rpc::make_shared<marshalled_tests::baz>(telemetry_service);
                    i_foo_ptr->set_interface(b);
                    i_foo_ptr->get_interface(c);
                    i_foo_ptr->set_interface(nullptr);
                    assert(b == c);
                }
                {
                    auto ret = example_ptr->give_interface(
                        rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(telemetry_service)));
                    assert(ret == rpc::error::OK());
                    cached.clear();
                }
                {
                    auto h = rpc::make_shared<host>();
                    auto ret = example_ptr->call_create_enclave_val(h);
                    assert(ret == rpc::error::OK());
                    cached.clear();
                }
            }
        }
        telemetry_service = nullptr;
    }
    return 0;
}

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz)
    {
        puts(str);
    }

    int call_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id,
                  uint64_t method_id, size_t sz_int, const char* data_in, size_t sz_out, char* data_out,
                  size_t* data_out_sz)
    {
        thread_local std::vector<char> out_buf;

        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            out_buf.clear();
            return rpc::error::TRANSPORT_ERROR();
        }
        if (out_buf.empty())
        {
            int ret = root_service->send(originating_zone_id, zone_id, object_id, interface_id, method_id, sz_int,
                                         data_in, out_buf);
            if (ret >= rpc::error::MIN() && ret <= rpc::error::MAX())
                return ret;
        }
        *data_out_sz = out_buf.size();
        if (*data_out_sz > sz_out)
            return rpc::error::NEED_MORE_MEMORY();
        memcpy(data_out, out_buf.data(), out_buf.size());
        out_buf.clear();
        return rpc::error::OK();
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
        if (telemetry_service)
            telemetry_service->on_service_creation(name, zone_id);
    }
    void on_service_deletion_host(const char* name, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_deletion(name, zone_id);
    }
    void on_service_proxy_creation_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_creation(name, originating_zone_id, zone_id);
    }
    void on_service_proxy_deletion_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_deletion(name, originating_zone_id, zone_id);
    }
    void on_service_proxy_try_cast_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                        uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_try_cast(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_service_proxy_add_ref_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                       uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_ref(name, originating_zone_id, zone_id, object_id);
    }
    void on_service_proxy_release_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                       uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release(name, originating_zone_id, zone_id, object_id);
    }

    void on_impl_creation_host(const char* name, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_creation(name, interface_id);
    }
    void on_impl_deletion_host(const char* name, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_deletion(name, interface_id);
    }

    void on_stub_creation_host(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_creation(name, zone_id, object_id, interface_id);
    }
    void on_stub_deletion_host(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_deletion(name, zone_id, object_id, interface_id);
    }
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_send(zone_id, object_id, interface_id, method_id);
    }
    void on_stub_add_ref_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count)
    {
        if (telemetry_service)
            telemetry_service->on_stub_add_ref(zone_id, object_id, interface_id, count);
    }
    void on_stub_release_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count)
    {
        if (telemetry_service)
            telemetry_service->on_stub_release(zone_id, object_id, interface_id, count);
    }

    void on_object_proxy_creation_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_creation(originating_zone_id, zone_id, object_id);
    }
    void on_object_proxy_deletion_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_deletion(originating_zone_id, zone_id, object_id);
    }

    void on_proxy_creation_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id,
                                uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_creation(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_proxy_deletion_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id,
                                uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_deletion(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_proxy_send_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id,
                            uint64_t interface_id, uint64_t method_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_send(name, originating_zone_id, zone_id, object_id, interface_id,
                                                       method_id);
    }

    void on_service_proxy_add_external_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                           int ref_count)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_external_ref(name, originating_zone_id, zone_id, ref_count);
    }
    void on_service_proxy_release_external_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                               int ref_count)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release_external_ref(name, originating_zone_id, zone_id, ref_count);
    }

    void message_host(uint64_t level, const char* name)
    {
        if (telemetry_service)
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }
}