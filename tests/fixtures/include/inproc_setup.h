/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include "test_host.h"
#include "test_service_logger.h"
#include "test_globals.h"
#include <gtest/gtest.h>
#include <common/foo_impl.h>
#include <common/tests.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/host_telemetry_service.h>
#endif

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone> class inproc_setup
{
    rpc::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<rpc::child_service> child_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;

    std::shared_ptr<coro::io_scheduler> io_scheduler_;
    bool error_has_occured_ = false;

    bool startup_complete_ = false;
    bool shutdown_complete_ = false;

public:
    std::shared_ptr<coro::io_scheduler> get_scheduler() const { return io_scheduler_; }
    bool error_has_occured() const { return error_has_occured_; }

    virtual ~inproc_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const { return root_service_; }
    bool get_has_enclave() const { return has_enclave_; }
    bool is_enclave_setup() const { return false; }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr() { return local_host_ptr_.lock(); }
    bool get_use_host_in_child() const { return use_host_in_child_; }

    CORO_TASK(void) check_for_error(coro::task<bool> task)
    {
        auto ret = CO_AWAIT task;
        if (!ret)
        {
            error_has_occured_ = true;
        }
        CO_RETURN;
    }

    CORO_TASK(bool) CoroSetUp()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if (enable_telemetry_server)
            CREATE_TELEMETRY_SERVICE(
                rpc::host_telemetry_service, test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/")
#endif

        root_service_ = rpc::make_shared<rpc::service>("host", rpc::zone{++zone_gen_}, io_scheduler_);
        root_service_->add_service_logger(std::make_shared<test_service_logger>());
        current_host_service = root_service_;

        rpc::shared_ptr<yyy::i_host> hst(new host(root_service_->get_zone_id()));
        local_host_ptr_ = hst; // assign to weak ptr

        auto ret = CO_AWAIT root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
            "main child",
            {++zone_gen_},
            hst,
            i_example_ptr_,
            [&](const rpc::shared_ptr<yyy::i_host>& host,
                rpc::shared_ptr<yyy::i_example>& new_example,
                const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
            {
                i_host_ptr_ = host;
                child_service_ = child_service_ptr;
                example_import_idl_register_stubs(child_service_ptr);
                example_shared_idl_register_stubs(child_service_ptr);
                example_idl_register_stubs(child_service_ptr);
                new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, nullptr));
                if (use_host_in_child_)
                    CO_AWAIT new_example->set_host(host);
                CO_RETURN rpc::error::OK();
            });
        startup_complete_ = true;
        if (ret != rpc::error::OK())
        {
            CO_RETURN false;
        }
        CO_RETURN true;
    }

    virtual void SetUp()
    {
        io_scheduler_ = coro::io_scheduler::make_shared(
            coro::io_scheduler::options{.thread_strategy = coro::io_scheduler::thread_strategy_t::manual,
                .pool = coro::thread_pool::options{
                    .thread_count = 1,
                }});

        io_scheduler_->schedule(check_for_error(CoroSetUp()));
        while (startup_complete_ == false || io_scheduler_->process_events())
        {
        }

        // auto err_code = SYNC_WAIT();

        ASSERT_EQ(error_has_occured_, false);
    }

    CORO_TASK(void) CoroTearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        child_service_ = nullptr;
        shutdown_complete_ = true;
        CO_RETURN;
    }

    virtual void TearDown()
    {
        io_scheduler_->schedule(CoroTearDown());
        while (shutdown_complete_ == false || io_scheduler_->process_events())
        {
        }
        root_service_ = nullptr;
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif
        // SYNC_WAIT(CoroTearDown());
    }

    CORO_TASK(rpc::shared_ptr<yyy::i_example>) create_new_zone()
    {
        rpc::shared_ptr<yyy::i_host> hst;
        if (use_host_in_child_)
            hst = local_host_ptr_.lock();

        rpc::shared_ptr<yyy::i_example> example_relay_ptr;

        auto err_code
            = CO_AWAIT root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>(
                "main child",
                {++zone_gen_},
                hst,
                example_relay_ptr,
                [&](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> CORO_TASK(int)
                {
                    example_import_idl_register_stubs(child_service_ptr);
                    example_shared_idl_register_stubs(child_service_ptr);
                    example_idl_register_stubs(child_service_ptr);
                    new_example
                        = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, nullptr));
                    if (use_host_in_child_)
                        CO_AWAIT new_example->set_host(host);
                    CO_RETURN rpc::error::OK();
                });

        if (CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            if (CO_AWAIT example_relay_ptr->create_example_in_subordinate_zone(
                    new_ptr, use_host_in_child_ ? hst : nullptr, ++zone_gen_)
                == rpc::error::OK())
            {
                CO_AWAIT example_relay_ptr->set_host(nullptr);
                example_relay_ptr = new_ptr;
            }
        }
        CO_RETURN example_relay_ptr;
    }
};
