/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
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
#include <rpc/telemetry/host_telemetry_service.h>
#endif

template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone> class enclave_setup
{
    rpc::shared_ptr<rpc::service> root_service_;
    rpc::shared_ptr<yyy::i_host> i_host_ptr_;
    rpc::weak_ptr<yyy::i_host> local_host_ptr_;
    rpc::shared_ptr<yyy::i_example> i_example_ptr_;

    const bool has_enclave_ = true;
    bool use_host_in_child_ = UseHostInChild;
    bool run_standard_tests_ = RunStandardTests;

    std::atomic<uint64_t> zone_gen_ = 0;

public:
    virtual ~enclave_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const { return root_service_; }
    bool get_has_enclave() const { return has_enclave_; }
    bool is_enclave_setup() const { return true; }
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr() { return local_host_ptr_.lock(); }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
    bool get_use_host_in_child() const { return use_host_in_child_; }

    virtual void SetUp()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if (enable_telemetry_server)
        {
            telemetry_service_manager_.create(test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/");
        }
#endif
        root_service_ = rpc::make_shared<rpc::service>("host", rpc::zone{++zone_gen_});
        example_import_idl_register_stubs(root_service_);
        example_shared_idl_register_stubs(root_service_);
        example_idl_register_stubs(root_service_);
        current_host_service = root_service_;

        i_host_ptr_ = rpc::shared_ptr<yyy::i_host>(new host(root_service_->get_zone_id()));
        local_host_ptr_ = i_host_ptr_;

        auto new_zone_id = ++(*zone_gen);
        auto err_code = root_service_->connect_to_zone<rpc::enclave_service_proxy>(
            "main child", {new_zone_id}, use_host_in_child_ ? i_host_ptr_ : nullptr, i_example_ptr_, enclave_path);

        ASSERT_ERROR_CODE(err_code);
    }

    virtual void TearDown()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        root_service_ = nullptr;
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
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
