/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <iostream>
#include <unordered_map>
#include <string_view>
#include <thread>

#include <common/foo_impl.h>
#include <common/tests.h>

#include "common/tcp/service_proxy.h"
#include "common/tcp/listener.h"

#include "common/spsc/service_proxy.h"
#include "common/spsc/channel_manager.h"

#include <rpc/coroutine_support.h>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#ifdef BUILD_COROUTINE
#include <coro/io_scheduler.hpp>
#endif

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <rpc/error_codes.h>
#include <rpc/casting_interface.h>

// Include extracted setup classes
#include "test_host.h"
#include "in_memory_setup.h"
#include "inproc_setup.h"
#include "enclave_setup.h"
#include "tcp_setup.h"
#include "spsc_setup.h"

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
bool enable_telemetry_server = true;
bool enable_multithreaded_tests = false;

rpc::weak_ptr<rpc::service> current_host_service;
std::atomic<uint64_t>* zone_gen = nullptr;

// This line tests that we can define tests in an unnamed namespace.
namespace
{

    extern "C" int main(int argc, char* argv[])
    {
        std::string disable_telemetry_server;
        std::string enable_multithreaded_tests_flag;

        for (size_t i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-t" || arg == "--disable_telemetry_server")
                enable_telemetry_server = false;
            if (arg == "-m" || arg == "--enable_multithreaded_tests")
                enable_multithreaded_tests = true;
        }

        auto logger = spdlog::stdout_color_mt("console");
        logger->set_pattern("[%^%l%$] %v");
        spdlog::set_default_logger(logger);
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
}

template<class T> class type_test : public testing::Test
{
    T lib_;

public:
    T& get_lib() { return lib_; }

    void SetUp() override { this->lib_.SetUp(); }
    void TearDown() override { this->lib_.TearDown(); }
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
    inproc_setup<true, true, true>,
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

    co_await lib.check_for_error(standard_tests(f, lib.get_has_enclave()));
    is_ready = true;
    CO_RETURN !lib.error_has_occured();
}

TYPED_TEST(type_test, standard_tests)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    root_service->get_scheduler()->schedule(lib.check_for_error(coro_standard_tests(is_ready, lib)));
    while (!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
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

    lib.get_scheduler()->schedule(lib.check_for_error(dyanmic_cast_tests(is_ready, root_service)));
    while (!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    };

    ASSERT_EQ(lib.error_has_occured(), false);
}

template<class T> using remote_type_test = type_test<T>;

typedef Types<
    // inproc_setup<false, false, false>,

    inproc_setup<true, false, false>,
    inproc_setup<true, false, true>,
    inproc_setup<true, true, false>,
    inproc_setup<true, true, true>,
    tcp_setup<true, false, false>,
    tcp_setup<true, false, true>,
    tcp_setup<true, true, false>,
    tcp_setup<true, true, true>,
    spsc_setup<true, false, false>,
    spsc_setup<true, false, true>,
    spsc_setup<true, true, false>,
    spsc_setup<true, true, true>

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
    auto ret = co_await lib.get_example()->create_foo(i_foo_ptr);
    if (ret != rpc::error::OK())
    {
        LOG_CSTR("failed create_foo");
        CO_RETURN;
    }
    co_await lib.check_for_error(standard_tests(*i_foo_ptr, lib.get_has_enclave()));
    is_ready = true;
}

TYPED_TEST(remote_type_test, remote_standard_tests)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    root_service->get_scheduler()->schedule(coro_remote_standard_tests(is_ready, lib));
    while (!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
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
    co_await remote_tests(lib.get_use_host_in_child(), lib.get_example(), zone_id);
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
    root_service->get_scheduler()->schedule(coro_remote_tests(is_ready, lib, zone_id));
    while (!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
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
    auto example_relay_ptr = co_await lib.create_new_zone();
    co_await example_relay_ptr->set_host(host);
    is_ready = true;
}

TYPED_TEST(remote_type_test, create_new_zone)
{
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    rpc::shared_ptr<yyy::i_host> host;
    root_service->get_scheduler()->schedule(coro_create_new_zone(is_ready, lib, host));
    while (!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
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
    auto example_relay_ptr = co_await lib.create_new_zone();
    rpc::shared_ptr<xxx::i_foo> i_foo_relay_ptr;
    co_await example_relay_ptr->create_foo(i_foo_relay_ptr);
    co_await standard_tests(*i_foo_relay_ptr, true);

    {
        auto example_o = rpc::casting_interface::get_object_id(*example_relay_ptr);
        auto foo_o = rpc::casting_interface::get_object_id(*i_foo_relay_ptr);
        foo_o = foo_o;
        example_o = example_o;
    }

    // rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
    // ASSERT_EQ(lib.get_example()->create_foo(i_foo_ptr), 0);
    example_relay_ptr = nullptr;
    i_foo_relay_ptr = nullptr;

    // standard_tests(*i_foo_ptr, true);

    auto success = lib.get_root_service()->schedule(
        [&is_ready]() -> CORO_TASK(void)
        {
            is_ready = true;
            CO_RETURN;
        }());
}

TYPED_TEST(remote_type_test, create_new_zone_releasing_host_then_running_on_other_enclave)
{
    GTEST_SKIP() << "for later";
    bool is_ready = false;
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    rpc::shared_ptr<yyy::i_host> host;
    root_service->get_scheduler()->schedule(
        coro_create_new_zone_releasing_host_then_running_on_other_enclave(is_ready, lib, host));
    while (!is_ready)
    {
        root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    root_service->get_scheduler()->process_events(std::chrono::milliseconds(1));
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

//     ASSERT_EQ(new_new_zone->send_interface_back(input, output), rpc::error::OK());
//     ASSERT_EQ(input, output);

// }

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

// TYPED_TEST(type_test_with_host, create_subordinate_zone)
// {
//     rpc::shared_ptr<yyy::i_example> target;
//     ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone(target, lib.get_local_host_ptr(), ++(*zone_gen)), rpc::error::OK());
// }

// TYPED_TEST(type_test_with_host, create_subordinate_zone_and_set_in_host)
// {
//     ASSERT_EQ(lib.get_example()->create_example_in_subordinate_zone_and_set_in_host(++(*zone_gen), "foo",
//     lib.get_local_host_ptr()), rpc::error::OK()); rpc::shared_ptr<yyy::i_example> target;
//     lib.get_host()->look_up_app("foo", target); lib.get_host()->unload_app("foo"); target->set_host(nullptr);
// }

// static_assert(rpc::id<std::string>::get(rpc::VERSION_2) == rpc::STD_STRING_ID);

// static_assert(rpc::id<xxx::test_template<std::string>>::get(rpc::VERSION_2) == 0xAFFFFFEB79FBFBFB);
// static_assert(rpc::id<xxx::test_template_without_params_in_id<std::string>>::get(rpc::VERSION_2) == 0x62C84BEB07545E2B);
// static_assert(rpc::id<xxx::test_template_use_legacy_empty_template_struct_id<std::string>>::get(rpc::VERSION_2) == 0x2E7E56276F6E36BE);
// static_assert(rpc::id<xxx::test_template_use_old<std::string>>::get(rpc::VERSION_2) == 0x66D71EBFF8C6FFA7);
//
