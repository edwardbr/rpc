/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// Standard C++ headers
#include <iostream>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <filesystem>
#include <atomic>

// RPC headers
#include <rpc/rpc.h>
#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/multiplexing_telemetry_service.h>
#include <rpc/telemetry/console_telemetry_service.h>
#include <rpc/telemetry/sequence_diagram_telemetry_service.h>
#endif

// Other headers
#include <args.hxx>
#ifdef BUILD_COROUTINE
#include <coro/io_scheduler.hpp>
#endif
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <common/foo_impl.h>
#include <common/tests.h>
#ifdef BUILD_COROUTINE
// #include "common/tcp/service_proxy.h"
// #include "common/tcp/listener.h"
// #include "common/spsc/service_proxy.h"
// #include "common/spsc/channel_manager.h"
#endif
#include "rpc_global_logger.h"
#include "test_host.h"
#include "in_memory_setup.h"
#include "inproc_setup.h"
#include "enclave_setup.h"
#ifdef BUILD_COROUTINE
// #include "tcp_setup.h"  // Temporarily disabled
#include "spsc_setup.h"
#endif
#include "crash_handler.h"

using namespace marshalled_tests;

#include "test_globals.h"

extern bool enable_multithreaded_tests;

// Test fixture for post functionality
template<typename SetupType> class post_functionality_test : public ::testing::Test
{
protected:
    SetupType lib_{};

    void SetUp() override { lib_.init(); }

    void TearDown() override { lib_.cleanup(); }

    SetupType& get_lib() { return lib_; }
};

// Define the test types for different transport mechanisms
using post_test_implementations = ::testing::Types<in_memory_setup<false>,
    in_memory_setup<true>,
    inproc_setup<false, false, false>,
    inproc_setup<false, false, true>,
    inproc_setup<false, true, false>,
    inproc_setup<false, true, true>,
    inproc_setup<true, false, false>,
    inproc_setup<true, false, true>,
    inproc_setup<true, true, false>,
    inproc_setup<true, true, true>
#ifdef BUILD_COROUTINE
    ,
    // tcp_setup<false, false, false>,
    // tcp_setup<false, false, true>,
    // tcp_setup<false, true, false>,
    // tcp_setup<false, true, true>,
    // tcp_setup<true, false, false>,
    // tcp_setup<true, false, true>,
    // tcp_setup<true, true, false>,
    // tcp_setup<true, true, true>,
    spsc_setup<false, false, false>,
    spsc_setup<false, false, true>,
    spsc_setup<false, true, false>,
    spsc_setup<false, true, true>,
    spsc_setup<true, false, false>,
    spsc_setup<true, false, true>,
    spsc_setup<true, true, false>,
    spsc_setup<true, true, true>
#endif
#ifdef BUILD_ENCLAVE
    ,
    enclave_setup<false, false, false>,
    enclave_setup<false, false, true>,
    enclave_setup<false, true, false>,
    enclave_setup<false, true, true>,
    enclave_setup<true, false, false>,
    enclave_setup<true, false, true>,
    enclave_setup<true, true, false>,
    enclave_setup<true, true, true>
#endif
    >;

TYPED_TEST_SUITE(post_functionality_test, post_test_implementations);

// Test basic post functionality with normal option
TYPED_TEST(post_functionality_test, basic_post_normal)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create a foo object to test with
            rpc::shared_ptr<xxx::i_foo> foo_obj;
            CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
            CORO_ASSERT_NE(foo_obj, nullptr);

            // Get the service proxy for the foo object to use the post functionality
            auto sp = rpc::casting_interface::get_service_proxy(foo_obj);
            CORO_ASSERT_NE(sp, nullptr);

            // Perform a post operation - fire-and-forget call using the service proxy directly
            // This should not block or return anything
            CO_AWAIT sp->post(rpc::get_version(),
                rpc::encoding::enc_default,
                0, // tag
                sp->get_zone_id().as_caller(),
                sp->get_destination_zone_id(),
                rpc::casting_interface::get_object_id(*foo_obj),
                0, // interface_id - this is for the i_foo interface
                0, // method_id - this would be specific to the method
                rpc::post_options::normal,
                0,       // in_size
                nullptr, // in_buf
                {});     // in_back_channel

            // Since this is fire-and-forget, we just verify no crash occurred
            // The actual behavior will depend on the implementation
            CO_RETURN true;
        });
}

// Test post functionality with zone_terminating option
TYPED_TEST(post_functionality_test, post_with_zone_terminating)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create a foo object to test with
            rpc::shared_ptr<xxx::i_foo> foo_obj;
            CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
            CORO_ASSERT_NE(foo_obj, nullptr);

            // Get the service proxy for the foo object to use the post functionality
            auto sp = rpc::casting_interface::get_service_proxy(foo_obj);
            CORO_ASSERT_NE(sp, nullptr);

            // Perform a post operation with zone_terminating option using the service proxy directly
            CO_AWAIT sp->post(rpc::get_version(),
                rpc::encoding::enc_default,
                0, // tag
                sp->get_zone_id().as_caller(),
                sp->get_destination_zone_id(),
                rpc::casting_interface::get_object_id(*foo_obj),
                0, // interface_id
                0, // method_id
                rpc::post_options::zone_terminating,
                0,       // in_size
                nullptr, // in_buf
                {});     // in_back_channel

            // Verify no crash occurred
            CO_RETURN true;
        });
}

// Test post functionality with release_optimistic option
TYPED_TEST(post_functionality_test, post_with_release_optimistic)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create a foo object to test with
            rpc::shared_ptr<xxx::i_foo> foo_obj;
            CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
            CORO_ASSERT_NE(foo_obj, nullptr);

            // Get the service proxy for the foo object to use the post functionality
            auto sp = rpc::casting_interface::get_service_proxy(foo_obj);
            CORO_ASSERT_NE(sp, nullptr);

            // Perform a post operation with release_optimistic option using the service proxy directly
            CO_AWAIT sp->post(rpc::get_version(),
                rpc::encoding::enc_default,
                0, // tag
                sp->get_zone_id().as_caller(),
                sp->get_destination_zone_id(),
                rpc::casting_interface::get_object_id(*foo_obj),
                0, // interface_id
                0, // method_id
                rpc::post_options::release_optimistic,
                0,       // in_size
                nullptr, // in_buf
                {});     // in_back_channel

            // Verify no crash occurred
            CO_RETURN true;
        });
}

// Test multiple concurrent post operations
TYPED_TEST(post_functionality_test, concurrent_post_operations)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create multiple foo objects to test with
            std::vector<rpc::shared_ptr<xxx::i_foo>> foo_objects;
            for (int i = 0; i < 5; ++i)
            {
                rpc::shared_ptr<xxx::i_foo> foo_obj;
                CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
                CORO_ASSERT_NE(foo_obj, nullptr);
                foo_objects.push_back(foo_obj);
            }

            // Perform multiple post operations concurrently
            for (size_t i = 0; i < foo_objects.size(); ++i)
            {
                // Get the service proxy for each foo object to use the post functionality
                auto sp = rpc::casting_interface::get_service_proxy(foo_objects[i]);
                CORO_ASSERT_NE(sp, nullptr);

                // Each post operation is fire-and-forget using the service proxy directly
                CO_AWAIT sp->post(rpc::get_version(),
                    rpc::encoding::enc_default,
                    0, // tag
                    sp->get_zone_id().as_caller(),
                    sp->get_destination_zone_id(),
                    rpc::casting_interface::get_object_id(*foo_objects[i]),
                    0, // interface_id
                    0, // method_id
                    rpc::post_options::normal,
                    0,       // in_size
                    nullptr, // in_buf
                    {});     // in_back_channel
            }

            // Verify no crash occurred
            CO_RETURN true;
        });
}

// Test post functionality with different data sizes
TYPED_TEST(post_functionality_test, post_with_different_data_sizes)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create a foo object to test with
            rpc::shared_ptr<xxx::i_foo> foo_obj;
            CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
            CORO_ASSERT_NE(foo_obj, nullptr);

            // Test with different data sizes
            std::vector<std::string> test_data = {
                "small",
                std::string(100, 'x'), // Medium size
                std::string(1000, 'y') // Large size
            };

            for (size_t i = 0; i < test_data.size(); ++i)
            {
                // Get the service proxy for the foo object to use the post functionality
                auto sp = rpc::casting_interface::get_service_proxy(foo_obj);
                CORO_ASSERT_NE(sp, nullptr);

                // Perform post operation with different data sizes - fire-and-forget
                // Using the service proxy directly with the string data as payload
                CO_AWAIT sp->post(rpc::get_version(),
                    rpc::encoding::enc_default,
                    0, // tag
                    sp->get_zone_id().as_caller(),
                    sp->get_destination_zone_id(),
                    rpc::casting_interface::get_object_id(*foo_obj),
                    0, // interface_id
                    0, // method_id
                    rpc::post_options::normal,
                    test_data[i].size(),  // in_size
                    test_data[i].c_str(), // in_buf
                    {});                  // in_back_channel
            }

            // Verify no crash occurred
            CO_RETURN true;
        });
}

// Test that post operations don't affect regular RPC calls
TYPED_TEST(post_functionality_test, post_does_not_interfere_with_regular_calls)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create a foo object to test with
            rpc::shared_ptr<xxx::i_foo> foo_obj;
            CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
            CORO_ASSERT_NE(foo_obj, nullptr);

            // Perform several post operations
            for (int i = 0; i < 10; ++i)
            {
                // Get the service proxy for the foo object to use the post functionality
                auto sp = rpc::casting_interface::get_service_proxy(foo_obj);
                CORO_ASSERT_NE(sp, nullptr);

                // Perform post operation - fire-and-forget using the service proxy directly
                CO_AWAIT sp->post(rpc::get_version(),
                    rpc::encoding::enc_default,
                    0, // tag
                    sp->get_zone_id().as_caller(),
                    sp->get_destination_zone_id(),
                    rpc::casting_interface::get_object_id(*foo_obj),
                    0, // interface_id
                    0, // method_id
                    rpc::post_options::normal,
                    0,       // in_size
                    nullptr, // in_buf
                    {});     // in_back_channel
            }

            // Now perform regular RPC calls to ensure they still work
            int result = 0;
            CORO_ASSERT_EQ(CO_AWAIT foo_obj->do_something_in_val_out_val(5, result), 0);
            CORO_ASSERT_EQ(result, 5);

            // Create baz interface through regular call
            rpc::shared_ptr<xxx::i_baz> baz;
            CORO_ASSERT_EQ(CO_AWAIT foo_obj->create_baz_interface(baz), 0);
            CORO_ASSERT_NE(baz, nullptr);

            CO_RETURN true;
        });
}

// Test post operations with optimistic pointer
TYPED_TEST(post_functionality_test, post_with_optimistic_ptr)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();

    run_coro_test(*this,
        [root_service, &lib](auto& lib_param)
        {
            (void)lib_param; // lib_param not used in this test

            // Get example object (local or remote depending on setup)
            auto example = lib.get_example();
            CORO_ASSERT_NE(example, nullptr);

            // Create a foo object to test with
            rpc::shared_ptr<xxx::i_foo> foo_obj;
            CORO_ASSERT_EQ(CO_AWAIT example->create_foo(foo_obj), 0);
            CORO_ASSERT_NE(foo_obj, nullptr);

            // Create an optimistic pointer
            rpc::optimistic_ptr<xxx::i_foo> opt_foo;
            auto err = CO_AWAIT rpc::make_optimistic(foo_obj, opt_foo);
            CORO_ASSERT_EQ(err, rpc::error::OK());

            // Get the service proxy for the foo object to use the post functionality
            auto sp = rpc::casting_interface::get_service_proxy(foo_obj);
            CORO_ASSERT_NE(sp, nullptr);

            // Perform post operation through service proxy directly (not through optimistic pointer)
            CO_AWAIT sp->post(rpc::get_version(),
                rpc::encoding::enc_default,
                0, // tag
                sp->get_zone_id().as_caller(),
                sp->get_destination_zone_id(),
                rpc::casting_interface::get_object_id(*foo_obj),
                0, // interface_id
                0, // method_id
                rpc::post_options::normal,
                0,       // in_size
                nullptr, // in_buf
                {});     // in_back_channel

            // Verify no crash occurred
            CO_RETURN true;
        });
}
