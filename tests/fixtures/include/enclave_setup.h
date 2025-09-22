/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#ifdef BUILD_ENCLAVE

#include <atomic>
#include "test_host.h"
#include "test_globals.h"
#include <gtest/gtest.h>
#include <common/foo_impl.h>
#include <common/tests.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone> class enclave_setup
{
    std::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;
    bool error_has_occured_ = false;

public:
    virtual ~enclave_setup() = default;

    std::shared_ptr<rpc::service> get_root_service() const { return root_service_; }
    bool get_has_enclave() const { return has_enclave_; }
    bool is_enclave_setup() const { return true; }
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr() { return local_host_ptr_.lock(); }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
    bool get_use_host_in_child() const { return use_host_in_child_; }

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
        zone_gen = &zone_gen_;
#ifdef USE_RPC_TELEMETRY
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        if (auto telemetry_service
            = std::static_pointer_cast<rpc::multiplexing_telemetry_service>(rpc::get_telemetry_service()))
        {
            telemetry_service->start_test(test_info->test_suite_name(), test_info->name());
        }
#endif
        root_service_ = std::make_shared<rpc::service>("host", rpc::zone{++zone_gen_});
        example_import_idl_register_stubs(root_service_);
        example_shared_idl_register_stubs(root_service_);
        example_idl_register_stubs(root_service_);
        current_host_service = root_service_;

        i_host_ptr_ = rpc::shared_ptr<yyy::i_host>(new host());
        local_host_ptr_ = i_host_ptr_;

        auto new_zone_id = ++(*zone_gen);
        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            "main child", {new_zone_id}, use_host_in_child_ ? i_host_ptr_ : nullptr, i_example_ptr_, enclave_path);

        RPC_ASSERT(err_code == rpc::error::OK());
    }

    virtual void tear_down()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        root_service_ = nullptr;
        current_host_service.reset();
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service
            = std::static_pointer_cast<rpc::multiplexing_telemetry_service>(rpc::get_telemetry_service()))
        {
            telemetry_service->reset_for_test();
        }
#endif
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {
        rpc::shared_ptr<yyy::i_example> ptr;
        auto new_zone_id = ++(zone_gen_);
        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            "main child", {new_zone_id}, use_host_in_child_ ? i_host_ptr_ : nullptr, ptr, enclave_path);

        if (err_code != rpc::error::OK())
            return nullptr;
        if (CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            err_code = ptr->create_example_in_subordinate_zone(
                new_ptr, use_host_in_child_ ? i_host_ptr_ : nullptr, ++zone_gen_);
            if (err_code != rpc::error::OK())
                return nullptr;
            ptr = new_ptr;
        }
        return ptr;
    }
};

#endif // BUILD_ENCLAVE
