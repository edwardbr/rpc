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
#include <spdlog/spdlog.h>

using namespace marshalled_tests;

rpc::weak_ptr<rpc::service> current_host_service;

class host_telemetry_service : public i_telemetry_service
{
    struct name_count
    {
        std::string name;
        uint64_t count = 0;
    };

    using zone_object = std::pair<uint64_t,uint64_t>;

    struct zone_object_hash
    {
        std::size_t operator()(zone_object const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.first);
            std::size_t h2 = std::hash<uint64_t>{}(s.second);
            return h1 ^ (h2 << 1); // or use boost::hash_combine
        }
    };

    struct interface_proxy_id
    {
        interface_proxy_id() = default;
        interface_proxy_id(const interface_proxy_id&) = default;
        interface_proxy_id(interface_proxy_id&&) = default;

        bool operator == (const interface_proxy_id& other) const
        {
        return  originating_zone_id == other.originating_zone_id &&
                zone_id == other.zone_id &&
                object_id == other.object_id;
        }
        uint64_t originating_zone_id = 0;
        uint64_t zone_id = 0;
        uint64_t object_id = 0;
    };

    struct interface_proxy_id_hash
    {
        std::size_t operator()(interface_proxy_id const& s) const noexcept
        {
            std::size_t h1 = std::hash<uint64_t>{}(s.originating_zone_id);
            std::size_t h2 = std::hash<uint64_t>{}(s.zone_id);
            std::size_t h3 = std::hash<uint64_t>{}(s.object_id);
            return h1 ^ (h2 << 1) ^ (h3 << 2); // or use boost::hash_combine
        }
    };

    mutable std::mutex mux;
    mutable std::unordered_map<uint64_t, name_count> services;
    mutable std::unordered_map<uint64_t, name_count> service_proxies;
    mutable std::unordered_map<zone_object, name_count, zone_object_hash> impls;
    mutable std::unordered_map<zone_object, name_count, zone_object_hash> stubs;
    mutable std::unordered_map<interface_proxy_id, name_count, interface_proxy_id_hash> interface_proxies;
    mutable std::unordered_map<interface_proxy_id, uint64_t, interface_proxy_id_hash> object_proxies;


public:
    virtual ~host_telemetry_service()
    {
        spdlog::info("orphaned services {}", services.size());
        spdlog::info("orphaned service_proxies {}", service_proxies.size());
        spdlog::info("orphaned impls {}", impls.size());
        spdlog::info("orphaned stubs {}", stubs.size());
        spdlog::info("orphaned interface_proxies {}", interface_proxies.size());
        spdlog::info("orphaned object_proxies {}", object_proxies.size());

        std::for_each(services.begin(), services.end(), [](std::pair<uint64_t, name_count> const& it){spdlog::warn("service {} zone {} count {}", it.second.name, it.first, it.second.count);});
        std::for_each(service_proxies.begin(), service_proxies.end(), [](std::pair<uint64_t, name_count> const& it){spdlog::warn("service_proxies {} zone {} count {}", it.second.name, it.first, it.second.count);});
        std::for_each(impls.begin(), impls.end(), [](std::pair<zone_object, name_count> const& it){spdlog::warn("impls {} zone {} object_id {} count {}", it.second.name, it.first.first, it.first.second, it.second.count);});
        std::for_each(stubs.begin(), stubs.end(), [](std::pair<zone_object, name_count> const& it){spdlog::warn("stubs {} zone {} object_id {} count {}", it.second.name, it.first.first, it.first.second, it.second.count);});
        std::for_each(object_proxies.begin(), object_proxies.end(), [](std::pair<interface_proxy_id, uint64_t> const& it){spdlog::warn("object_proxies originating_zone {} zone {} object_id {} count {}", it.first.originating_zone_id, it.first.zone_id, it.first.object_id, it.second);});
        std::for_each(interface_proxies.begin(), interface_proxies.end(), [](std::pair<interface_proxy_id, name_count> const& it){spdlog::warn("interface_proxies {} originating_zone {} zone {} object_id {} count {}", it.second.name, it.first.originating_zone_id, it.first.zone_id, it.first.object_id, it.second.count);});


        bool is_heathy = services.empty() && service_proxies.empty() && impls.empty() && stubs.empty() && interface_proxies.empty() && object_proxies.empty();
        if(is_heathy)
        {
            spdlog::info("system is healthy");
        }
        else
        {
            spdlog::error("system is NOT healthy!");
        }
    }

    virtual void on_service_creation(const char* name, uint64_t zone_id) const
    {
        std::lock_guard g(mux);
        services.emplace(zone_id, name_count{name, 1});
        spdlog::info("on_service_creation {} {}", name, zone_id);
    }

    virtual void on_service_deletion(const char* name, uint64_t zone_id) const
    {
        std::lock_guard g(mux);
        auto found = services.find(zone_id);
        if(found == services.end())
        {
            spdlog::error("service not found {} {}", name, zone_id);
        }
        else if(found->second.count == 1)
        {
            services.erase(found);
            spdlog::info("on_service_deletion {} {}", name, zone_id);
        }
        else
        {
            
            found->second.count--;
            spdlog::error("service still being used!{} {}", name, zone_id);
            spdlog::info("on_service_deletion {} {}", name, zone_id);
        }
    }
    virtual void on_service_proxy_creation(const char* name, uint64_t zone_id) const
    {
        std::lock_guard g(mux);
        service_proxies.emplace(zone_id, name_count{name, 1});
        spdlog::info("on_service_proxy_creation {} {}", name, zone_id);
    }
    virtual void on_service_proxy_deletion(const char* name, uint64_t zone_id) const
    {
        std::lock_guard g(mux);
        auto found = service_proxies.find(zone_id);
        if(found == service_proxies.end())
        {
            spdlog::error("service_proxy not found {} {}", name, zone_id);
        }
        else if(found->second.count == 1)
        {
            service_proxies.erase(found);
            spdlog::info("on_service_proxy_deletion {} {}", name, zone_id);
        }
        else
        {
            
            found->second.count--;
            spdlog::error("service still being used!{} {}", name, zone_id);
            spdlog::info("on_service_proxy_deletion {} {}", name, zone_id);
        }        
    }
    virtual void on_service_proxy_try_cast(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        spdlog::info("on_service_proxy_try_cast {} {} {} {} {}", name, originating_zone_id, zone_id, object_id, interface_id);
    }
    virtual void on_service_proxy_add_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        spdlog::info("on_service_proxy_add_ref {} {} {} {}", name, originating_zone_id, zone_id, object_id);
    }
    virtual void on_service_proxy_release(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        spdlog::info("on_service_proxy_release {} {} {} {}", name, originating_zone_id, zone_id, object_id);
    }

    virtual void on_impl_creation(const char* name, uint64_t zone_id, uint64_t object_id) const
    {
        std::lock_guard g(mux);
        impls.emplace(zone_object{zone_id, object_id}, name_count{name, 1});
        spdlog::info("on_impl_creation {} {} {}", name, zone_id, object_id);
    }
    virtual void on_impl_deletion(const char* name, uint64_t zone_id, uint64_t object_id) const
    {
        std::lock_guard g(mux);
        auto found = impls.find(zone_object{zone_id, object_id});
        if(found == impls.end())
        {
            spdlog::error("impl not found {} {}", name, zone_id);
        }
        else if(found->second.count == 1)
        {
            impls.erase(found);
            spdlog::info("on_impl_deletion {} {}", name, zone_id);
        }
        else
        {            
            found->second.count--;
            spdlog::error("impl still being used!{} {}", name, zone_id);
            spdlog::info("on_impl_deletion {} {}", name, zone_id);
        }
    }

    virtual void on_stub_creation(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        std::lock_guard g(mux);
        stubs.emplace(zone_object{zone_id, object_id}, name_count{name, 1});
        spdlog::info("on_stub_creation {} {} {} {}", name, zone_id, object_id, interface_id);
    }
    virtual void on_stub_deletion(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        std::lock_guard g(mux);
        auto found = stubs.find(zone_object{zone_id, object_id});
        if(found == stubs.end())
        {
            spdlog::error("stub not found {} {}", name, zone_id);
        }
        else if(found->second.count == 1)
        {
            stubs.erase(found);
            spdlog::info("on_stub_deletion {} {}", name, zone_id);
        }
        else
        {            
            found->second.count--;
            spdlog::error("stub still being used! {} {}", name, zone_id);
            spdlog::info("on_stub_deletion {} {}", name, zone_id);
        }
    }
    virtual void on_stub_send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const
    {
        spdlog::info("on_stub_send {} {} {} {}", zone_id, object_id, interface_id, method_id);
    }
    virtual void on_stub_add_ref(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const
    {
        spdlog::info("on_stub_add_ref {} {} {} {}", zone_id, object_id, interface_id, count);
    }
    virtual void on_stub_release(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const
    {
        spdlog::info("on_stub_release {} {} {} {}", zone_id, object_id, interface_id, count);
    }

    virtual void on_object_proxy_creation(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        std::lock_guard g(mux);
        object_proxies.emplace(interface_proxy_id{originating_zone_id, zone_id, object_id}, 1);
        spdlog::info("on_object_proxy_creation {} {}", zone_id, object_id);
    }
    virtual void on_object_proxy_deletion(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const
    {
        std::lock_guard g(mux);
        auto found = object_proxies.find(interface_proxy_id{originating_zone_id, zone_id, object_id});
        if(found == object_proxies.end())
        {
            spdlog::error("object proxy not found {} {}", object_id, zone_id);
        }
        else if(found->second == 1)
        {
            object_proxies.erase(found);
            spdlog::info("on_object_proxy_deletion {} {}", object_id, zone_id);
        }
        else
        {            
            found->second--;
            spdlog::error("object proxy still being used! {} {}", object_id, zone_id);
            spdlog::info("on_object_proxy_deletion {} {}", object_id, zone_id);
        }
    }

    virtual void on_interface_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        std::lock_guard g(mux);
        interface_proxies.emplace(interface_proxy_id{originating_zone_id, zone_id, object_id}, name_count{name, 1});
        spdlog::info("on_interface_proxy_creation {} {} {} {} {}", name, originating_zone_id, zone_id, object_id, interface_id);
    }
    virtual void on_interface_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const
    {
        std::lock_guard g(mux);
        auto found = interface_proxies.find(interface_proxy_id{originating_zone_id, zone_id, object_id});
        if(found == interface_proxies.end())
        {
            spdlog::error("interface proxy not found {} {}", name, zone_id);
        }
        else if(found->second.count == 1)
        {
            interface_proxies.erase(found);
            spdlog::info("on_interface_proxy_deletion {} {}", name, zone_id);
        }
        else
        {            
            found->second.count--;
            spdlog::error("interface proxy still being used! {} {}", name, zone_id);
            spdlog::info("on_interface_proxy_deletion {} {}", name, zone_id);
        }
    }
    virtual void on_interface_proxy_send(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const
    {
        spdlog::info("on_interface_proxy_send {} {} {} {} {} {}", name, originating_zone_id, zone_id, object_id, interface_id, method_id);
    }

    virtual void message(level_enum level, const char* message) const
    {
        spdlog::log((spdlog::level::level_enum)level, "message {}", message);
    }
};

const i_telemetry_service* telemetry_service = nullptr;

int main()
{
    // conventional c++ object on stack
 /*   {
        host_telemetry_service tm;
        telemetry_service = &tm;
        {
            foo f;
            standard_tests(f, false);
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
                        rpc::shared_ptr<yyy::i_example> remote_ex(new example);
                        branch_service->create_stub<yyy::i_example, yyy::i_example_stub>(remote_ex, &stub_id);
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
                    standard_tests(*i_foo_ptr, true);

                    remote_tests(i_example_ptr);
                }
            }
        }
        telemetry_service = nullptr;
    } while (0);*/

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
                err_code = rpc::enclave_service_proxy::create(++zone_gen, "./marshal_test_enclave.signed.dll", root_service, example_ptr, telemetry_service);
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

                /*rpc::shared_ptr<yyy::i_example> example_relay_ptr;
                err_code = rpc::enclave_service_proxy::create(++zone_gen, "./marshal_test_enclave.signed.dll", root_service, example_relay_ptr, telemetry_service);
                ASSERT(!err_code);

                rpc::shared_ptr<marshalled_tests::xxx::i_baz> i_baz;
                i_foo_ptr->create_baz_interface(i_baz);

                rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
                example_relay_ptr->create_foo(i_foo_relay_ptr);

                i_foo_relay_ptr->call_baz_interface(i_baz);*/
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
    void on_service_proxy_creation_host(const char* name, uint64_t zone_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_creation(name, zone_id);
    }
    void on_service_proxy_deletion_host(const char* name, uint64_t zone_id)
    {
        if(telemetry_service)
            telemetry_service->on_service_proxy_deletion(name, zone_id);
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

    void on_impl_creation_host(const char* name, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_impl_creation(name, object_id, interface_id);
    }
    void on_impl_deletion_host(const char* name, uint64_t object_id, uint64_t interface_id)
    {
        if(telemetry_service)
            telemetry_service->on_impl_deletion(name, object_id, interface_id);
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
            telemetry_service->message((i_telemetry_service::level_enum)level, name);
    }  

}