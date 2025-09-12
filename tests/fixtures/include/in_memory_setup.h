/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include "test_host.h"
#include "test_globals.h"
#include <gtest/gtest.h>
#include <common/foo_impl.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

template<bool UseHostInChild> class in_memory_setup
{
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = false;
    bool use_host_in_child_ = UseHostInChild;

    std::atomic<uint64_t> zone_gen_ = 0;

#ifdef BUILD_COROUTINE
    std::shared_ptr<coro::io_scheduler> io_scheduler_;
#endif    
    bool error_has_occured_ = false;

public:
    virtual ~in_memory_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const { return nullptr; }
    bool get_has_enclave() const { return has_enclave_; }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr() { return local_host_ptr_.lock(); }
    bool get_use_host_in_child() const { return use_host_in_child_; }

#ifdef BUILD_COROUTINE
    std::shared_ptr<coro::io_scheduler> get_scheduler() const { return io_scheduler_; }
#endif    
    bool error_has_occured() const { return error_has_occured_; }

    CORO_TASK(void) check_for_error(CORO_TASK(bool) task)
    {
        auto ret = CO_AWAIT task;
        if (!ret)
        {
            error_has_occured_ = true;
        }
        CO_RETURN;
    }

    virtual void set_up()
    {
#ifdef BUILD_COROUTINE        
        io_scheduler_ = coro::io_scheduler::make_shared(
            coro::io_scheduler::options{.thread_strategy = coro::io_scheduler::thread_strategy_t::manual,
                .pool = coro::thread_pool::options{
                    .thread_count = 1,
                }});
#endif                
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if (enable_telemetry_server)
            telemetry_service_manager_.create(test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/");
#endif

        i_host_ptr_ = rpc::shared_ptr<yyy::i_host>(new host({++zone_gen_}));
        local_host_ptr_ = i_host_ptr_;
        i_example_ptr_ = rpc::shared_ptr<yyy::i_example>(
            new marshalled_tests::example(nullptr, use_host_in_child_ ? i_host_ptr_ : nullptr));
    }

    virtual void tear_down()
    {
        i_host_ptr_ = nullptr;
        i_example_ptr_ = nullptr;
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif
    }
};
