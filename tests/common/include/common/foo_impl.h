/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
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

#include <example_shared/example_shared_stub.h>
#include <example_import/example_import_stub.h>
#include <example/example_stub.h>


namespace marshalled_tests
{
    class baz : public xxx::i_baz, public xxx::i_bar
    {
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
        baz()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("baz", (uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }

        virtual ~baz()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }
        CORO_TASK(error_code) callback(int val) override
        {
            std::ignore = val;
            RPC_INFO("callback {}", val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) blob_test(const std::vector<uint8_t>& inval, std::vector<uint8_t>& out_val) override
        {
            RPC_INFO("baz blob_test {}", inval.size());
            out_val = inval;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_else(int val) override
        {
            std::ignore = val;
            RPC_INFO("baz do_something_else");
            CO_RETURN rpc::error::OK();
        }
    };

    class foo : public xxx::i_foo
    {
        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<xxx::i_foo>(interface_id))
                return static_cast<const xxx::i_foo*>(this);
            return nullptr;
        }

        rpc::member_ptr<xxx::i_baz> cached_;

    public:
        foo()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("foo", (uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }
        virtual ~foo()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }
        CORO_TASK(error_code) do_something_in_val(int val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_in_ref(const int& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_in_by_val_ref(const int& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_in_move_ref(int&& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_in_ptr(const int* val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", *val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_out_val(int& val) override
        {
            val = 33;
            CO_RETURN rpc::error::OK();
        };
        CORO_TASK(error_code) do_something_out_ptr_ref(int*& val) override
        {
            val = new int(33);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_out_ptr_ptr(int** val) override
        {
            *val = new int(33);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_something_in_out_ref(int& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val);
            val = 33;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_complicated_val(const xxx::something_complicated val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.int_val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_complicated_ref(const xxx::something_complicated& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.int_val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_complicated_ref_val(const xxx::something_complicated& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.int_val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_complicated_move_ref(xxx::something_complicated&& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.int_val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_complicated_ptr(const xxx::something_complicated* val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val->int_val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) receive_something_complicated_ref(xxx::something_complicated& val) override
        {
            val = xxx::something_complicated{33, "22"};
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) receive_something_complicated_ptr(xxx::something_complicated*& val) override
        {
            std::ignore = val;
            val = new xxx::something_complicated{33, "22"};
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) receive_something_complicated_in_out_ref(xxx::something_complicated& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.int_val);
            val.int_val = 33;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_more_complicated_val(const xxx::something_more_complicated val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.map_val.begin()->first);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_more_complicated_ref(const xxx::something_more_complicated& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.map_val.begin()->first);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_more_complicated_move_ref(xxx::something_more_complicated&& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.map_val.begin()->first);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code)
        give_something_more_complicated_ref_val(const xxx::something_more_complicated& val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val.map_val.begin()->first);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_something_more_complicated_ptr(const xxx::something_more_complicated* val) override
        {
            std::ignore = val;
            RPC_INFO("got {}", val->map_val.begin()->first);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) receive_something_more_complicated_ref(xxx::something_more_complicated& val) override
        {
            val.map_val["22"] = xxx::something_complicated{33, "22"};
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) receive_something_more_complicated_ptr(xxx::something_more_complicated*& val) override
        {
            val = new xxx::something_more_complicated();
            val->map_val["22"] = xxx::something_complicated{33, "22"};
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code)
        receive_something_more_complicated_in_out_ref(xxx::something_more_complicated& val) override
        {
            if (val.map_val.size())
                RPC_INFO("got {}", val.map_val.begin()->first);
            else
                RPC_ASSERT(!"value is null");
            val.map_val["22"] = xxx::something_complicated{33, "23"};
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) do_multi_val(int val1, int val2) override
        {
            std::ignore = val1;
            std::ignore = val2;
            RPC_INFO("got {}", val1);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code)
        do_multi_complicated_val(const xxx::something_more_complicated val1, const xxx::something_more_complicated val2) override
        {
            std::ignore = val1;
            std::ignore = val2;
            RPC_INFO("got {}", val1.map_val.begin()->first);
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) receive_interface(rpc::shared_ptr<i_foo>& val) override
        {
            val = rpc::shared_ptr<xxx::i_foo>(new foo());
            auto val1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) give_interface(rpc::shared_ptr<xxx::i_baz> baz) override
        {
            CO_AWAIT baz->callback(22);
            auto val1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) call_baz_interface(const rpc::shared_ptr<xxx::i_baz>& val) override
        {
            if (!val)
                CO_RETURN rpc::error::OK();
            CO_AWAIT val->callback(22);
            auto val1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(val);
            // #sgx dynamic cast in an enclave this fails
            auto val2 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            //note val2 may be null if the cast fails this is dependant on if we are using the class foo or class baz

            std::vector<uint8_t> in_val{1, 2, 3, 4};
            std::vector<uint8_t> out_val;

            CO_AWAIT val->blob_test(in_val, out_val);
            RPC_ASSERT(in_val == out_val);

            // this should trigger NEED_MORE_MEMORY signal requiring more out param data to be provided to the called
            // the out param data is temporarily cached and given over when enough memory has been provided, without
            // recalling the implementation
            in_val.resize(100000);
            std::fill(in_val.begin(), in_val.end(), 42);
            CO_AWAIT val->blob_test(in_val, out_val);
            RPC_ASSERT(in_val == out_val);
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) create_baz_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = rpc::shared_ptr<xxx::i_baz>(new baz());
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) get_null_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = nullptr;
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) set_interface(const rpc::shared_ptr<xxx::i_baz>& val) override
        {
            cached_ = val;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) get_interface(rpc::shared_ptr<xxx::i_baz>& val) override
        {
            val = cached_.get_nullable();
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) exception_test() override
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->message(rpc::i_telemetry_service::info, "exception_test");
#endif
            throw std::runtime_error("oops");
            CO_RETURN rpc::error::OK();
        }
    };

    class multiple_inheritance : public xxx::i_bar, public xxx::i_baz
    {
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
        multiple_inheritance()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("multiple_inheritance", (uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }
        virtual ~multiple_inheritance()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }

        CORO_TASK(error_code) do_something_else(int val) override
        {
            std::ignore = val;
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) callback(int val) override
        {
            std::ignore = val;
            RPC_INFO("callback {}", val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) blob_test(const std::vector<uint8_t>& inval, std::vector<uint8_t>& out_val) override
        {
            out_val = inval;
            CO_RETURN rpc::error::OK();
        }
    };

    class example : public yyy::i_example, public rpc::enable_shared_from_this<example>
    {
        rpc::member_ptr<yyy::i_host> host_;
        rpc::weak_ptr<rpc::service> this_service_;

        void* get_address() const override { return (void*)this; }
        const rpc::casting_interface* query_interface(rpc::interface_ordinal interface_id) const override
        {
            if (rpc::match<yyy::i_example>(interface_id))
                return static_cast<const yyy::i_example*>(this);
            return nullptr;
        }

    public:
        example(rpc::shared_ptr<rpc::service> this_service, rpc::shared_ptr<yyy::i_host> host)
            : host_(host)
            , this_service_(this_service)
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_creation("example", (uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }
        virtual ~example()
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->on_impl_deletion((uint64_t)this, rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id() : rpc::zone{0});
#endif
        }

        CORO_TASK(error_code) get_host(rpc::shared_ptr<yyy::i_host>& host) override
        {
            host = host_.get_nullable();
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) set_host(const rpc::shared_ptr<yyy::i_host>& host) override
        {
            host_ = host;
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) create_multiple_inheritance(rpc::shared_ptr<xxx::i_baz>& target) override
        {
            target = rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance());
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) create_foo(rpc::shared_ptr<xxx::i_foo>& target) override
        {
            target = rpc::shared_ptr<xxx::i_foo>(new foo());
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) create_baz(rpc::shared_ptr<xxx::i_baz>& target) override
        {
            target = rpc::shared_ptr<xxx::i_baz>(new baz());
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        inner_create_example_in_subordinate_zone(
            rpc::shared_ptr<yyy::i_example>& target, uint64_t new_zone_id, const rpc::shared_ptr<yyy::i_host>& host_ptr)
        {
            auto this_service = this_service_.lock();

            auto err_code
                = CO_AWAIT this_service->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
                    "example_zone",
                    {new_zone_id},
                    host_ptr,
                    target,
                    [](const rpc::shared_ptr<yyy::i_host>& host,
                        rpc::shared_ptr<yyy::i_example>& new_example,
                        const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(error_code)
                    {
                        example_import_idl_register_stubs(child_service_ptr);
                        example_shared_idl_register_stubs(child_service_ptr);
                        example_idl_register_stubs(child_service_ptr);
                        new_example
                            = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, host));
                        CO_RETURN rpc::error::OK();
                    });
            if (err_code != rpc::error::OK())
                CO_RETURN err_code;
            CO_RETURN err_code;
        }

        CORO_TASK(error_code)
        create_example_in_subordinate_zone(rpc::shared_ptr<yyy::i_example>& target,
            const rpc::shared_ptr<yyy::i_host>& host_ptr,
            uint64_t new_zone_id) override
        {
            CO_RETURN CO_AWAIT inner_create_example_in_subordinate_zone(target, new_zone_id, host_ptr);
        }
        CORO_TASK(error_code)
        create_example_in_subordinate_zone_and_set_in_host(
            uint64_t new_zone_id, const std::string& name, const rpc::shared_ptr<yyy::i_host>& host_ptr) override
        {
            rpc::shared_ptr<yyy::i_example> target;
            auto ret = CO_AWAIT inner_create_example_in_subordinate_zone(target, new_zone_id, host_ptr);
            if (ret != rpc::error::OK())
                CO_RETURN ret;
            rpc::shared_ptr<yyy::i_host> host;
            CO_AWAIT target->get_host(host);
            CO_RETURN CO_AWAIT host->set_app(name, target);
        }

        CORO_TASK(error_code) add(int a, int b, int& c) override
        {
            c = a + b;
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) call_create_enclave(const rpc::shared_ptr<yyy::i_host>& host) override
        {
            CO_RETURN CO_AWAIT call_create_enclave_val(host);
        }
        CORO_TASK(error_code) call_create_enclave_val(rpc::shared_ptr<yyy::i_host> host) override
        {
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();
            rpc::shared_ptr<yyy::i_example> target;
            auto err = CO_AWAIT host->create_enclave(target);
            if (err != rpc::error::OK())
                CO_RETURN err;
            if (!target)
                CO_RETURN rpc::error::INVALID_DATA();
            //            target = nullptr;
            int outval = 0;
            auto ret = CO_AWAIT target->add(1, 2, outval);
            if (ret != rpc::error::OK())
                CO_RETURN ret;
            if (outval != 3)
                CO_RETURN rpc::error::INVALID_DATA();
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) call_host_create_enclave_and_throw_away(bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();
            rpc::shared_ptr<i_example> target;
            auto err = CO_AWAIT host->create_enclave(target);
            if (err != rpc::error::OK())
                CO_RETURN err;
            if (!target)
                CO_RETURN rpc::error::INVALID_DATA();
            if (run_standard_tests)
            {
                int sum = 0;
                err = CO_AWAIT target->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        call_host_create_enclave(rpc::shared_ptr<i_example>& target, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();
            auto err = CO_AWAIT host->create_enclave(target);
            if (err != rpc::error::OK())
                CO_RETURN err;
            if (!target)
                CO_RETURN rpc::error::INVALID_DATA();
            if (run_standard_tests)
            {
                int sum = 0;
                err = CO_AWAIT target->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        call_host_look_up_app_not_return(const std::string& name, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();

            rpc::shared_ptr<i_example> app;
            {
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return");
#endif
                auto err = CO_AWAIT host->look_up_app(name, app);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return complete");
#endif
                if (err != rpc::error::OK())
                    CO_RETURN err;
            }
            if (run_standard_tests && app)
            {
                int sum = 0;
                auto err = CO_AWAIT app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }

        // live app registry, it should have sole responsibility for the long term storage of app shared ptrs
        CORO_TASK(error_code)
        call_host_look_up_app(const std::string& name, rpc::shared_ptr<i_example>& app, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();

            {
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "look_up_app");
#endif

                auto err = CO_AWAIT host->look_up_app(name, app);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "look_up_app complete");
#endif

                if (err != rpc::error::OK())
                    CO_RETURN err;
            }

            if (run_standard_tests && app)
            {
                int sum = 0;
                auto err = CO_AWAIT app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        call_host_look_up_app_not_return_and_delete(const std::string& name, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();

            rpc::shared_ptr<i_example> app;
#ifdef USE_RPC_TELEMETRY
            auto telemetry_service = rpc::telemetry_service_manager::get();
            if (telemetry_service)
                telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_not_return_and_delete");
#endif

            auto err = CO_AWAIT host->look_up_app(name, app);
            CO_AWAIT host->unload_app(name);

#ifdef USE_RPC_TELEMETRY
            if (telemetry_service)
                telemetry_service->message(
                    rpc::i_telemetry_service::info, "call_host_look_up_app_not_return_and_delete complete");
#endif
            if (err != rpc::error::OK())
                CO_RETURN err;
            if (run_standard_tests && app)
            {
                int sum = 0;
                err = CO_AWAIT app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        call_host_look_up_app_and_delete(
            const std::string& name, rpc::shared_ptr<i_example>& app, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();

            {
#ifdef USE_RPC_TELEMETRY
                auto telemetry_service = rpc::telemetry_service_manager::get();
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete");
#endif
                auto err = CO_AWAIT host->look_up_app(name, app);
                CO_AWAIT host->unload_app(name);

#ifdef USE_RPC_TELEMETRY
                if (telemetry_service)
                    telemetry_service->message(rpc::i_telemetry_service::info, "call_host_look_up_app_and_delete complete");
#endif
                if (err != rpc::error::OK())
                    CO_RETURN err;
            }

            if (run_standard_tests && app)
            {
                int sum = 0;
                auto err = CO_AWAIT app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        call_host_set_app(const std::string& name, const rpc::shared_ptr<i_example>& app, bool run_standard_tests) override
        {
            auto host = host_.get_nullable();            
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();
            auto err = CO_AWAIT host->set_app(name, app);
            if (err != rpc::error::OK())
                CO_RETURN err;
            if (run_standard_tests && app)
            {
                int sum = 0;
                err = CO_AWAIT app->add(1, 2, sum);
                if (err != rpc::error::OK())
                    CO_RETURN err;
                if (sum != 3)
                    CO_RETURN rpc::error::INVALID_DATA();
            }
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) call_host_unload_app(const std::string& name) override
        {
            auto host = host_.get_nullable();
            if (!host)
                CO_RETURN rpc::error::INVALID_DATA();
            auto err = CO_AWAIT host->unload_app(name);
            if (err != rpc::error::OK())
                CO_RETURN err;
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) receive_interface(rpc::shared_ptr<xxx::i_foo>& val) override
        {
            val = rpc::shared_ptr<xxx::i_foo>(new foo());
            auto val1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(val);
            CO_RETURN rpc::error::OK();
        }
        CORO_TASK(error_code) give_interface(const rpc::shared_ptr<xxx::i_baz> baz) override
        {
            CO_AWAIT baz->callback(22);
            auto val1 = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code)
        send_interface_back(const rpc::shared_ptr<xxx::i_baz>& input, rpc::shared_ptr<xxx::i_baz>& output) override
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
                telemetry_service->message(rpc::i_telemetry_service::info, "send_interface_back");
#endif
            output = input;
            CO_RETURN rpc::error::OK();
        }

        CORO_TASK(error_code) create_fork_and_return_object(rpc::shared_ptr<yyy::i_example> zone_factory, const std::vector<uint64_t>& fork_zone_ids, rpc::shared_ptr<yyy::i_example>& object_from_forked_zone) override
        {
            // This method runs in the current zone and autonomously creates a chain of zones through the factory
            // The zone_factory is a reference to an intermediate zone that can create new zones
            // fork_zone_ids specifies the chain of zones to create and which zone to get the object from
            RPC_INFO("example::create_fork_and_return_object - Zone {} creating fork chain through zone factory", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            if (fork_zone_ids.empty()) {
                RPC_ERROR("fork_zone_ids cannot be empty");
                CO_RETURN rpc::error::INVALID_DATA();
            }
            
            auto host = host_.get_nullable();
            if (!host) {
                RPC_ERROR("Cannot get host for zone creation");
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }
            
            // Create the chain of zones using the factory
            // The factory creates zones that the root zone is unaware of
            rpc::shared_ptr<yyy::i_example> current_zone = zone_factory;
            rpc::shared_ptr<yyy::i_example> target_zone = nullptr;
            
            for (size_t i = 0; i < fork_zone_ids.size(); ++i) {
                uint64_t zone_id = fork_zone_ids[i];
                RPC_INFO("Creating zone {} in fork chain (step {} of {})", zone_id, i+1, fork_zone_ids.size());
                
                rpc::shared_ptr<yyy::i_example> new_zone;
                auto err = CO_AWAIT current_zone->create_example_in_subordinate_zone(new_zone, host, zone_id);
                if (err != rpc::error::OK()) {
                    RPC_ERROR("Failed to create zone {} in fork chain: {}", zone_id, err);
                    CO_RETURN err;
                }
                
                if (!new_zone) {
                    RPC_ERROR("Zone creation returned null for zone {}", zone_id);
                    CO_RETURN rpc::error::ZONE_NOT_FOUND();
                }
                
                // The last zone in the chain is where we'll create the object
                if (i == fork_zone_ids.size() - 1) {
                    target_zone = new_zone;
                }
                
                // For the next iteration, this new zone becomes the factory
                current_zone = new_zone;
            }
            
            if (!target_zone) {
                RPC_ERROR("No target zone available for object creation");
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }
            
            RPC_INFO("Successfully created fork chain, creating object in final zone {}", fork_zone_ids.back());
            object_from_forked_zone = current_zone;
            
            RPC_INFO("Successfully created object in zone {} - will return to caller", fork_zone_ids.back());
            
            // This object is from the final zone in the fork, which the root zone doesn't know about
            // When this gets passed to the root zone, it should trigger the routing fix
            CO_RETURN rpc::error::OK();
        }

    private:
        // Cache for storing objects from autonomous zones
        rpc::shared_ptr<yyy::i_example> cached_autonomous_object_;

    public:
        CORO_TASK(error_code) cache_object_from_autonomous_zone(const std::vector<uint64_t>& zone_ids) override
        {
            RPC_INFO("example::cache_object_from_autonomous_zone - Zone {} autonomously creating and caching object from unknown zone", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            if (zone_ids.empty()) {
                RPC_ERROR("zone_ids cannot be empty");
                CO_RETURN rpc::error::INVALID_DATA();
            }
            
            // Create the autonomous zone and object using create_fork_and_return_object
            // This zone creates a child zone that other zones (including root) don't know about
            rpc::shared_ptr<yyy::i_example> autonomous_object;
            auto err = CO_AWAIT create_fork_and_return_object(shared_from_this(), zone_ids, autonomous_object);
            if (err != rpc::error::OK()) {
                RPC_ERROR("Failed to create autonomous zone and object: {}", err);
                CO_RETURN err;
            }
            
            if (!autonomous_object) {
                RPC_ERROR("Autonomous object creation returned null");
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }
            
            // Cache the object locally
            cached_autonomous_object_ = autonomous_object;
            
            RPC_INFO("Successfully cached object from autonomous zone {} in zone {}", zone_ids.back(), rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            CO_RETURN rpc::error::OK();
        }
        
        CORO_TASK(error_code) create_y_topology_fork(rpc::shared_ptr<yyy::i_example> factory_zone, const std::vector<uint64_t>& fork_zone_ids) override
        {
            RPC_INFO("example::create_y_topology_fork - Zone {} creating Y-topology fork via factory zone", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            if (fork_zone_ids.empty()) {
                RPC_ERROR("fork_zone_ids cannot be empty");
                CO_RETURN rpc::error::INVALID_DATA();
            }
            
            if (!factory_zone) {
                RPC_ERROR("factory_zone cannot be null");
                CO_RETURN rpc::error::INVALID_DATA();
            }
            
            // CRITICAL Y-TOPOLOGY PATTERN:
            // This zone (e.g. Zone 5) asks an earlier zone in the hierarchy (e.g. Zone 3) 
            // to create autonomous zones. Zone 3 creates the new zones but Zone 1 (root) 
            // and other zones in the original chain are NOT notified.
            // This creates the true Y-topology where one prong creates a fork at an earlier point.
            
            RPC_INFO("Zone {} asking factory zone to create autonomous fork with {} zones", 
                     rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0, fork_zone_ids.size());
                     
            rpc::shared_ptr<yyy::i_example> object_from_forked_zone;
            auto err = CO_AWAIT create_fork_and_return_object(factory_zone, fork_zone_ids, object_from_forked_zone);
            if (err != rpc::error::OK()) {
                RPC_ERROR("Factory zone failed to create autonomous fork: {}", err);
                CO_RETURN err;
            }
            
            // Cache it locally so we can later pass it to zones that have no route to the fork
            cached_autonomous_object_ = object_from_forked_zone;
            
            RPC_INFO("Successfully created Y-topology fork - Zone {} now has object from factory's autonomous zones", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            CO_RETURN rpc::error::OK();
        }
        
        CORO_TASK(error_code) retrieve_cached_autonomous_object(rpc::shared_ptr<yyy::i_example>& cached_object) override
        {
            RPC_INFO("example::retrieve_cached_autonomous_object - Zone {} retrieving cached autonomous object", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            if (!cached_autonomous_object_) {
                RPC_ERROR("No cached autonomous object available in zone {}", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }
            
            cached_object = cached_autonomous_object_;
            
            RPC_INFO("Successfully retrieved cached autonomous object in zone {}", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            // CRITICAL: This is where the routing bug should trigger
            // When this cached object (from an unknown autonomous zone) gets passed
            // to another zone that has no route to the original zone, it causes
            // infinite recursion in add_ref without the known_direction_zone fix
            
            CO_RETURN rpc::error::OK();
        }
        
        CORO_TASK(error_code) give_host_cached_object()
        {
            RPC_INFO("example::give_host_cached_object - Zone {} giving host cached autonomous object", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            if (!cached_autonomous_object_) {
                RPC_ERROR("No cached autonomous object available in zone {}", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }
            
            auto host = host_.get_nullable();
            if(!host)
            {
                RPC_ERROR("No cached host object available in zone {}", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
                CO_RETURN rpc::error::OBJECT_NOT_FOUND();
            }
            auto err = CO_AWAIT host->set_app("foo", cached_autonomous_object_);
            if (err != rpc::error::OK()) {
                RPC_ERROR("Factory zone failed to call set_app: {}", err);
                CO_RETURN err;
            }
            
            RPC_INFO("Successfully retrieved cached autonomous object in zone {}", rpc::service::get_current_service() ? rpc::service::get_current_service()->get_zone_id().id : 0);
            
            // CRITICAL: This is where the routing bug should trigger
            // When this cached object (from an unknown autonomous zone) gets passed
            // to another zone that has no route to the original zone, it causes
            // infinite recursion in add_ref without the known_direction_zone fix
            
            CO_RETURN rpc::error::OK();            
        }
    };
}
