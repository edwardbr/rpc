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
#include <rpc/telemetry/i_telemetry_service.h>
#endif


template<bool UseHostInChild, bool RunStandardTests, bool CreateNewZoneThenCreateSubordinatedZone> 
class inproc_setup
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

public:
    virtual ~inproc_setup() = default;

    rpc::shared_ptr<rpc::service> get_root_service() const { return root_service_; }
    bool get_has_enclave() const { return has_enclave_; }
    bool is_enclave_setup() const { return false; }
    rpc::shared_ptr<yyy::i_example> get_example() const { return i_example_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_host() const { return i_host_ptr_; }
    rpc::shared_ptr<yyy::i_host> get_local_host_ptr() { return local_host_ptr_.lock(); }
    bool get_use_host_in_child() const { return use_host_in_child_; }

    virtual void set_up()
    {
        zone_gen = &zone_gen_;
        auto test_info = ::testing::UnitTest::GetInstance()->current_test_info();
#ifdef USE_RPC_TELEMETRY
        if (enable_telemetry_server)
            telemetry_service_manager_.create(test_info->test_suite_name(), test_info->name(), "../../rpc_test_diagram/");                
#endif

        root_service_ = rpc::make_shared<rpc::service>("host", rpc::zone{++zone_gen_});
        current_host_service = root_service_;

        rpc::shared_ptr<yyy::i_host> hst(new host(root_service_->get_zone_id()));
        local_host_ptr_ = hst; // assign to weak ptr

        auto err_code
            = root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>("main child",
                {++zone_gen_},
                hst,
                i_example_ptr_,
                [&](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
                {
                    i_host_ptr_ = host;
                    child_service_ = child_service_ptr;
                    example_import_idl_register_stubs(child_service_ptr);
                    example_shared_idl_register_stubs(child_service_ptr);
                    example_idl_register_stubs(child_service_ptr);
                    new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, nullptr));
                    if (use_host_in_child_)
                        new_example->set_host(host);
                    return rpc::error::OK();
                });
        ASSERT_ERROR_CODE(err_code);
    }

    virtual void tear_down()
    {
        i_example_ptr_ = nullptr;
        i_host_ptr_ = nullptr;
        child_service_ = nullptr;
        root_service_ = nullptr;
        current_host_service.reset();
        test_service_logger::reset_logger();
        zone_gen = nullptr;
#ifdef USE_RPC_TELEMETRY
        RESET_TELEMETRY_SERVICE
#endif
    }

    rpc::shared_ptr<yyy::i_example> create_new_zone()
    {
        rpc::shared_ptr<yyy::i_host> hst;
        if (use_host_in_child_)
            hst = local_host_ptr_.lock();

        rpc::shared_ptr<yyy::i_example> example_relay_ptr;

        auto err_code
            = root_service_->connect_to_zone<rpc::local_child_service_proxy<yyy::i_example, yyy::i_host>>("main child",
                {++zone_gen_},
                hst,
                example_relay_ptr,
                [&](const rpc::shared_ptr<yyy::i_host>& host,
                    rpc::shared_ptr<yyy::i_example>& new_example,
                    const rpc::shared_ptr<rpc::child_service>& child_service_ptr) -> int
                {
                    example_import_idl_register_stubs(child_service_ptr);
                    example_shared_idl_register_stubs(child_service_ptr);
                    example_idl_register_stubs(child_service_ptr);
                    new_example = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, nullptr));
                    if (use_host_in_child_)
                        new_example->set_host(host);
                    return rpc::error::OK();
                });

        if (CreateNewZoneThenCreateSubordinatedZone)
        {
            rpc::shared_ptr<yyy::i_example> new_ptr;
            if (example_relay_ptr->create_example_in_subordinate_zone(new_ptr, use_host_in_child_ ? hst : nullptr, ++zone_gen_)
                == rpc::error::OK())
            {
                example_relay_ptr->set_host(nullptr);
                example_relay_ptr = new_ptr;
            }
        }
        return example_relay_ptr;
    }
};