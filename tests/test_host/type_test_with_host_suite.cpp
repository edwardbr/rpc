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
#include "common/tcp/service_proxy.h"
#include "common/tcp/listener.h"
#include "common/spsc/service_proxy.h"
#include "common/spsc/channel_manager.h"
#endif
#include "rpc_global_logger.h"
#include "test_host.h"
#include "in_memory_setup.h"
#include "inproc_setup.h"
#include "enclave_setup.h"
#ifdef BUILD_COROUTINE
#include "tcp_setup.h"
#include "spsc_setup.h"
#endif
#include "crash_handler.h"
#include "type_test_fixture.h"

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

#include "test_globals.h"

extern bool enable_multithreaded_tests;

// Fixture and type list for host-aware tests.
template<class T>
class type_test_with_host : public type_test<T>
{
};

using type_test_with_host_implementations = ::testing::Types<
    inproc_setup<true, false, false>,
    inproc_setup<true, false, true>,
    inproc_setup<true, true, false>,
    inproc_setup<true, true, true>

#ifdef BUILD_ENCLAVE
    ,
    enclave_setup<true, false, false>,
    enclave_setup<true, false, true>,
    enclave_setup<true, true, false>,
    enclave_setup<true, true, true>
#endif
    >;

TYPED_TEST_SUITE(type_test_with_host, type_test_with_host_implementations);

#ifdef BUILD_ENCLAVE
template<class T> CORO_TASK(bool) coro_call_host_create_enclave_and_throw_away(T& lib)
{
    bool run_standard_tests = false;
    CORO_ASSERT_EQ(
        CO_AWAIT lib.get_example()->call_host_create_enclave_and_throw_away(run_standard_tests), rpc::error::OK());
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, call_host_create_enclave_and_throw_away)
{
    run_coro_test(*this, [](auto& lib) { return coro_call_host_create_enclave_and_throw_away<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_call_host_create_enclave(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_NE(target, nullptr);
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, call_host_create_enclave)
{
    run_coro_test(*this, [](auto& lib) { return coro_call_host_create_enclave<TypeParam>(lib); });
}
#endif

template<class T> CORO_TASK(bool) coro_look_up_app_and_return_with_nothing(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    CORO_ASSERT_EQ(
        CO_AWAIT lib.get_example()->call_host_look_up_app("target", target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(target, nullptr);
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, look_up_app_and_return_with_nothing)
{
    run_coro_test(*this, [](auto& lib) { return coro_look_up_app_and_return_with_nothing<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_call_host_unload_app_not_there(T& lib)
{
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, call_host_unload_app_not_there)
{
    run_coro_test(*this, [](auto& lib) { return coro_call_host_unload_app_not_there<TypeParam>(lib); });
}

#ifdef BUILD_ENCLAVE
template<class T> CORO_TASK(bool) coro_call_host_look_up_app_unload_app(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_NE(target, nullptr);

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
    target = nullptr;
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, call_host_look_up_app_unload_app)
{
    run_coro_test(*this, [](auto& lib) { return coro_call_host_look_up_app_unload_app<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_call_host_look_up_app_not_return(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_NE(target, nullptr);

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(
        CO_AWAIT lib.get_example()->call_host_look_up_app_not_return("target", run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
    target = nullptr;
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, call_host_look_up_app_not_return)
{
    run_coro_test(*this, [](auto& lib) { return coro_call_host_look_up_app_not_return<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_create_store_fetch_delete(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;
    rpc::shared_ptr<yyy::i_example> target2;

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_NE(target, nullptr);

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(
        CO_AWAIT lib.get_example()->call_host_look_up_app("target", target2, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_unload_app("target"), rpc::error::OK());
    CORO_ASSERT_EQ(target, target2);
    target = nullptr;
    target2 = nullptr;
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, create_store_fetch_delete)
{
    run_coro_test(*this, [](auto& lib) { return coro_create_store_fetch_delete<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_create_store_not_return_delete(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_NE(target, nullptr);

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_look_up_app_not_return_and_delete("target", run_standard_tests),
        rpc::error::OK());
    target = nullptr;
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, create_store_not_return_delete)
{
    run_coro_test(*this, [](auto& lib) { return coro_create_store_not_return_delete<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_create_store_delete(T& lib)
{
    bool run_standard_tests = false;
    rpc::shared_ptr<yyy::i_example> target;
    rpc::shared_ptr<yyy::i_example> target2;

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_create_enclave(target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_NE(target, nullptr);

    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_set_app("target", target, run_standard_tests), rpc::error::OK());
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->call_host_look_up_app_and_delete("target", target2, run_standard_tests),
        rpc::error::OK());
    CORO_ASSERT_EQ(target, target2);
    target = nullptr;
    target2 = nullptr;
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, create_store_delete)
{
    run_coro_test(*this, [](auto& lib) { return coro_create_store_delete<TypeParam>(lib); });
}
#endif

template<class T> CORO_TASK(bool) coro_create_subordinate_zone(T& lib)
{
    rpc::shared_ptr<yyy::i_example> target;
    CORO_ASSERT_EQ(
        CO_AWAIT lib.get_example()->create_example_in_subordinate_zone(target, lib.get_local_host_ptr(), ++(*zone_gen)),
        rpc::error::OK());
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, create_subordinate_zone) // TODO: Missing test suite definition
{
    run_coro_test(*this, [](auto& lib) { return coro_create_subordinate_zone<TypeParam>(lib); });
}

template<class T> CORO_TASK(bool) coro_create_subordinate_zone_and_set_in_host(T& lib)
{
    CORO_ASSERT_EQ(CO_AWAIT lib.get_example()->create_example_in_subordinate_zone_and_set_in_host(
                       ++(*zone_gen), "foo", lib.get_local_host_ptr()),
        rpc::error::OK());
    rpc::shared_ptr<yyy::i_example> target;
    CO_AWAIT lib.get_host()->look_up_app("foo", target);
    CO_AWAIT lib.get_host()->unload_app("foo");
    CO_AWAIT target->set_host(nullptr);
    CO_RETURN true;
}

TYPED_TEST(type_test_with_host, create_subordinate_zone_and_set_in_host)
{
    run_coro_test(*this, [](auto& lib) { return coro_create_subordinate_zone_and_set_in_host<TypeParam>(lib); });
}
