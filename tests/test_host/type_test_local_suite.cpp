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

// Type list for local type_test instantiations.
// Requires the various setup helpers to be included before this header.

using local_implementations = ::testing::Types<
    in_memory_setup<false>,
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

TYPED_TEST(type_test, standard_tests)
{
    run_coro_test(*this, [](auto& lib) { return coro_standard_tests<TypeParam>(lib); });
}

CORO_TASK(bool) dyanmic_cast_tests(std::shared_ptr<rpc::service> root_service)
{
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo());

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
    CO_RETURN true;
}

TYPED_TEST(type_test, dyanmic_cast_tests)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    run_coro_test(*this,
        [root_service](auto& lib)
        {
            (void)lib; // lib not used in dyanmic_cast_tests
            return dyanmic_cast_tests(root_service);
        });
}
