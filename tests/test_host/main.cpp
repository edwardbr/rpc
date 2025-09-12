/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <iostream>
#include <unordered_map>
#include <string_view>
#include <thread>

#include <common/foo_impl.h>
#include <common/tests.h>

#include <rpc/coroutine_support.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#ifdef BUILD_COROUTINE
#include "common/tcp/service_proxy.h"
#include "common/tcp/listener.h"

#include "common/spsc/service_proxy.h"
#include "common/spsc/channel_manager.h"

#include <coro/io_scheduler.hpp>
#endif

#include "rpc_global_logger.h"

#ifdef BUILD_COROUTINE
#include "common/tcp/service_proxy.h"
#include "common/tcp/listener.h"

#include "common/spsc/service_proxy.h"
#include "common/spsc/channel_manager.h"

#include <coro/io_scheduler.hpp>
#endif

#include "rpc_global_logger.h"
#include <rpc/error_codes.h>
#include <rpc/casting_interface.h>

// Include extracted setup classes
#include "test_host.h"
#include "in_memory_setup.h"
#include "inproc_setup.h"
#include "enclave_setup.h"

#ifdef BUILD_COROUTINE
#include "tcp_setup.h"
#include "spsc_setup.h"
#endif
#include "crash_handler.h"

// This list should be kept sorted.
using testing::_;
using testing::Action;
using testing::ActionInterface;
using testing::Assign;
using testing::ByMove;
using testing::ByRef;
using testing::DoDefault;
using testing::get;
using testing::IgnoreResult;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::make_tuple;
using testing::MakePolymorphicAction;
using testing::Ne;
using testing::PolymorphicAction;
using testing::Return;
using testing::ReturnNull;
using testing::ReturnRef;
using testing::ReturnRefOfCopy;
using ::testing::Sequence;
using testing::SetArgPointee;
using testing::SetArgumentPointee;
using testing::tuple;
using testing::tuple_element;
using testing::Types;

using namespace marshalled_tests;

// Global variables (defined here and declared in headers)
#ifdef _WIN32 // windows
std::string enclave_path = "./marshal_test_enclave.signed.dll";
#else // Linux
std::string enclave_path = "./libmarshal_test_enclave.signed.so";
#endif

#ifdef USE_RPC_TELEMETRY
TELEMETRY_SERVICE_MANAGER
#endif
bool enable_telemetry_server = false;
bool enable_multithreaded_tests = false;

rpc::weak_ptr<rpc::service> current_host_service;
std::atomic<uint64_t>* zone_gen = nullptr;

// This line tests that we can define tests in an unnamed namespace.
namespace
{

    extern "C" int main(int argc, char* argv[])
    {
        // Parse command line arguments
        for (size_t i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-t" || arg == "--enable_telemetry_server")
                enable_telemetry_server = true;
            if (arg == "-m" || arg == "--enable_multithreaded_tests")
                enable_multithreaded_tests = true;
        }

#ifndef _WIN32
        // Initialize comprehensive crash handler with multi-threaded support
        // (Only available on POSIX systems - Windows not supported)
        crash_handler::crash_handler::Config crash_config;
        crash_config.enable_multithreaded_traces = true;
        crash_config.enable_symbol_resolution = true;
        crash_config.enable_threading_debug_info = true;
        crash_config.enable_pattern_detection = true;
        crash_config.max_stack_frames = 64;
        crash_config.max_threads = 50;
        crash_config.save_crash_dump = true;
        crash_config.crash_dump_path = "./build/crash";

        // Set up custom crash analysis callback
        crash_handler::crash_handler::set_analysis_callback(
            [](const crash_handler::crash_handler::crash_report& report) {
                std::cout << "\n=== CUSTOM CRASH ANALYSIS ===" << std::endl;
                std::cout << "Crash occurred at: " << std::chrono::duration_cast<std::chrono::seconds>(
                    report.crash_time.time_since_epoch()).count() << std::endl;
                
                // Check for threading debug patterns
                bool threading_bug_detected = false;
                for (const auto& pattern : report.detected_patterns) {
                    if (pattern.find("THREADING") != std::string::npos || 
                        pattern.find("ZONE PROXY") != std::string::npos) {
                        threading_bug_detected = true;
                        break;
                    }
                }
                
                if (threading_bug_detected) {
                    std::cout << "🐛 THREADING BUG CONFIRMED: This crash was caused by a race condition!" << std::endl;
                    std::cout << "   The threading debug system successfully detected the issue." << std::endl;
                } else {
                    std::cout << "ℹ️  Standard crash - not detected as threading-related." << std::endl;
                }
                
                std::cout << "=== END CUSTOM ANALYSIS ===" << std::endl;
            }
        );

        if (!crash_handler::crash_handler::initialize(crash_config)) {
            std::cerr << "Failed to initialize crash handler" << std::endl;
            return 1;
        }

        std::cout << "[Main] Comprehensive crash handler initialized" << std::endl;
        std::cout << "[Main] - Multi-threaded stack traces: ENABLED" << std::endl;
        std::cout << "[Main] - Symbol resolution: ENABLED" << std::endl;
        std::cout << "[Main] - Threading debug integration: ENABLED" << std::endl;
        std::cout << "[Main] - Pattern detection: ENABLED" << std::endl;
        std::cout << "[Main] - Crash dumps will be saved to: " << crash_config.crash_dump_path << std::endl;
#else
        std::cout << "[Main] Crash handler not available on Windows" << std::endl;
#endif

        int ret = 0;
        try {
            // Initialize global logger for consistent logging
            rpc_global_logger::get_logger();
            ::testing::InitGoogleTest(&argc, argv);
            ret = RUN_ALL_TESTS();
            rpc_global_logger::reset_logger();
        } catch (const std::exception& e) {
            std::cerr << "Exception caught in main: " << e.what() << std::endl;
            ret = 1;
        } catch (...) {
            std::cerr << "Unknown exception caught in main" << std::endl;
            ret = 1;
        }

        // Cleanup crash handler
#ifndef _WIN32
        crash_handler::crash_handler::shutdown();
#endif
        std::cout << "[Main] test shutdown complete" << std::endl;

        return ret;
    }
}

template<class T> class type_test : public testing::Test
{
    T lib_;

public:
    T& get_lib() { return lib_; }

    void SetUp() override { this->lib_.set_up(); }
    void TearDown() override { this->lib_.tear_down(); }
};

using local_implementations = ::testing::Types<in_memory_setup<false>,
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
    tcp_setup<false, false, false>,
    tcp_setup<false, false, true>,
    tcp_setup<false, true, false>,
    tcp_setup<false, true, true>,
    tcp_setup<true, false, false>,
    tcp_setup<true, false, true>,
    tcp_setup<true, true, false>,
    tcp_setup<true, true, true>,
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
TYPED_TEST_SUITE(type_test, local_implementations);

TYPED_TEST(type_test, initialisation_test) { }

template<class T> CORO_TASK(bool) coro_standard_tests(bool& is_ready, T& lib)
{
    auto root_service = lib.get_root_service();

    rpc::zone zone_id;
    if (root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};

    foo f(zone_id);

    CO_AWAIT standard_tests(f, lib.get_has_enclave());
    is_ready = true;
    CO_RETURN !lib.error_has_occured();
}

TYPED_TEST(type_test, standard_tests)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
#ifdef BUILD_COROUTINE      
    lib.get_scheduler()->schedule(lib.check_for_error(coro_standard_tests(is_ready, lib)));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
#else
    coro_standard_tests(is_ready, lib);
#endif    
    ASSERT_EQ(lib.error_has_occured(), false);
}

TEST_RETURN_VAL dyanmic_cast_tests(bool& is_ready, rpc::shared_ptr<rpc::service> root_service)
{
    rpc::zone zone_id;
    if (root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo(zone_id));

    rpc::shared_ptr<xxx::i_baz> baz;
    CORO_ASSERT_EQ(CO_AWAIT f->create_baz_interface(baz), 0);
    CORO_ASSERT_EQ(CO_AWAIT f->call_baz_interface(nullptr), 0); // feed in a nullptr
    CORO_ASSERT_EQ(CO_AWAIT f->call_baz_interface(baz), 0);     // feed back to the implementation

    {
        // test for identity
        auto x = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
        CORO_ASSERT_NE(x, nullptr);
        CORO_ASSERT_EQ(x, baz);
        auto y = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
        CORO_ASSERT_NE(y, nullptr);
        CO_AWAIT y->do_something_else(1);
        CORO_ASSERT_NE(y, nullptr);
        auto z = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
        CORO_ASSERT_EQ(z, nullptr);
    }
    // retest
    {
        auto x = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
        CORO_ASSERT_NE(x, nullptr);
        CORO_ASSERT_EQ(x, baz);
        auto y = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
        CORO_ASSERT_NE(y, nullptr);
        CO_AWAIT y->do_something_else(1);
        CORO_ASSERT_NE(y, nullptr);
        auto z = CO_AWAIT rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
        CORO_ASSERT_EQ(z, nullptr);
    }
    is_ready = true;
    TEST_RETURN_SUCCESFUL;
}

TYPED_TEST(type_test, dyanmic_cast_tests)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    // TEST_SYNC_WAIT(dyanmic_cast_tests(root_service));

    bool is_ready = false;
#ifdef BUILD_COROUTINE  
    lib.get_scheduler()->schedule(lib.check_for_error(dyanmic_cast_tests(is_ready, root_service)));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    };
#else
    dyanmic_cast_tests(is_ready, root_service);
#endif    

    ASSERT_EQ(lib.error_has_occured(), false);
}

template<class T> using remote_type_test = type_test<T>;

typedef Types<
    // inproc_setup<false, false, false>,

    inproc_setup<true, false, false>,
    inproc_setup<true, false, true>,
    inproc_setup<true, true, false>,
    inproc_setup<true, true, true>
#ifdef BUILD_COROUTINE
    ,
    tcp_setup<true, false, false>,
    tcp_setup<true, false, true>,
    tcp_setup<true, true, false>,
    tcp_setup<true, true, true>,
    spsc_setup<true, false, false>,
    spsc_setup<true, false, true>,
    spsc_setup<true, true, false>,
    spsc_setup<true, true, true>
#endif

#ifdef BUILD_ENCLAVE
    ,
    enclave_setup<true, false, false>,
    enclave_setup<true, false, true>,
    enclave_setup<true, true, false>,
    enclave_setup<true, true, true>
#endif
    >
    remote_implementations;
TYPED_TEST_SUITE(remote_type_test, remote_implementations);

template<class T> CORO_TASK(void) coro_remote_standard_tests(bool& is_ready, T& lib)
{
    rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    auto ret = CO_AWAIT lib.get_example()->create_foo(i_foo_ptr);
    if (ret != rpc::error::OK())
    {
        LOG_CSTR("failed create_foo");
        CO_RETURN;
    }
    CO_AWAIT standard_tests(*i_foo_ptr, lib.get_has_enclave());
    is_ready = true;
}

TYPED_TEST(remote_type_test, remote_standard_tests)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
#ifdef BUILD_COROUTINE  
    lib.get_scheduler()->schedule(coro_remote_standard_tests(is_ready, lib));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
#else
    coro_remote_standard_tests(is_ready, lib);
#endif    
    ASSERT_EQ(lib.error_has_occured(), false);
}

// TYPED_TEST(remote_type_test, multithreaded_standard_tests)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             standard_tests(*i_foo_ptr, true);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, multithreaded_standard_tests_with_and_foos)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//             ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
//             standard_tests(*i_foo_ptr, true);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

template<class T> CORO_TASK(void) coro_remote_tests(bool& is_ready, T& lib, rpc::zone zone_id)
{
    CO_AWAIT remote_tests(lib.get_use_host_in_child(), lib.get_example(), zone_id);
    is_ready = true;
}

TYPED_TEST(remote_type_test, remote_tests)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    rpc::zone zone_id;
    if (root_service)
        zone_id = root_service->get_zone_id();
    else
        zone_id = {0};
#ifdef BUILD_COROUTINE                
    lib.get_scheduler()->schedule(coro_remote_tests(is_ready, lib, zone_id));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
#else
    coro_remote_tests(is_ready, lib, zone_id);
#endif    
    ASSERT_EQ(lib.error_has_occured(), false);
}

// TYPED_TEST(remote_type_test, multithreaded_remote_tests)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             remote_tests(lib.get_use_host_in_child(), lib.get_example(), zone_id);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

template<class T> CORO_TASK(void) coro_create_new_zone(bool& is_ready, T& lib, const rpc::shared_ptr<yyy::i_host>& host)
{
    auto example_relay_ptr = CO_AWAIT lib.create_new_zone();
    CO_AWAIT example_relay_ptr->set_host(host);
    is_ready = true;
}

TYPED_TEST(remote_type_test, create_new_zone)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    rpc::shared_ptr<yyy::i_host> host;
#ifdef BUILD_COROUTINE        
    lib.get_scheduler()->schedule(coro_create_new_zone(is_ready, lib, host));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
#else
    coro_create_new_zone(is_ready, lib, host);
#endif    
    ASSERT_EQ(lib.error_has_occured(), false);
}

// TYPED_TEST(remote_type_test, multithreaded_create_new_zone)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             auto example_relay_ptr = lib.create_new_zone();
//             example_relay_ptr->set_host(nullptr);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

template<class T>
CORO_TASK(void)
coro_create_new_zone_releasing_host_then_running_on_other_enclave(
    bool& is_ready, T& lib, const rpc::shared_ptr<yyy::i_host>& host)
{
    GTEST_SKIP() << "for later";
    auto example_relay_ptr = CO_AWAIT lib.create_new_zone();
    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    CO_AWAIT example_relay_ptr->create_foo(i_foo_relay_ptr);
    // CO_AWAIT standard_tests(*i_foo_relay_ptr, true);
    
    // rpc::shared_ptr<yyy::i_host> host_copy;
    // CO_AWAIT example_relay_ptr->get_host(host_copy);
            
    // {
    //     auto example_o = rpc::casting_interface::get_object_id(*example_relay_ptr);
    //     auto foo_o = rpc::casting_interface::get_object_id(*i_foo_relay_ptr);
    //     auto host_o = rpc::casting_interface::get_object_id(*host_copy);
    //     auto example_z = rpc::casting_interface::get_destination_zone(*example_relay_ptr);
    //     auto foo_z = rpc::casting_interface::get_destination_zone(*i_foo_relay_ptr);
    //     auto host_z = rpc::casting_interface::get_destination_zone(*host_copy);
    //     yyy::i_host* p_host = host_copy.get();
    //     p_host = p_host;
    //     foo_o = foo_o;
    //     example_o = example_o;
    //     host_o = host_o;
    //     foo_z = foo_z;
    //     example_z = example_z;
    //     host_z = host_z;
    // }

    // rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    // ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
    example_relay_ptr = nullptr;
    i_foo_relay_ptr = nullptr;

    // standard_tests(*i_foo_ptr, true);

#ifdef BUILD_COROUTINE    
    auto success = lib.get_root_service()->schedule(
        [&is_ready]() -> CORO_TASK(void)
        {
            is_ready = true;
            CO_RETURN;
        }());
#endif        
}

TYPED_TEST(remote_type_test, create_new_zone_releasing_host_then_running_on_other_enclave)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    rpc::shared_ptr<yyy::i_host> host;
#ifdef BUILD_COROUTINE    
    lib.get_scheduler()->schedule(
        coro_create_new_zone_releasing_host_then_running_on_other_enclave(is_ready, lib, host));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
#else
    coro_create_new_zone_releasing_host_then_running_on_other_enclave(is_ready, lib, host);
#endif    
    ASSERT_EQ(lib.error_has_occured(), false);
}

// TYPED_TEST(remote_type_test, multithreaded_create_new_zone_releasing_host_then_running_on_other_enclave)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//             auto example_relay_ptr = lib.create_new_zone();
//             example_relay_ptr->create_foo(i_foo_relay_ptr);
//             standard_tests(*i_foo_relay_ptr, true);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, dyanmic_cast_tests)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     rpc::shared_ptr<xxx::i_baz> baz;
//     i_foo_ptr->create_baz_interface(baz);
//     i_foo_ptr->call_baz_interface(nullptr); // feed in a nullptr
//     i_foo_ptr->call_baz_interface(baz);     // feed back to the implementation

//     auto x = rpc::dynamic_pointer_cast<xxx::i_baz>(baz);
//     ASSERT_NE(x, nullptr);
//     auto y = rpc::dynamic_pointer_cast<xxx::i_bar>(baz);
//     ASSERT_NE(y, nullptr);
//     y->do_something_else(1);
//     auto z = rpc::dynamic_pointer_cast<xxx::i_foo>(baz);
//     ASSERT_EQ(z, nullptr);
// }

// TYPED_TEST(remote_type_test, bounce_baz_between_two_interfaces)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//     auto example_relay_ptr = lib.create_new_zone();
//     example_relay_ptr->create_foo(i_foo_relay_ptr);

//     rpc::shared_ptr<xxx::i_baz> baz;
//     i_foo_ptr->create_baz_interface(baz);
//     i_foo_relay_ptr->call_baz_interface(baz);
// }

// TYPED_TEST(remote_type_test, multithreaded_bounce_baz_between_two_interfaces)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//             ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//             rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
//             auto example_relay_ptr = lib.create_new_zone();
//             example_relay_ptr->create_foo(i_foo_relay_ptr);

//             rpc::shared_ptr<xxx::i_baz> baz;
//             i_foo_ptr->create_baz_interface(baz);
//             i_foo_relay_ptr->call_baz_interface(baz);
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// // check for null
// TYPED_TEST(remote_type_test, check_for_null_interface)
// {
//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
//     rpc::shared_ptr<xxx::i_baz> c;
//     i_foo_ptr->get_interface(c);
//     RPC_ASSERT(c == nullptr);
// }

// TYPED_TEST(remote_type_test, check_for_multiple_sets)
// {
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     auto zone_id = casting_interface::get_zone_id(*i_foo_ptr);

//     auto b = rpc::make_shared<marshalled_tests::baz>(zone_id);
//     // set
//     i_foo_ptr->set_interface(b);
//     // reset
//     i_foo_ptr->set_interface(nullptr);
//     // set
//     i_foo_ptr->set_interface(b);
//     // reset
//     i_foo_ptr->set_interface(nullptr);
// }

// TYPED_TEST(remote_type_test, check_for_interface_storage)
// {
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
//     ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);

//     rpc::shared_ptr<xxx::i_baz> c;
//     auto zone_id = casting_interface::get_zone_id(i_foo_ptr);

//     auto b = rpc::make_shared<marshalled_tests::baz>(zone_id);
//     i_foo_ptr->set_interface(b);
//     i_foo_ptr->get_interface(c);
//     i_foo_ptr->set_interface(nullptr);
//     RPC_ASSERT(b == c);
// }

// TYPED_TEST(remote_type_test, check_for_set_multiple_inheritance)
// {
//     if(!lib.get_use_host_in_child())
//         return;
//     auto proxy = lib.get_example()->query_proxy_base();
//     auto ret = lib.get_example()->give_interface(
//         rpc::shared_ptr<xxx::i_baz>(new multiple_inheritance(casting_interface::get_zone_id(proxy->get_object_proxy())));
//     RPC_ASSERT(ret == rpc::error::OK());
// }

// #ifdef BUILD_ENCLAVE
// TYPED_TEST(remote_type_test, host_test)
// {
//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     auto h = rpc::make_shared<host>(zone_id);

//     rpc::shared_ptr<yyy::i_example> target;
//     rpc::shared_ptr<yyy::i_example> target2;
//     h->create_enclave(target);
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(h->set_app("target", target), rpc::error::OK());
//     ASSERT_EQ(h->look_up_app("target", target2), rpc::error::OK());
//     ASSERT_EQ(h->unload_app("target"), rpc::error::OK());
//     target = nullptr;
//     target2 = nullptr;

// }

// TYPED_TEST(remote_type_test, check_for_call_enclave_zone)
// {
//     if(!lib.get_use_host_in_child())
//         return;

//     auto root_service = lib.get_root_service();
//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};

//     auto h = rpc::make_shared<host>(zone_id);
//     auto ret = lib.get_example()->call_create_enclave_val(h);
//     RPC_ASSERT(ret == rpc::error::OK());
// }
// #endif

// TYPED_TEST(remote_type_test, check_sub_subordinate)
// {
//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<yyy::i_example> new_zone;
//     ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//     rpc::shared_ptr<yyy::i_example> new_new_zone;
//     ASSERT_EQ(new_zone->create_example_in_subordinate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)),
//     rpc::error::OK()); //third level
// }

// TYPED_TEST(remote_type_test, multithreaded_check_sub_subordinate)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;
//     std::vector<std::thread> threads(lib.is_enclave_setup() ? 3 : 100);
//     for(auto& thread_target : threads)
//     {
//         thread_target = std::thread([&](){
//             rpc::shared_ptr<yyy::i_example> new_zone;
//             ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//             rpc::shared_ptr<yyy::i_example> new_new_zone;
//             ASSERT_EQ(new_zone->create_example_in_subordinate_zone(new_new_zone, lib.get_local_host_ptr(),
//             ++(*zone_gen)), rpc::error::OK()); //third level
//         });
//     }
//     for(auto& thread_target : threads)
//     {
//         thread_target.join();
//     }
// }

// TYPED_TEST(remote_type_test, send_interface_back)
// {
//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_baz> output;

//     rpc::shared_ptr<yyy::i_example> new_zone;
//     //lib.i_example_ptr_ //first level
//     ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//     rpc::shared_ptr<xxx::i_baz> new_baz;
//     new_zone->create_baz(new_baz);

//     ASSERT_EQ(lib.get_example()->send_interface_back(new_baz, output), rpc::error::OK());
//     ASSERT_EQ(new_baz, output);
// }

// TYPED_TEST(remote_type_test, two_zones_get_one_to_lookup_other)
// {
//     auto root_service = lib.get_root_service();

//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     auto h = lib.get_local_host_ptr();
//     auto ex = lib.get_example();

//     auto enclaveb = lib.create_new_zone();
//     enclaveb->set_host(h);
//     ASSERT_EQ(h->set_app("enclaveb", enclaveb), rpc::error::OK());

//     ex->call_host_look_up_app_not_return("enclaveb", false);

//     enclaveb->set_host(nullptr);
// }
// TYPED_TEST(remote_type_test, multithreaded_two_zones_get_one_to_lookup_other)
// {
//     if(!enable_multithreaded_tests || lib.is_enclave_setup())
//     {
//         GTEST_SKIP() << "multithreaded tests are skipped";
//         return;
//     }

//     auto root_service = lib.get_root_service();

//     rpc::zone zone_id;
//     if(root_service)
//         zone_id = root_service->get_zone_id();
//     else
//         zone_id = {0};
//     auto h = rpc::make_shared<host>(zone_id);

//     auto enclavea = lib.create_new_zone();
//     enclavea->set_host(h);
//     ASSERT_EQ(h->set_app("enclavea", enclavea), rpc::error::OK());

//     auto enclaveb = lib.create_new_zone();
//     enclaveb->set_host(h);
//     ASSERT_EQ(h->set_app("enclaveb", enclaveb), rpc::error::OK());

//     const auto thread_size = 3;
//     std::array<std::thread, thread_size> threads;
//     for(auto& thread : threads)
//     {
//         thread = std::thread([&](){
//             enclavea->call_host_look_up_app_not_return("enclaveb", true);
//         });
//     }
//     for(auto& thread : threads)
//     {
//         thread.join();
//     }
//     enclavea->set_host(nullptr);
//     enclaveb->set_host(nullptr);
// }

// TYPED_TEST(remote_type_test, check_identity)
// {
//     auto& lib = lib;
//     if(!lib.get_use_host_in_child())
//         return;

//     rpc::shared_ptr<xxx::i_baz> output;

//     rpc::shared_ptr<yyy::i_example> new_zone;
//     //lib.i_example_ptr_ //first level
//     ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //second level

//     rpc::shared_ptr<yyy::i_example> new_new_zone;
//     ASSERT_EQ(new_zone->create_example_in_subordinate_zone(new_new_zone, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK()); //third level

//     auto new_zone_fork = lib.create_new_zone();//second level

//     rpc::shared_ptr<xxx::i_baz> new_baz;
//     new_zone->create_baz(new_baz);

//     rpc::shared_ptr<xxx::i_baz> new_new_baz;
//     new_new_zone->create_baz(new_new_baz);

//     rpc::shared_ptr<xxx::i_baz> new_baz_fork;
//     new_zone_fork->create_baz(new_baz_fork);

//     // topology looks like this now flinging bazes around these nodes to ensure that the identity of bazes is the same
//     // *4                           #
//     //  \                           #
//     //   *3                         #
//     //    \                         #
//     //     *2  *5                   #
//     //      \ /                     #
//     //       1                      #

//     auto proxy = lib.get_example()->query_proxy_base();
//     auto base_baz = rpc::shared_ptr<xxx::i_baz>(new
//     baz(casting_interface::get_zone_id(*proxy)); auto input = base_baz;

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     input = new_baz;

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     input = new_new_baz;

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     input = new_baz_fork;

//     // *z2   *i5                 #
//     //  \  /                     #
//     //   h1                      #

//     ASSERT_EQ(lib.get_example()->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     // *z3                       #
//     //  \                        #
//     //   *2   *i5                #
//     //    \ /                    #
//     //     h1                    #

//     ASSERT_EQ(new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

//     // *z4                          #
//     //  \                           #
//     //   *3                         #
//     //    \                         #
//     //     *2  *i5                  #
//     //      \ /                     #
//     //       h1                     #

    // ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
    // ASSERT_EQ(input, output);
// }

TYPED_TEST(remote_type_test, check_deeply_nested_zone_reference_counting_fork_scenario)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Create a complex branching topology to test untested path on line 792
    // This tests scenario where dest_channel == caller_channel && build_channel
    //
    // Target topology:
    //     *6     *7
    //      \    /
    //       *4 *5     *8
    //        \ |     /
    //         \|    /
    //          *2  *3
    //           \ /
    //            *1 (root)

    // Create the initial hierarchy
    rpc::shared_ptr<yyy::i_example> level2_left;
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(level2_left, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level2_right;  
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(level2_right, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level4_left;
    ASSERT_EQ(level2_left->create_example_in_subordinate_zone(level4_left, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level5_shared;
    ASSERT_EQ(level2_left->create_example_in_subordinate_zone(level5_shared, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level8_isolated;
    ASSERT_EQ(level2_right->create_example_in_subordinate_zone(level8_isolated, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Create deeply nested child zones
    rpc::shared_ptr<yyy::i_example> level6_deep;
    ASSERT_EQ(level4_left->create_example_in_subordinate_zone(level6_deep, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level7_deep;
    ASSERT_EQ(level5_shared->create_example_in_subordinate_zone(level7_deep, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Create objects in different zones
    rpc::shared_ptr<xxx::i_baz> baz_from_level6;
    level6_deep->create_baz(baz_from_level6);

    rpc::shared_ptr<xxx::i_baz> baz_from_level7;  
    level7_deep->create_baz(baz_from_level7);

    rpc::shared_ptr<xxx::i_baz> baz_from_level8;
    level8_isolated->create_baz(baz_from_level8);

    // Test complex routing scenarios that should trigger the untested path
    // Pass interface from level6 to level8 via level2_right - this should create
    // a scenario where the channels converge (dest_channel == caller_channel)
    rpc::shared_ptr<xxx::i_baz> output;
    
    // This routing should exercise the fork logic in service::add_ref line 792
    ASSERT_EQ(level8_isolated->send_interface_back(baz_from_level6, output), rpc::error::OK());
    ASSERT_EQ(baz_from_level6, output);

    // Cross-branch routing from level7 to level8
    ASSERT_EQ(level8_isolated->send_interface_back(baz_from_level7, output), rpc::error::OK()); 
    ASSERT_EQ(baz_from_level7, output);

    // Test reverse routing that might expose the second untested path
    ASSERT_EQ(level6_deep->send_interface_back(baz_from_level8, output), rpc::error::OK());
    ASSERT_EQ(baz_from_level8, output);
}

TYPED_TEST(remote_type_test, check_unknown_zone_reference_path)  
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Create a topology designed to trigger the untested path on line 870
    // This tests scenario where a zone doesn't know about the caller zone
    //
    // Target topology:
    //   *A3    *B3
    //    |      |   
    //   *A2    *B2
    //    |      |
    //   *A1    *B1
    //     \    /
    //      root

    // Create two separate branch hierarchies
    rpc::shared_ptr<yyy::i_example> branchA_level1;
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(branchA_level1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> branchB_level1; 
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(branchB_level1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Extend each branch deeper
    rpc::shared_ptr<yyy::i_example> branchA_level2;
    ASSERT_EQ(branchA_level1->create_example_in_subordinate_zone(branchA_level2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> branchA_level3;
    ASSERT_EQ(branchA_level2->create_example_in_subordinate_zone(branchA_level3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> branchB_level2;
    ASSERT_EQ(branchB_level1->create_example_in_subordinate_zone(branchB_level2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> branchB_level3;
    ASSERT_EQ(branchB_level2->create_example_in_subordinate_zone(branchB_level3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Create objects in the deepest zones  
    rpc::shared_ptr<xxx::i_baz> baz_from_branchA;
    branchA_level3->create_baz(baz_from_branchA);

    rpc::shared_ptr<xxx::i_baz> baz_from_branchB;
    branchB_level3->create_baz(baz_from_branchB);

    // This cross-branch communication should trigger the unknown zone path
    // when branchB_level3 tries to route back to branchA_level3 but doesn't 
    // know about branchA's hierarchy
    rpc::shared_ptr<xxx::i_baz> output;
    
    // This should exercise the unknown caller zone logic on line 870
    ASSERT_EQ(branchB_level3->send_interface_back(baz_from_branchA, output), rpc::error::OK());
    ASSERT_EQ(baz_from_branchA, output);

    // Reverse direction
    ASSERT_EQ(branchA_level3->send_interface_back(baz_from_branchB, output), rpc::error::OK());
    ASSERT_EQ(baz_from_branchB, output);

    // Test intermediate level cross-communication  
    ASSERT_EQ(branchB_level2->send_interface_back(baz_from_branchA, output), rpc::error::OK());
    ASSERT_EQ(baz_from_branchA, output);

    ASSERT_EQ(branchA_level2->send_interface_back(baz_from_branchB, output), rpc::error::OK());
    ASSERT_EQ(baz_from_branchB, output);
}

// Helper structure to hold the complex topology
struct complex_topology_nodes {
    // Root hierarchy
    rpc::shared_ptr<yyy::i_example> child_1;
    rpc::shared_ptr<yyy::i_example> child_2;
    rpc::shared_ptr<yyy::i_example> child_3;

    // Branch 1 hierarchy
    rpc::shared_ptr<yyy::i_example> grandchild_1_1;
    rpc::shared_ptr<yyy::i_example> grandchild_1_2;
    rpc::shared_ptr<yyy::i_example> grandchild_1_3;
    rpc::shared_ptr<yyy::i_example> grandchild_1_4;

    // Branch 2 hierarchy  
    rpc::shared_ptr<yyy::i_example> grandchild_2_1;
    rpc::shared_ptr<yyy::i_example> grandchild_2_2;
    rpc::shared_ptr<yyy::i_example> grandchild_2_3;
    rpc::shared_ptr<yyy::i_example> grandchild_2_4;

    // Branch 1 great-grandchildren - sub-branch 1
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_1_1;
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_1_2;
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_1_3;
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_1_4;

    // Branch 1 great-grandchildren - sub-branch 2
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_2_1;
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_2_2;
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_2_3;
    rpc::shared_ptr<yyy::i_example> great_grandchild_1_2_4;

    // Branch 2 great-grandchildren - sub-branch 1
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_1_1;
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_1_2;
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_1_3;
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_1_4;

    // Branch 2 great-grandchildren - sub-branch 2
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_2_1;
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_2_2;
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_2_3;
    rpc::shared_ptr<yyy::i_example> great_grandchild_2_2_4;
};

// Helper function to build the complex topology
template<class T>
complex_topology_nodes build_complex_topology(T& test_instance) {
    complex_topology_nodes nodes;
    auto& lib = test_instance.get_lib();

    // Build the root hierarchy
    EXPECT_EQ(lib.get_example()->create_example_in_subordinate_zone(nodes.child_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.child_1->create_example_in_subordinate_zone(nodes.child_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.child_2->create_example_in_subordinate_zone(nodes.child_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Build branch 1 grandchild hierarchy
    EXPECT_EQ(nodes.child_3->create_example_in_subordinate_zone(nodes.grandchild_1_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.grandchild_1_1->create_example_in_subordinate_zone(nodes.grandchild_1_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.grandchild_1_2->create_example_in_subordinate_zone(nodes.grandchild_1_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.grandchild_1_3->create_example_in_subordinate_zone(nodes.grandchild_1_4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Build branch 2 grandchild hierarchy  
    EXPECT_EQ(nodes.child_3->create_example_in_subordinate_zone(nodes.grandchild_2_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.grandchild_2_1->create_example_in_subordinate_zone(nodes.grandchild_2_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.grandchild_2_2->create_example_in_subordinate_zone(nodes.grandchild_2_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.grandchild_2_3->create_example_in_subordinate_zone(nodes.grandchild_2_4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Build branch 1, sub-branch 1 great-grandchild hierarchy
    EXPECT_EQ(nodes.grandchild_1_4->create_example_in_subordinate_zone(nodes.great_grandchild_1_1_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_1_1_1->create_example_in_subordinate_zone(nodes.great_grandchild_1_1_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_1_1_2->create_example_in_subordinate_zone(nodes.great_grandchild_1_1_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_1_1_3->create_example_in_subordinate_zone(nodes.great_grandchild_1_1_4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Build branch 1, sub-branch 2 great-grandchild hierarchy
    EXPECT_EQ(nodes.grandchild_1_4->create_example_in_subordinate_zone(nodes.great_grandchild_1_2_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_1_2_1->create_example_in_subordinate_zone(nodes.great_grandchild_1_2_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_1_2_2->create_example_in_subordinate_zone(nodes.great_grandchild_1_2_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_1_2_3->create_example_in_subordinate_zone(nodes.great_grandchild_1_2_4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Build branch 2, sub-branch 1 great-grandchild hierarchy
    EXPECT_EQ(nodes.grandchild_2_4->create_example_in_subordinate_zone(nodes.great_grandchild_2_1_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_2_1_1->create_example_in_subordinate_zone(nodes.great_grandchild_2_1_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_2_1_2->create_example_in_subordinate_zone(nodes.great_grandchild_2_1_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_2_1_3->create_example_in_subordinate_zone(nodes.great_grandchild_2_1_4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Build branch 2, sub-branch 2 great-grandchild hierarchy
    EXPECT_EQ(nodes.grandchild_2_4->create_example_in_subordinate_zone(nodes.great_grandchild_2_2_1, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_2_2_1->create_example_in_subordinate_zone(nodes.great_grandchild_2_2_2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_2_2_2->create_example_in_subordinate_zone(nodes.great_grandchild_2_2_3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    EXPECT_EQ(nodes.great_grandchild_2_2_3->create_example_in_subordinate_zone(nodes.great_grandchild_2_2_4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    return nodes;
}

TYPED_TEST(remote_type_test, complex_topology_cross_branch_routing_trap_1)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Test 1: Cross-branch routing between deepest nodes without any prior communication
    // This should stress the unknown zone reference path at line 870
    // Strategy: Create object in great_grandchild_1_1_4, route it to great_grandchild_2_2_4
    // These two nodes are in completely separate sub-branches with no established routes

    auto nodes = build_complex_topology(*this);

    // Create test objects in the deepest nodes of different branches
    rpc::shared_ptr<xxx::i_baz> baz_from_1_1_4;
    nodes.great_grandchild_1_1_4->create_baz(baz_from_1_1_4);

    // Critical test: Route from great_grandchild_2_2_4 to great_grandchild_1_1_4
    // This requires routing across: 2_2_4 -> 2_2_3 -> 2_2_2 -> 2_2_1 -> grandchild_2_4 -> 
    // grandchild_2_3 -> grandchild_2_2 -> grandchild_2_1 -> child_3 -> grandchild_1_1 -> 
    // grandchild_1_2 -> grandchild_1_3 -> grandchild_1_4 -> great_grandchild_1_1_1 -> 
    // great_grandchild_1_1_2 -> great_grandchild_1_1_3 -> great_grandchild_1_1_4
    // This should trigger unknown zone reference logic
    rpc::shared_ptr<xxx::i_baz> output;
    ASSERT_EQ(nodes.great_grandchild_2_2_4->send_interface_back(baz_from_1_1_4, output), rpc::error::OK());
    ASSERT_EQ(baz_from_1_1_4, output);
}

TYPED_TEST(remote_type_test, complex_topology_intermediate_channel_collision_trap_2)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Test 2: Create scenario where dest_channel == caller_channel && build_channel
    // This should target the untested path at line 792
    // Strategy: Create routing scenario where channels converge at an intermediate node

    auto nodes = build_complex_topology(*this);

    // Create objects at strategic points to force channel convergence
    rpc::shared_ptr<xxx::i_baz> baz_from_1_2_3;
    nodes.great_grandchild_1_2_3->create_baz(baz_from_1_2_3);

    rpc::shared_ptr<xxx::i_baz> baz_from_2_1_3;
    nodes.great_grandchild_2_1_3->create_baz(baz_from_2_1_3);

    // Route through grandchild_1_4 which is a convergence point for both sub-branches
    // This might create a scenario where routing channels collide
    rpc::shared_ptr<xxx::i_baz> output;
    ASSERT_EQ(nodes.great_grandchild_1_1_2->send_interface_back(baz_from_2_1_3, output), rpc::error::OK());
    ASSERT_EQ(baz_from_2_1_3, output);

    ASSERT_EQ(nodes.great_grandchild_2_1_2->send_interface_back(baz_from_1_2_3, output), rpc::error::OK());
    ASSERT_EQ(baz_from_1_2_3, output);
}

TYPED_TEST(remote_type_test, complex_topology_deep_nesting_parent_fallback_trap_3)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Test 3: Deep nesting with parent fallback failure
    // Force scenarios where get_parent() fallback should fail
    // Strategy: Create routing from the deepest possible nodes

    auto nodes = build_complex_topology(*this);

    // Create objects at maximum depth
    rpc::shared_ptr<xxx::i_baz> baz_1_1_4;
    nodes.great_grandchild_1_1_4->create_baz(baz_1_1_4);

    rpc::shared_ptr<xxx::i_baz> baz_1_2_4;  
    nodes.great_grandchild_1_2_4->create_baz(baz_1_2_4);

    rpc::shared_ptr<xxx::i_baz> baz_2_1_4;
    nodes.great_grandchild_2_1_4->create_baz(baz_2_1_4);

    rpc::shared_ptr<xxx::i_baz> baz_2_2_4;
    nodes.great_grandchild_2_2_4->create_baz(baz_2_2_4);

    // Cross-routing between all deepest nodes - maximum challenge for parent lookups
    rpc::shared_ptr<xxx::i_baz> output;
    
    ASSERT_EQ(nodes.great_grandchild_1_1_4->send_interface_back(baz_2_2_4, output), rpc::error::OK());
    ASSERT_EQ(baz_2_2_4, output);

    ASSERT_EQ(nodes.great_grandchild_1_2_4->send_interface_back(baz_2_1_4, output), rpc::error::OK());
    ASSERT_EQ(baz_2_1_4, output);

    ASSERT_EQ(nodes.great_grandchild_2_1_4->send_interface_back(baz_1_2_4, output), rpc::error::OK());
    ASSERT_EQ(baz_1_2_4, output);

    ASSERT_EQ(nodes.great_grandchild_2_2_4->send_interface_back(baz_1_1_4, output), rpc::error::OK());
    ASSERT_EQ(baz_1_1_4, output);
}

TYPED_TEST(remote_type_test, complex_topology_service_proxy_cache_bypass_trap_4)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Test 4: Service proxy cache bypass scenarios
    // Create routing patterns that might bypass established service proxy caches
    // Strategy: Interleave operations to confuse the caching mechanism

    auto nodes = build_complex_topology(*this);

    // Create objects at various levels
    rpc::shared_ptr<xxx::i_baz> baz_child3;
    nodes.child_3->create_baz(baz_child3);

    rpc::shared_ptr<xxx::i_baz> baz_gc_1_2;
    nodes.grandchild_1_2->create_baz(baz_gc_1_2);

    rpc::shared_ptr<xxx::i_baz> baz_gc_2_3;
    nodes.grandchild_2_3->create_baz(baz_gc_2_3);

    rpc::shared_ptr<xxx::i_baz> baz_ggc_1_1_2;
    nodes.great_grandchild_1_1_2->create_baz(baz_ggc_1_1_2);

    rpc::shared_ptr<xxx::i_baz> baz_ggc_2_2_3;
    nodes.great_grandchild_2_2_3->create_baz(baz_ggc_2_2_3);

    // Interleaved routing to stress proxy caching
    rpc::shared_ptr<xxx::i_baz> output;
    
    // Route from deep to shallow
    ASSERT_EQ(nodes.great_grandchild_1_1_2->send_interface_back(baz_child3, output), rpc::error::OK());
    ASSERT_EQ(baz_child3, output);

    // Route from shallow to deep  
    ASSERT_EQ(nodes.child_3->send_interface_back(baz_ggc_2_2_3, output), rpc::error::OK());
    ASSERT_EQ(baz_ggc_2_2_3, output);

    // Route cross-branch at intermediate levels
    ASSERT_EQ(nodes.grandchild_1_2->send_interface_back(baz_gc_2_3, output), rpc::error::OK());
    ASSERT_EQ(baz_gc_2_3, output);

    // Route from intermediate to deep cross-branch
    ASSERT_EQ(nodes.grandchild_2_3->send_interface_back(baz_ggc_1_1_2, output), rpc::error::OK());
    ASSERT_EQ(baz_ggc_1_1_2, output);

    // Final deep-to-intermediate cross routing
    ASSERT_EQ(nodes.great_grandchild_2_2_3->send_interface_back(baz_gc_1_2, output), rpc::error::OK());
    ASSERT_EQ(baz_gc_1_2, output);
}

TYPED_TEST(remote_type_test, complex_topology_multiple_convergence_points_trap_5)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Test 5: Multiple convergence points - stress test for channel collision
    // Create scenarios with multiple potential convergence points that could trigger line 792
    // Strategy: Route through multiple convergence points simultaneously

    auto nodes = build_complex_topology(*this);

    // Create objects at convergence points
    rpc::shared_ptr<xxx::i_baz> baz_convergence_child3;
    nodes.child_3->create_baz(baz_convergence_child3);

    rpc::shared_ptr<xxx::i_baz> baz_convergence_gc14; 
    nodes.grandchild_1_4->create_baz(baz_convergence_gc14);

    rpc::shared_ptr<xxx::i_baz> baz_convergence_gc24;
    nodes.grandchild_2_4->create_baz(baz_convergence_gc24);

    // Route through all convergence points - should stress channel management
    rpc::shared_ptr<xxx::i_baz> output;

    // Route from one convergence point to another via deep branches
    ASSERT_EQ(nodes.great_grandchild_1_1_1->send_interface_back(baz_convergence_gc24, output), rpc::error::OK());
    ASSERT_EQ(baz_convergence_gc24, output);

    ASSERT_EQ(nodes.great_grandchild_2_1_1->send_interface_back(baz_convergence_gc14, output), rpc::error::OK());
    ASSERT_EQ(baz_convergence_gc14, output);

    ASSERT_EQ(nodes.great_grandchild_1_2_1->send_interface_back(baz_convergence_child3, output), rpc::error::OK());
    ASSERT_EQ(baz_convergence_child3, output);

    ASSERT_EQ(nodes.great_grandchild_2_2_1->send_interface_back(baz_convergence_child3, output), rpc::error::OK());
    ASSERT_EQ(baz_convergence_child3, output);
}

TYPED_TEST(remote_type_test, check_interface_routing_with_out_params)
{
    auto& lib = this->get_lib();
    if (!lib.get_use_host_in_child())
        return;

    // Test the add_ref_options behavior with receive_interface (out-parameter)
    // This specifically tests the routing logic in prepare_out_param
    
    // Create multi-level hierarchy
    rpc::shared_ptr<yyy::i_example> level2;
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(level2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level3;
    ASSERT_EQ(level2->create_example_in_subordinate_zone(level3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    rpc::shared_ptr<yyy::i_example> level4;
    ASSERT_EQ(level3->create_example_in_subordinate_zone(level4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Create a parallel branch to test cross-routing
    rpc::shared_ptr<yyy::i_example> parallel_branch;
    ASSERT_EQ(level2->create_example_in_subordinate_zone(parallel_branch, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());

    // Test receive_interface calls that should exercise different add_ref_options paths
    rpc::shared_ptr<xxx::i_foo> received_interface;

    // This should test build_destination_route | build_caller_route scenario
    ASSERT_EQ(level4->receive_interface(received_interface), rpc::error::OK());
    ASSERT_NE(received_interface, nullptr);

    // Cross-zone receive that should test complex routing
    ASSERT_EQ(parallel_branch->receive_interface(received_interface), rpc::error::OK());
    ASSERT_NE(received_interface, nullptr);

    // Test give_interface with complex routing
    rpc::shared_ptr<xxx::i_baz> test_baz;
    level4->create_baz(test_baz);

    // This give_interface call should test in-parameter routing
    ASSERT_EQ(parallel_branch->give_interface(test_baz), rpc::error::OK());

    // Test from root level to deeply nested
    ASSERT_EQ(lib.get_example()->give_interface(test_baz), rpc::error::OK());
}


// template <class T>
// class type_test_with_host :
//     public type_test<T>
// {

// };

// typedef Types<
//     inproc_setup<true, false, false>,
//     inproc_setup<true, false, true>,
//     inproc_setup<true, true, false>,
//     inproc_setup<true, true, true>

// #ifdef BUILD_ENCLAVE
//     ,
//     enclave_setup<true, false, false>,
//     enclave_setup<true, false, true>,
//     enclave_setup<true, true, false>,
//     enclave_setup<true, true, true>
// #endif
//     > type_test_with_host_implementations;
// TYPED_TEST_SUITE(type_test_with_host, type_test_with_host_implementations);

// #ifdef BUILD_ENCLAVE
// TYPED_TEST(type_test_with_host, call_host_create_enclave_and_throw_away)
// {
//     bool run_standard_tests = false;
//     ASSERT_EQ(lib.get_example()->call_host_create_enclave_and_throw_away(run_standard_tests), rpc::error::OK());
// }

// TYPED_TEST(type_test_with_host, call_host_create_enclave)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);
// }
// #endif

// TYPED_TEST(type_test_with_host, look_up_app_and_return_with_nothing)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_look_up_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(target, nullptr);
// }

// TYPED_TEST(type_test_with_host, call_host_unload_app_not_there)
// {
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
// }

// #ifdef BUILD_ENCLAVE
// TYPED_TEST(type_test_with_host, call_host_look_up_app_unload_app)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
//     target = nullptr;
// }

// TYPED_TEST(type_test_with_host, call_host_look_up_app_not_return)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app_not_return("target", run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
//     target = nullptr;
// }

// TYPED_TEST(type_test_with_host, create_store_fetch_delete)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;
//     rpc::shared_ptr<yyy::i_example> target2;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app("target", target2, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
//     ASSERT_EQ(target, target2);
//     target = nullptr;
//     target2 = nullptr;
// }

// TYPED_TEST(type_test_with_host, create_store_not_return_delete)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app_not_return_and_delete("target", run_standard_tests),
//     rpc::error::OK()); target = nullptr;
// }

// TYPED_TEST(type_test_with_host, create_store_delete)
// {
//     bool run_standard_tests = false;
//     rpc::shared_ptr<yyy::i_example> target;
//     rpc::shared_ptr<yyy::i_example> target2;

//     ASSERT_EQ(lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
//     ASSERT_NE(target, nullptr);

//     ASSERT_EQ(lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
//     ASSERT_EQ(lib.get_example()->call_host_look_up_app_and_delete("target", target2, run_standard_tests),
//     rpc::error::OK()); ASSERT_EQ(target, target2); target = nullptr; target2 = nullptr;
// }
// #endif

TYPED_TEST(type_test_with_host, create_subordinate_zone)
{
    rpc::shared_ptr<yyy::i_example> target;
    ASSERT_EQ(this->get_lib().get_example()->create_example_in_subordinate_zone(
                  target, this->get_lib().get_local_host_ptr(), ++(*zone_gen)),
        rpc::error::OK());
}

// TYPED_TEST(type_test_with_host, create_subordinate_zone_and_set_in_host)
// {
//     ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone_and_set_in_host(++(*zone_gen), "foo",
//     lib.get_local_host_ptr()), rpc::error::OK()); rpc::shared_ptr<yyy::i_example> target;
//     lib.get_host()->look_up_app("foo", target); lib.get_host()->unload_app("foo"); target->set_host(nullptr);
// }

TYPED_TEST(remote_type_test, test_y_topology_and_return_new_prong_object)
{
    /*
     * Test for the Y-shaped topology bug described in service.cpp line 222-229:
     * 
     * This test creates the complete Y-topology scenario:
     * Zone 1 (root) → Zone 2 → Zone 3 (first prong, factory) → Zone 4 → Zone 5 (deep zone)
     *                         └─ Zone 6 (fork created by Zone 5) → Zone 7 (unknown to Zone 1)
     * 
     * Sequence:
     * - Zone 1 (root) creates Zone 2, which creates Zone 3 (first prong factory)
     * - Zone 3 creates Zone 4, which creates Zone 5 (deep zone, 5 zones deep from root)  
     * - Zone 5 AUTONOMOUSLY asks Zone 3 to create Zone 6 → Zone 7 (second prong)
     * - Zone 1 never knows about Zones 6 or 7 (critical for bug reproduction)
     * - Zone 5 gets an object from Zone 7 and passes it to Zone 1
     * - Zone 1 cannot set up service proxy chain to Zone 7 (bug condition)
     * - The fix: known_direction_zone parameter + reverse proxy creation
     */
    
    auto& lib = this->get_lib();
    
    // Create the first prong of the Y: Zone 1 → Zone 2 → Zone 3 (factory)
    rpc::shared_ptr<yyy::i_example> zone2;
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(zone2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone2, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone3_factory;
    ASSERT_EQ(zone2->create_example_in_subordinate_zone(zone3_factory, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone3_factory, nullptr);
    
    // Continue the first prong: Zone 3 → Zone 4 → Zone 5 (deep zone)
    rpc::shared_ptr<yyy::i_example> zone4;
    ASSERT_EQ(zone3_factory->create_example_in_subordinate_zone(zone4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone4, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone5_deep_service;
    ASSERT_EQ(zone4->create_example_in_subordinate_zone(zone5_deep_service, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone5_deep_service, nullptr);
    
    // THE CRITICAL TEST: Zone 5 autonomously creates the second prong through Zone 3 factory
    // Zone 1 (root) is NOT involved in this call, so it remains unaware of Zone 6 and Zone 7
    // This creates: Zone 3 → Zone 6 → Zone 7 (the fork that Zone 1 doesn't know about)
    std::vector<uint64_t> fork_zone_ids = {++(*zone_gen), ++(*zone_gen)}; // Zone 6, Zone 7
    rpc::shared_ptr<yyy::i_example> object_from_unknown_zone;
    ASSERT_EQ(zone5_deep_service->create_fork_and_return_object(zone3_factory, fork_zone_ids, object_from_unknown_zone), rpc::error::OK());
    ASSERT_NE(object_from_unknown_zone, nullptr);
}

TYPED_TEST(remote_type_test, test_y_topology_and_cache_and_retrieve_prong_object)
{
    /*
     * Test for cached object retrieval from autonomous zones that creates Y-topology routing problems.
     * Similar to previous test but Zone 5 caches the object from Zone 7, then host retrieves it.
     * 
     * Extended Deep Y-Topology Pattern:
     * Zone 1 (root) → Zone 2 → Zone 3 → Zone 4 → Zone 5 (deep factory)
     * Zone 5 autonomously creates Zone 6 → Zone 7 via Zone 3 (unknown to all ancestors)
     * 
     * This creates deeper routing isolation where:
     * - Zone 1 and Zone 2 have no knowledge of Zone 6 or Zone 7  
     * - Zone 5 gets Zone 3 to create Zones 6 and 7 autonomously
     * - Forces system to rely on known_direction_zone hint for routing
     * 
     * Bug Trigger Pattern:
     * 1. Zone 7 object gets created in autonomous fork
     * 2. Zone 5 caches Zone 7 object locally
     * 3. Host retrieves cached object, triggering cross-zone routing
     * 4. System must route to Zone 7 but Zone 1 has no direct path
     * 5. Without known_direction_zone fix: routing failure or infinite recursion
     */
    
    auto& lib = this->get_lib();
    
    // Create Deep Chain: Zone 1 → Zone 2 → Zone 3 → Zone 4 → Zone 5 (deep factory)
    rpc::shared_ptr<yyy::i_example> zone2;
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(zone2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone2, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone3;
    ASSERT_EQ(zone2->create_example_in_subordinate_zone(zone3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone3, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone4;
    ASSERT_EQ(zone3->create_example_in_subordinate_zone(zone4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone4, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone5_deep_factory;
    ASSERT_EQ(zone4->create_example_in_subordinate_zone(zone5_deep_factory, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone5_deep_factory, nullptr);
    
    // STEP 1: Zone 5 autonomously creates longer fork: Zone 5 → Zone 6 → Zone 7
    // This creates maximum isolation - no ancestor zones know about this branch
    std::vector<uint64_t> deep_autonomous_zones = {++(*zone_gen), ++(*zone_gen)}; // Zone 6, then Zone 7
    
    RPC_INFO("Zone 5 autonomously creating deep fork (Zone 6 → Zone 7) and caching object from Zone 7...");
    ASSERT_EQ(zone5_deep_factory->create_y_topology_fork(zone3, deep_autonomous_zones), rpc::error::OK());
    
    // STEP 2: Zone 5 retrieves Zone 7 object to begin the long journey back to Zone 1
    rpc::shared_ptr<yyy::i_example> zone7_object;
    
    RPC_INFO("Zone 5 retrieving Zone 7 object to pass up the chain...");
    ASSERT_EQ(zone5_deep_factory->retrieve_cached_autonomous_object(zone7_object), rpc::error::OK());
    ASSERT_NE(zone7_object, nullptr);
    
    RPC_INFO("Test completed - Zone 1 successfully worked with Zone 7 object from deep autonomous fork");
}

TYPED_TEST(remote_type_test, test_y_topology_and_set_host_with_prong_object)
{
    /*
     * CRITICAL TEST: Host registry with objects from autonomous zones - triggers stack overflow.
     * Similar to previous but Zone 5 caches the object from Zone 7, then in a separate call 
     * Zone 5 supplies it to Zone 1 via host registry.
     * 
     * Extended Deep Y-Topology Pattern:
     * Zone 1 (root) → Zone 2 → Zone 3 → Zone 4 → Zone 5 (deep factory)
     * Zone 5 autonomously creates Zone 6 → Zone 7 via Zone 3 (unknown to all ancestors)
     * 
     * This creates the most critical routing isolation scenario:
     * - Zone 1 and Zone 2 have no knowledge of Zone 6 or Zone 7  
     * - Zone 5 gets Zone 3 to create Zones 6 and 7 autonomously
     * - Zone 5 caches Zone 7 object locally
     * - In a separate call, Zone 5 sets the cached object in Zone 1's host registry
     * - Zone 1 later accesses object through host registry lookup
     * - Forces system to rely on known_direction_zone hint for routing
     * 
     * Bug Trigger Pattern - STACK OVERFLOW:
     * When known_direction_zone is disabled (set to 0), the service proxy has no idea 
     * where to find Zone 7 and goes into infinite recursive loop until stack overflow.
     * This is the most severe manifestation of the Y-topology routing bug.
     */
    
    auto& lib = this->get_lib();
    
    // Create Deep Chain: Zone 1 → Zone 2 → Zone 3 → Zone 4 → Zone 5 (deep factory)
    rpc::shared_ptr<yyy::i_example> zone2;
    ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(zone2, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone2, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone3;
    ASSERT_EQ(zone2->create_example_in_subordinate_zone(zone3, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone3, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone4;
    ASSERT_EQ(zone3->create_example_in_subordinate_zone(zone4, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone4, nullptr);
    
    rpc::shared_ptr<yyy::i_example> zone5_deep_factory;
    ASSERT_EQ(zone4->create_example_in_subordinate_zone(zone5_deep_factory, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
    ASSERT_NE(zone5_deep_factory, nullptr);
    
    // STEP 1: Zone 5 autonomously creates longer fork: Zone 5 → Zone 6 → Zone 7
    // This creates maximum isolation - no ancestor zones know about this branch
    std::vector<uint64_t> deep_autonomous_zones = {++(*zone_gen), ++(*zone_gen)}; // Zone 6, then Zone 7
    
    RPC_INFO("Zone 5 autonomously creating deep fork (Zone 6 → Zone 7) and caching object from Zone 7...");
    ASSERT_EQ(zone5_deep_factory->create_y_topology_fork(zone3, deep_autonomous_zones), rpc::error::OK());
    
    RPC_INFO("Zone 5 retrieving Zone 7 object to pass up the chain...");
    ASSERT_EQ(zone5_deep_factory->give_host_cached_object(), rpc::error::OK());
    
    rpc::shared_ptr<yyy::i_example> zone7_object;
    lib.get_local_host_ptr()->look_up_app("foo", zone7_object);
    ASSERT_NE(zone7_object, nullptr);
    
    // clean up the test or we will get non empty service errors
    lib.get_local_host_ptr()->set_app("foo", nullptr);
    
    RPC_INFO("Test completed - Zone 1 successfully worked with Zone 7 object from deep autonomous fork");
}

static_assert(rpc::id<std::string>::get(rpc::VERSION_2) == rpc::STD_STRING_ID);

// static_assert(rpc::id<xxx::test_template<std::string>>::get(rpc::VERSION_2) == 0xAFFFFFEB79FBFBFB);
// static_assert(rpc::id<xxx::test_template_without_params_in_id<std::string>>::get(rpc::VERSION_2) == 0x62C84BEB07545E2B);
// static_assert(rpc::id<xxx::test_template_use_legacy_empty_template_struct_id<std::string>>::get(rpc::VERSION_2) == 0x2E7E56276F6E36BE);
// static_assert(rpc::id<xxx::test_template_use_old<std::string>>::get(rpc::VERSION_2) == 0x66D71EBFF8C6FFA7);
//
