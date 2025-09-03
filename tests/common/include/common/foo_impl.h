/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <example/example.h>
#include <rpc/types.h>
#include <rpc/proxy.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif
#include <rpc/basic_service_proxies.h>
#include <rpc/logger.h>

#include <example_shared/example_shared_stub.h>
#include <example_import/example_import_stub.h>
#include <example/example_stub.h>


namespace marshalled_tests
{
    class baz : public xxx::i_baz, public xxx::i_bar
    {
        rpc::zone zone_id_;
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<xxx::i_baz>(interface_id))
                return static_cast<const xxx::i_baz*>(this);
            if (rpc::match<xxx::i_bar>(interface_id))
                return static_cast<const xxx::i_bar*>(this);
            return nullptr;
        }

    public:
        baz(rpc::zone zone_id)
            : zone_id_(zone_id)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("baz", (uint64_t)this, zone_id_);
#endif
        }

        virtual ~baz()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, zone_id_);
#endif
        }
        int callback(int val) override
        {
            RPC_INFO("callback {}", val);
            return rpc::error::OK();
        }
        error_code blob_test(const std::vector<uint8_t>& inval, std::vector<uint8_t>& out_val) override
        {
            RPC_INFO("baz blob_test {}", inval.size());
            out_val = inval;
            return rpc::error::OK();
        }
        error_code do_something_else(int val) override
        {
            RPC_INFO("baz do_something_else");
            return rpc::error::OK();
        }
    };

    class foo : public xxx::i_foo
    {
        rpc::zone zone_id_;
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<xxx::i_foo>(interface_id))
                return static_cast<const xxx::i_foo*>(this);
            return nullptr;
        }

        rpc::member_ptr<xxx::i_baz> cached_;

    public:
        foo(rpc::zone zone_id)
            : zone_id_(zone_id)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("foo", (uint64_t)this, zone_id_);
#endif
        }
        virtual ~foo()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, zone_id_);
#endif
        }
        error_code do_something_in_val(int val) override
        {
            RPC_INFO("got {}", val);
            return rpc::error::OK();
        }
        error_code do_something_in_ref(const int& val) override
        {
            RPC_INFO("got {}", val);
            return rpc::error::OK();
        }
        error_code do_something_in_by_val_ref(const int& val) override
        {
            RPC_INFO("got {}", val);
            return rpc::error::OK();
        }
        error_code do_something_in_move_ref(int&& val) override
        {
            RPC_INFO("got {}", val);
            return rpc::error::OK();
        }
        error_code do_something_in_ptr(const int* val) override
        {
            RPC_INFO("got {}", *val);
            return rpc::error::OK();
        }
        error_code do_something_out_val(int& val) override
        {
            val = 33;
            return rpc::error::OK();
        };
        error_code do_something_out_ptr_ref(int*& val) override
        {
            val = new int(33);
            return rpc::error::OK();
        }
        error_code do_something_out_ptr_ptr(int** val) override
        {
            *val = new int(33);
            return rpc::error::OK();
        }
        error_code do_something_in_out_ref(int& val) override
        {
            RPC_INFO("got {}", val);
            val = 33;
            return rpc::error::OK();
        }
        error_code give_something_complicated_val(const xxx::something_complicated val) override
        {
            RPC_INFO("got {}", val.int_val);
            return rpc::error::OK();
        }
        error_code give_something_complicated_ref(const xxx::something_complicated& val) override
        {
            RPC_INFO("got {}", val.int_val);
            return rpc::error::OK();
        }
        error_code give_something_complicated_ref_val(const xxx::something_complicated& val) override
        {
            RPC_INFO("got {}", val.int_val);
            return rpc::error::OK();
        }
        error_code give_something_complicated_move_ref(xxx::something_complicated&& val) override
        {
            RPC_INFO("got {}", val.int_val);
            return rpc::error::OK();
        }
        error_code give_something_complicated_ptr(const xxx::something_complicated* val) override
        {
            RPC_INFO("got {}", val->int_val);
            return rpc::error::OK();
        }
        error_code receive_something_complicated_ref(xxx::something_complicated& val) override
        {
            val = xxx::something_complicated{33, "22"};
            return rpc::error::OK();
        }
        error_code receive_something_complicated_ptr(xxx::something_complicated*& val) override
        {
            val = new xxx::something_complicated{33, "22"};
            return rpc::error::OK();
        }
        error_code receive_something_complicated_in_out_ref(xxx::something_complicated& val) override
        {
            RPC_INFO("got {}", val.int_val);
            val.int_val = 33;
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_val(const xxx::something_more_complicated val) override
        {
            RPC_INFO("got {}", val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_ref(const xxx::something_more_complicated& val) override
        {
            RPC_INFO("got {}", val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_move_ref(xxx::something_more_complicated&& val) override
        {
            RPC_INFO("got {}", val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_ref_val(const xxx::something_more_complicated& val) override
        {
            RPC_INFO("got {}", val.map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code give_something_more_complicated_ptr(const xxx::something_more_complicated* val) override
        {
            RPC_INFO("got {}", val->map_val.begin()->first);
            return rpc::error::OK();
        }
        error_code receive_something_more_complicated_ref(xxx::something_more_complicated& val) override
        {
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            return rpc::error::OK();
        }
        error_code receive_something_more_complicated_ptr(xxx::something_more_complicated*& val) override
        {
            val = new xxx::something_more_complicated();
            val->map_val["22"] = xxx::something_complicated{33, "22"};
            return rpc::error::OK();
        }
        error_code receive_something_more_complicated_in_out_ref(xxx::something_more_complicated& val) override
        {
            if (val.map_val.size())
                RPC_INFO("got {}", val.map_val.begin()->first);
            else
                RPC_ASSERT(!"value is null");
            val.map_val["22"] = xxx::something_complicated{33, "23"};
            return rpc::error::OK();
        }
        error_code do_multi_val(int val1, int val2) override
        {
            RPC_INFO("got {}", val1);
            return rpc::error::OK();
        }
        error_code do_multi_complicated_val(
            const xxx::something_more_complicated val1, const xxx::something_more_complicated val2) override
        {
            RPC_INFO("got {}", val1.map_val.begin()->first);
            return rpc::error::OK();
        }

        error_code receive_interface(rpc::shared_ptr<i_foo>& val) override
        {
            val = rpc::shared_ptr<xxx::i_foo>(new foo(zone_id_));
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            return rpc::error::OK();
        }

        error_code give_interface(rpc::shared_ptr<xxx::i_baz> baz) override
        {
            baz->callback(22);
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
            return rpc::error::OK();
        }

        error_code call_baz_interface(const rpc::shared_ptr<xxx::i_baz>& val) override
        {
            if (!val)
                return rpc::error::OK();
            val->callback(22);
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_baz>(val);
            // #sgx dynamic cast in an enclave this fails
            auto val2 = rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            //note val2 may be null if the cast fails this is dependant on if we are using the class foo or class baz

            std::vector<uint8_t> in_val{1, 2, 3, 4};
            std::vector<uint8_t> out_val;

            val->blob_test(in_val, out_val);
            RPC_ASSERT(in_val == out_val);

            // this should trigger NEED_MORE_MEMORY signal requiring more out param data to be provided to the called
            // the out param data is temporarily cached and given over when enough memory has been provided, without
            // recalling the implementation
            in_val.resize(100000);
            std::fill(in_val.begin(), in_val.end(), 42);
            val->blob_test(in_val, out_val);
            RPC_ASSERT(in_val == out_val);
            return rpc::error::OK();
        }

        error_code create_baz_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = rpc::shared_ptr<xxx::i_baz>(new baz(zone_id_));
            return rpc::error::OK();
        }

        error_code get_null_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = nullptr;
            return rpc::error::OK();
        }

        error_code set_interface(const rpc::shared_ptr<xxx::i_baz>& val) override
        {
            cached_ = val;
            return rpc::error::OK();
        }
        error_code get_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = cached_.get_nullable();
            return rpc::error::OK();
        }

        error_code exception_test() override
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->message(rpc::i_telemetry_service::info, "exception_test");
#endif
            throw std::runtime_error("oops");
            return rpc::error::OK();
        }
    };

    class multiple_inheritance : public xxx::i_bar, public xxx::i_baz
    {
        rpc::zone zone_id_;

        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<xxx::i_bar>(interface_id))
                return static_cast<const xxx::i_bar*>(this);
            if (rpc::match<xxx::i_baz>(interface_id))
                return static_cast<const xxx::i_baz*>(this);
            return nullptr;
        }

    public:
        multiple_inheritance(rpc::zone zone_id)
            : zone_id_(zone_id)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("multiple_inheritance", (uint64_t)this, zone_id_);
#endif
        }
        virtual ~multiple_inheritance()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, zone_id_);
#endif
        }

        error_code do_something_else(int val) override { return rpc::error::OK(); }
        int callback(int val) override
        {
            RPC_INFO("callback {}", val);
            return rpc::error::OK();
        }
        error_code blob_test(const std::vector<uint8_t>& inval, std::vector<uint8_t>& out_val) override
        {
            out_val = inval;
            return rpc::error::OK();
        }
    };

    class example : public yyy::i_example
    {
        rpc::member_ptr<yyy::i_host> host_;
        rpc::weak_ptr<rpc::child_service> this_service_;
        rpc::zone zone_id_;

        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<yyy::i_example>(interface_id))
                return static_cast<const yyy::i_example*>(this);
            return nullptr;
        }

    public:
        example(rpc::shared_ptr<rpc::child_service> this_service, rpc::shared_ptr<yyy::i_host> host)
            : host_(host)
            , this_service_(this_service)
            , zone_id_(0)
        {
            if (this_service)
                zone_id_ = this_service->get_zone_id();
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("example", (uint64_t)this, zone_id_);
#endif
        }
        virtual ~example()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, zone_id_);
#endif
        }

        error_code get_host(rpc::shared_ptr<yyy::i_host>& host) override
        {
            host = host_.get_nullable();
            return rpc::error::OK();
        }
        error_code set_host(const rpc::shared_ptr<yyy::i_host>& host) override
        {
            host_ = host;
            return rpc::error::OK();
        }

        error_code create_multiple_inheritance(rpc::shared_ptr<xxx::i_baz>& target) override
        {
            target = rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(zone_id_));
            return rpc::error::OK();
        }

        error_code create_foo(rpc::shared_ptr<xxx::i_foo>& target) override
        {
            target = rpc::shared_ptr<xxx::i_foo>(new foo(zone_id_));
            return rpc::error::OK();
        }

        error_code create_baz(rpc::shared_ptr<xxx::i_baz>& target) override
        {
            target = rpc::shared_ptr<xxx::i_baz>(new baz(zone_id_));
            return rpc::error::OK();
        }

        error_code inner_create_example_in_subordinate_zone(
            rpc::shared_ptr<yyy::i_example>& target, uint64_t new_zone_id, const rpc::shared_ptr<yyy::i_host>& host_ptr)
        {
            auto this_service = this_service_.lock();

            auto err_code = this_service->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
                "example_zone",
                {new_zone_id},
                host_ptr,
                target,
                [](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
                {
                    example_import_idl_register_stubs(child_service_ptr);
                    example_shared_idl_register_stubs(child_service_ptr);
                    example_idl_register_stubs(child_service_ptr);
                    new_example = rpc::shared_ptr<yyy::i_example>(new example(child_service_ptr, host));
                    return rpc::error::OK();
                });
            if (err_code != rpc::error::OK())
                return err_code;
            return err_code;
        }

        error_code create_example_in_subordinate_zone(rpc::shared_ptr<yyy::i_example>& target,
            const rpc::shared_ptr<yyy::i_host>& host_ptr,
            uint64_t new_zone_id) override
        {
            return inner_create_example_in_subordinate_zone(target, new_zone_id, host_ptr);
        }
        error_code create_example_in_subordinate_zone_and_set_in_host(
            uint64_t new_zone_id, const std::string& name, const rpc::shared_ptr<yyy::i_host>& host_ptr) override
        {
            rpc::shared_ptr<yyy::i_example> target;
            auto ret = inner_create_example_in_subordinate_zone(target, new_zone_id, host_ptr);
            if (ret != rpc::error::OK())
                return ret;
            rpc::shared_ptr<yyy::i_host> host;
            target->get_host(host);
            return host->set_app(name, target);
        }

        error_code add(int a, int b, int& c) override
        {
            c = a + b;
            return rpc::error::OK();
        }

        error_code call_create_enclave(const rpc::shared_ptr<yyy::i_host>& host) override
        {
            return call_create_enclave_val(host);
        }
        error_code call_create_enclave_val(rpc::shared_ptr<yyy::i_host> host) override
        {
            if (!host)
                return rpc::error::INVALID_DATA();
            rpc::shared_ptr<yyy::i_example> target;
            auto err = host->create_enclave(target);
            if (err != rpc::error::OK())
                return err;
            if (!target)
                return rpc::error::INVALID_DATA();
            //            target = nullptr;
            int outval = 0;
            auto ret = target->add(1, 2, outval);
            if (ret != rpc::error::OK())
                return ret;
            if (outval != 3)
                return rpc::error::INVALID_DATA();
            return rpc::error::OK();
        }

        error_code call_host_create_enclave_and_throw_away(bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();
            rpc::shared_ptr<i_example> target;
            auto err = host->create_enclave(target);
            if (err != rpc::error::OK())
                return err;
            if (!target)
                return rpc::error::INVALID_DATA();
            if (run_standard_tests)
            {
                int sum = 0;
                err = target->add(1, 2, sum);
                if (err != rpc::error::OK())
                    return err;
                if (sum != 3)
                    return rpc::error::INVALID_DATA();
            }
            return rpc::error::OK();
        }

        error_code call_host_create_enclave(rpc::shared_ptr<i_example>& target, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();
            auto err = host->create_enclave(target);
            if (err != rpc::error::OK())
                return err;
            if (!target)
                return rpc::error::INVALID_DATA();
            if (run_standard_tests)
            {
                int sum = 0;
                err = target->add(1, 2, sum);
                if (err != rpc::error::OK())
                    return err;
                if (sum != 3)
                    return rpc::error::INVALID_DATA();
            }
            return rpc::error::OK();
        }

        error_code call_host_look_up_app_not_return(const std::string& name, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();

            rpc::shared_ptr<i_example> app;
            {
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return");
#endif
                auto err = host->look_up_app(name, app);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return complete");
#endif
                if (err != rpc::error::OK())
                    return err;
            }
            if (run_standard_tests && app)
            {
                int sum = 0;
                auto err = app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    return err;
                if (sum != 3)
                    return rpc::error::INVALID_DATA();
            }
            return rpc::error::OK();
        }

        // live app registry, it should have sole responsibility for the long term storage of app shared ptrs
        error_code call_host_look_up_app(
            const std::string& name, rpc::shared_ptr<i_example>& app, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();

            {
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "look_up_app");
#endif

                auto err = host->look_up_app(name, app);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "look_up_app complete");
#endif

                if (err != rpc::error::OK())
                    return err;
            }

            if (run_standard_tests && app)
            {
                int sum = 0;
                auto err = app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    return err;
                if (sum != 3)
                    return rpc::error::INVALID_DATA();
            }
            return rpc::error::OK();
        }

        error_code call_host_look_up_app_not_return_and_delete(const std::string& name, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();

            rpc::shared_ptr<i_example> app;
            {
                rpc::shared_ptr<i_example> app;
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(
                        rpc::i_telemetry_service::info, "call_host_look_up_app_not_return_and_delete");
#endif

                auto err = host->look_up_app(name, app);
                host->unload_app(name);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(
                        rpc::i_telemetry_service::info, "call_host_look_up_app_not_return_and_delete complete");
#endif
                if (err != rpc::error::OK())
                    return err;
                if (run_standard_tests && app)
                {
                    int sum = 0;
                    auto err = app->add(1, 2, sum);
                    if (err != rpc::error::OK())
                        return err;
                    if (sum != 3)
                        return rpc::error::INVALID_DATA();
                }
            }
            return rpc::error::OK();
        }

        error_code call_host_look_up_app_and_delete(
            const std::string& name, rpc::shared_ptr<i_example>& app, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();

            {
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete");
#endif
                auto err = host->look_up_app(name, app);
                host->unload_app(name);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete complete");
#endif
                if (err != rpc::error::OK())
                    return err;
            }

            if (run_standard_tests && app)
            {
                int sum = 0;
                auto err = app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    return err;
                if (sum != 3)
                    return rpc::error::INVALID_DATA();
            }
            return rpc::error::OK();
        }

        error_code call_host_set_app(
            const std::string& name, const rpc::shared_ptr<i_example>& app, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();            
            if (!host)
                return rpc::error::INVALID_DATA();
            auto err = host->set_app(name, app);
            if (err != rpc::error::OK())
                return err;
            if (run_standard_tests && app)
            {
                int sum = 0;
                err = app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    return err;
                if (sum != 3)
                    return rpc::error::INVALID_DATA();
            }
            return rpc::error::OK();
        }
        error_code call_host_unload_app(const std::string& name) override
        {
            auto host = host_.get_nullable();
            if (!host)
                return rpc::error::INVALID_DATA();
            auto err = host->unload_app(name);
            if (err != rpc::error::OK())
                return err;
            return rpc::error::OK();
        }

        error_code receive_interface(rpc::shared_ptr<xxx::i_foo>& val) override
        {
            val = rpc::shared_ptr<xxx::i_foo>(new foo(zone_id_));
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            return rpc::error::OK();
        }
        error_code give_interface(const rpc::shared_ptr<xxx::i_baz> baz) override
        {
            baz->callback(22);
            auto val1 = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
            return rpc::error::OK();
        }

        error_code send_interface_back(const rpc::shared_ptr<xxx::i_baz>& input, rpc::shared_ptr<xxx::i_baz>& output) override
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->message(rpc::i_telemetry_service::info, "send_interface_back");
#endif
            output = input;
            return rpc::error::OK();
        }
    };
}
