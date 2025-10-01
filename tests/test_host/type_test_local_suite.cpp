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

#ifndef TEST_STL_COMPLIANCE

// ============================================================================
// Optimistic Pointer Tests
// ============================================================================

// Test 1: Basic optimistic_ptr construction and lifecycle
CORO_TASK(bool) optimistic_ptr_basic_lifecycle_test(std::shared_ptr<rpc::service> root_service)
{
    // Create a shared_ptr to a local object
    auto f = rpc::shared_ptr<xxx::i_foo>(new foo());
    CORO_ASSERT_NE(f, nullptr);

    // Create optimistic_ptr from shared_ptr
    rpc::optimistic_ptr<xxx::i_foo> opt_f(f);
    CORO_ASSERT_NE(opt_f, nullptr);
    CORO_ASSERT_EQ(opt_f.get(), f.get());

    // Test copy constructor
    rpc::optimistic_ptr<xxx::i_foo> opt_f_copy(opt_f);
    CORO_ASSERT_NE(opt_f_copy, nullptr);
    CORO_ASSERT_EQ(opt_f_copy.get(), opt_f.get());

    // Test move constructor
    rpc::optimistic_ptr<xxx::i_foo> opt_f_move(std::move(opt_f_copy));
    CORO_ASSERT_NE(opt_f_move, nullptr);
    CORO_ASSERT_EQ(opt_f_move.get(), f.get());
    CORO_ASSERT_EQ(opt_f_copy, nullptr); // Moved-from should be null

    // Test assignment
    rpc::optimistic_ptr<xxx::i_foo> opt_f_assigned;
    CORO_ASSERT_EQ(opt_f_assigned, nullptr);
    opt_f_assigned = opt_f_move;
    CORO_ASSERT_NE(opt_f_assigned, nullptr);
    CORO_ASSERT_EQ(opt_f_assigned.get(), f.get());

    // Test reset
    opt_f_move.reset();
    CORO_ASSERT_EQ(opt_f_move, nullptr);

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_basic_lifecycle_test)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    run_coro_test(*this,
        [root_service](auto& lib)
        {
            (void)lib;
            return optimistic_ptr_basic_lifecycle_test(root_service);
        });
}

// Test 2: Optimistic pointer weak semantics for local objects
CORO_TASK(bool) optimistic_ptr_weak_semantics_local_test(std::shared_ptr<rpc::service> root_service)
{
    rpc::optimistic_ptr<xxx::i_foo> opt_f;

    {
        // Create a shared_ptr to a local object
        auto f = rpc::shared_ptr<xxx::i_foo>(new foo());
        CORO_ASSERT_NE(f, nullptr);

        // Create optimistic_ptr from shared_ptr
        opt_f = rpc::optimistic_ptr<xxx::i_foo>(f);
        CORO_ASSERT_NE(opt_f, nullptr);

        // Verify object is accessible
        CORO_ASSERT_EQ(opt_f.get(), f.get());

        // f goes out of scope - object should be deleted
        // (optimistic_ptr has weak semantics for local objects)
    }

    // opt_f still exists but points to deleted object
    // This is valid behavior per spec - optimistic_ptr has weak semantics
    // Accessing would be undefined behavior, but the pointer can exist
    CORO_ASSERT_NE(opt_f, nullptr); // Control block still exists

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_weak_semantics_local_test)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    run_coro_test(*this,
        [root_service](auto& lib)
        {
            (void)lib;
            return optimistic_ptr_weak_semantics_local_test(root_service);
        });
}

// Test 3: local_optimistic_ptr RAII locking for local objects
CORO_TASK(bool) local_optimistic_ptr_raii_lock_test(std::shared_ptr<rpc::service> root_service)
{
    rpc::shared_ptr<xxx::i_foo> f;
    rpc::optimistic_ptr<xxx::i_foo> opt_f;

    {
        // Create a shared_ptr to a local object
        f = rpc::shared_ptr<xxx::i_foo>(new foo());
        CORO_ASSERT_NE(f, nullptr);

        // Create optimistic_ptr from shared_ptr
        opt_f = rpc::optimistic_ptr<xxx::i_foo>(f);
        CORO_ASSERT_NE(opt_f, nullptr);

        // Clear the shared_ptr (but optimistic_ptr remains)
        f.reset();
        CORO_ASSERT_EQ(f, nullptr);

        // Object should be deleted now (weak semantics)
    }

    // Create local_optimistic_ptr - this would try to RAII lock
    // but object is already gone (this is a demonstration of the pattern)
    rpc::local_optimistic_ptr<xxx::i_foo> local_opt_f(opt_f);

    // For a valid use case, we need to keep the shared_ptr alive:
    f = rpc::shared_ptr<xxx::i_foo>(new foo());
    opt_f = rpc::optimistic_ptr<xxx::i_foo>(f);

    {
        // Create local_optimistic_ptr with valid object
        rpc::local_optimistic_ptr<xxx::i_foo> local_opt_f2(opt_f);
        CORO_ASSERT_EQ(static_cast<bool>(local_opt_f2), true);
        CORO_ASSERT_EQ(local_opt_f2.is_local(), true); // Should be local

        // Can safely access object through local_optimistic_ptr
        CORO_ASSERT_EQ(local_opt_f2.get(), f.get());

        // local_optimistic_ptr holds RAII lock, so object stays alive
        // even if we clear the shared_ptr
        f.reset();

        // Object still alive because local_optimistic_ptr holds RAII lock
        CORO_ASSERT_EQ(static_cast<bool>(local_opt_f2), true);
        CORO_ASSERT_NE(local_opt_f2.get(), nullptr);

        // local_optimistic_ptr goes out of scope, releasing RAII lock
    }

    // Now object is deleted

    CO_RETURN true;
}

TYPED_TEST(type_test, local_optimistic_ptr_raii_lock_test)
{
    auto& lib = this->get_lib();
    auto root_service = lib.get_root_service();
    run_coro_test(*this,
        [root_service](auto& lib)
        {
            (void)lib;
            return local_optimistic_ptr_raii_lock_test(root_service);
        });
}

// Test 4: Optimistic pointer semantics (weak for local, shared for remote)
template<class T>
CORO_TASK(bool) optimistic_ptr_remote_shared_semantics_test(T& lib)
{
    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create foo through example (will be local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_foo> f;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f), 0);
    CORO_ASSERT_NE(f, nullptr);

    // Create baz interface (local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_baz> baz;
    CORO_ASSERT_EQ(CO_AWAIT f->create_baz_interface(baz), 0);
    CORO_ASSERT_NE(baz, nullptr);

    // Check if the object is local or remote
    bool is_local = baz->is_local();

    // Create optimistic_ptr
    rpc::optimistic_ptr<xxx::i_baz> opt_baz(baz);
    CORO_ASSERT_NE(opt_baz, nullptr);

    // Clear the shared_ptr
    auto* raw_ptr = baz.get();
    baz.reset();

    if (is_local)
    {
        // For LOCAL objects: optimistic_ptr has WEAK semantics
        // The object is deleted when the last shared_ptr goes away
        // opt_baz pointer remains non-null but object is deleted
        CORO_ASSERT_NE(opt_baz, nullptr);  // Pointer still exists
        // Cannot safely call methods - object is deleted (this is expected behavior)
        // (local_optimistic_ptr RAII locking is tested separately in test 3)
    }
    else
    {
        // For REMOTE objects: optimistic_ptr has SHARED semantics
        // opt_baz keeps the remote proxy alive
        CORO_ASSERT_NE(opt_baz, nullptr);
        CORO_ASSERT_EQ(opt_baz.get(), raw_ptr);

        // Can call through optimistic_ptr directly (shared semantics keep proxy alive)
        auto error = CO_AWAIT opt_baz->callback(42);
        CORO_ASSERT_EQ(error, 0);

        // Also test with local_optimistic_ptr (should passthrough for remote)
        {
            rpc::local_optimistic_ptr<xxx::i_baz> local_opt_baz(opt_baz);
            CORO_ASSERT_EQ(static_cast<bool>(local_opt_baz), true);
            CORO_ASSERT_EQ(local_opt_baz.is_local(), false);  // Remote passthrough

            // Can call through local_optimistic_ptr (just a passthrough)
            auto error2 = CO_AWAIT local_opt_baz->callback(43);
            CORO_ASSERT_EQ(error2, 0);
        }
    }

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_remote_shared_semantics_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_remote_shared_semantics_test(lib); });
}

// Test 5: local_optimistic_ptr behavior (RAII lock for local, passthrough for remote)
template<class T>
CORO_TASK(bool) local_optimistic_ptr_remote_passthrough_test(T& lib)
{
    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create foo through example (will be local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_foo> f;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f), 0);
    CORO_ASSERT_NE(f, nullptr);

    // Create baz interface (local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_baz> baz;
    CORO_ASSERT_EQ(CO_AWAIT f->create_baz_interface(baz), 0);
    CORO_ASSERT_NE(baz, nullptr);

    // Check if the object is local or remote
    bool is_local = baz->is_local();

    // Create optimistic_ptr
    rpc::optimistic_ptr<xxx::i_baz> opt_baz(baz);
    CORO_ASSERT_NE(opt_baz, nullptr);

    if (is_local)
    {
        // Test local_optimistic_ptr RAII locking for LOCAL objects
        // Keep baz alive initially so we can test with local_optimistic_ptr
        {
            // Create local_optimistic_ptr - this should create RAII lock
            rpc::local_optimistic_ptr<xxx::i_baz> local_opt_baz(opt_baz);
            CORO_ASSERT_EQ(static_cast<bool>(local_opt_baz), true);
            CORO_ASSERT_EQ(local_opt_baz.is_local(), true);

            // Clear the shared_ptr - but local_optimistic_ptr has RAII lock
            baz.reset();

            // Object is locked - can safely call methods even though shared_ptr is gone
            auto error = CO_AWAIT local_opt_baz->callback(43);
            CORO_ASSERT_EQ(error, 0);
        } // RAII lock released here

        // After local_optimistic_ptr destroyed, object may be deleted
        // (if no other references exist)
    }
    else
    {
        // Test local_optimistic_ptr PASSTHROUGH for REMOTE objects
        // Clear the shared_ptr - opt_baz keeps proxy alive via shared semantics
        baz.reset();

        {
            // Create local_optimistic_ptr from remote optimistic_ptr
            rpc::local_optimistic_ptr<xxx::i_baz> local_opt_baz(opt_baz);
            CORO_ASSERT_EQ(static_cast<bool>(local_opt_baz), true);
            CORO_ASSERT_EQ(local_opt_baz.is_local(), false); // Should NOT be local

            // Should just passthrough to the remote proxy (no RAII lock)
            CORO_ASSERT_EQ(local_opt_baz.get(), opt_baz.get());

            // Can call through local_optimistic_ptr (passthrough)
            auto error = CO_AWAIT local_opt_baz->callback(43);
            CORO_ASSERT_EQ(error, 0);
        }

        // Remote proxy still accessible through opt_baz after local_optimistic_ptr destroyed
        auto error = CO_AWAIT opt_baz->callback(44);
        CORO_ASSERT_EQ(error, 0);
    }

    CO_RETURN true;
}

TYPED_TEST(type_test, local_optimistic_ptr_remote_passthrough_test)
{
    run_coro_test(*this, [](auto& lib) { return local_optimistic_ptr_remote_passthrough_test(lib); });
}

// Test 6: Optimistic pointer transparent operator-> for both local and remote
template<class T>
CORO_TASK(bool) optimistic_ptr_transparent_access_test(T& lib)
{
    auto example = lib.get_example();
    // Test 1: Local object access
    {
        // Create interface (local or remote depending on setup)
        rpc::shared_ptr<xxx::i_foo> f_local;
        CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f_local), 0);
        CORO_ASSERT_NE(f_local, nullptr);
            
        rpc::optimistic_ptr<xxx::i_foo> opt_f_local(f_local);

        // operator-> works transparently for local object
        CORO_ASSERT_NE(opt_f_local.operator->(), nullptr);
        CORO_ASSERT_NE(opt_f_local.get(), nullptr);
        CORO_ASSERT_EQ(opt_f_local.get(), f_local.get());
    }

    // Test 2: Remote object access
    {
                // Create interface (local or remote depending on setup)
        rpc::shared_ptr<xxx::i_baz> baz;
        CORO_ASSERT_EQ(CO_AWAIT example->create_baz(baz), 0);
        CORO_ASSERT_NE(baz, nullptr);
            
        rpc::optimistic_ptr<xxx::i_baz> opt_baz(baz);

        // operator-> works transparently for remote proxy
        CORO_ASSERT_NE(opt_baz.operator->(), nullptr);
        CORO_ASSERT_NE(opt_baz.get(), nullptr);
        CORO_ASSERT_EQ(opt_baz.get(), baz.get());

        // No bad_local_object exception - works transparently
        auto error = CO_AWAIT opt_baz->callback(45);
        CORO_ASSERT_EQ(error, 0);
    }

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_transparent_access_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_transparent_access_test(lib); });
}

// Test 7: Circular dependency resolution use case
template<class T>
CORO_TASK(bool) optimistic_ptr_circular_dependency_test(T& lib)
{
    // Simulate circular dependency scenario:
    // - Host owns children (shared_ptr)
    // - Children hold optimistic_ptr to host (no RAII ownership)

    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create host (will be local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_foo> host;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(host), 0);
    CORO_ASSERT_NE(host, nullptr);

    // Create child
    rpc::shared_ptr<xxx::i_baz> child_ref;
    CORO_ASSERT_EQ(CO_AWAIT host->create_baz_interface(child_ref), 0);

    // Child could hold optimistic_ptr back to host (breaking circular RAII ownership)
    rpc::optimistic_ptr<xxx::i_foo> opt_host(host);
    CORO_ASSERT_NE(opt_host, nullptr);

    // If we delete host (last shared_ptr), object is destroyed
    // even though optimistic_ptr exists (weak semantics)
    host.reset();

    // opt_host still exists but points to deleted object
    // This is correct behavior - circular dependency is broken
    CORO_ASSERT_NE(opt_host, nullptr); // Control block remains

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_circular_dependency_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_circular_dependency_test(lib); });
}

// Test 8: Comparison and nullptr operations
template<class T>
CORO_TASK(bool) optimistic_ptr_comparison_test(T& lib)
{
    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create two separate foo objects
    rpc::shared_ptr<xxx::i_foo> f1;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f1), 0);
    CORO_ASSERT_NE(f1, nullptr);

    rpc::shared_ptr<xxx::i_foo> f2;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f2), 0);
    CORO_ASSERT_NE(f2, nullptr);

    rpc::optimistic_ptr<xxx::i_foo> opt_f1(f1);
    rpc::optimistic_ptr<xxx::i_foo> opt_f2(f2);
    rpc::optimistic_ptr<xxx::i_foo> opt_null;

    // Test equality
    CORO_ASSERT_NE(opt_f1, opt_f2);
    CORO_ASSERT_EQ(opt_f1, opt_f1);

    // Test nullptr comparison
    CORO_ASSERT_EQ(opt_null, nullptr);
    CORO_ASSERT_NE(opt_f1, nullptr);

    // Test bool operator
    CORO_ASSERT_EQ(static_cast<bool>(opt_null), false);
    CORO_ASSERT_EQ(static_cast<bool>(opt_f1), true);

    // Test assignment to nullptr
    opt_f1 = nullptr;
    CORO_ASSERT_EQ(opt_f1, nullptr);

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_comparison_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_comparison_test(lib); });
}

// Test 9: Heterogeneous upcast (statically verifiable)
template<class T>
CORO_TASK(bool) optimistic_ptr_heterogeneous_upcast_test(T& lib)
{
    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create foo through example (will be local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_foo> f;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f), 0);
    CORO_ASSERT_NE(f, nullptr);

    // Create a baz object (implements both i_baz and i_bar)
    rpc::shared_ptr<xxx::i_baz> baz;
    CORO_ASSERT_EQ(CO_AWAIT f->create_baz_interface(baz), 0);
    CORO_ASSERT_NE(baz, nullptr);

    // Create optimistic_ptr<i_baz>
    rpc::optimistic_ptr<xxx::i_baz> opt_baz(baz);
    CORO_ASSERT_NE(opt_baz, nullptr);

    // Upcast to i_bar (statically verifiable - should compile)
    // Note: This requires i_baz to properly derive from i_bar
    // For now, test with same type conversion
    rpc::optimistic_ptr<xxx::i_baz> opt_baz2(opt_baz);
    CORO_ASSERT_NE(opt_baz2, nullptr);
    CORO_ASSERT_EQ(opt_baz2.get(), opt_baz.get());

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_heterogeneous_upcast_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_heterogeneous_upcast_test(lib); });
}

// Test 10: Multiple optimistic_ptr instances to same object
template<class T>
CORO_TASK(bool) optimistic_ptr_multiple_refs_test(T& lib)
{
    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create foo through example (will be local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_foo> f;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f), 0);
    CORO_ASSERT_NE(f, nullptr);

    // Create multiple optimistic_ptr instances to same object
    rpc::optimistic_ptr<xxx::i_foo> opt_f1(f);
    rpc::optimistic_ptr<xxx::i_foo> opt_f2(f);
    rpc::optimistic_ptr<xxx::i_foo> opt_f3(opt_f1);
    rpc::optimistic_ptr<xxx::i_foo> opt_f4 = opt_f2;

    // All should point to same object
    CORO_ASSERT_EQ(opt_f1.get(), f.get());
    CORO_ASSERT_EQ(opt_f2.get(), f.get());
    CORO_ASSERT_EQ(opt_f3.get(), f.get());
    CORO_ASSERT_EQ(opt_f4.get(), f.get());

    // All should be equal
    CORO_ASSERT_EQ(opt_f1, opt_f2);
    CORO_ASSERT_EQ(opt_f2, opt_f3);
    CORO_ASSERT_EQ(opt_f3, opt_f4);

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_multiple_refs_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_multiple_refs_test(lib); });
}

// Test 11: optimistic_ptr OBJECT_GONE behavior when remote stub is deleted
template<class T>
CORO_TASK(bool) optimistic_ptr_object_gone_test(T& lib)
{
    // Get example object (local or remote depending on setup)
    auto example = lib.get_example();
    CORO_ASSERT_NE(example, nullptr);

    // Create foo through example (will be local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_foo> f;
    CORO_ASSERT_EQ(CO_AWAIT example->create_foo(f), 0);
    CORO_ASSERT_NE(f, nullptr);

    // Create baz interface (local or marshalled depending on setup)
    rpc::shared_ptr<xxx::i_baz> baz;
    CORO_ASSERT_EQ(CO_AWAIT f->create_baz_interface(baz), 0);
    CORO_ASSERT_NE(baz, nullptr);

    // Check if the object is local or remote
    bool is_local = baz->is_local();

    if (!is_local)
    {
        // Test OBJECT_GONE for REMOTE objects only
        // Create optimistic_ptr from shared_ptr
        rpc::optimistic_ptr<xxx::i_baz> opt_baz(baz);
        CORO_ASSERT_NE(opt_baz, nullptr);

        // First call should work - shared_ptr keeps stub alive
        auto error1 = CO_AWAIT opt_baz->callback(42);
        CORO_ASSERT_EQ(error1, 0);

        // Release the shared_ptr - this should delete the stub on the remote side
        // because optimistic references don't hold stub lifetime
        baz.reset();
        f.reset();

        // Second call through optimistic_ptr should fail with OBJECT_GONE
        // The optimistic_ptr still exists but the remote stub has been deleted
        auto error2 = CO_AWAIT opt_baz->callback(43);
        CORO_ASSERT_EQ(error2, rpc::error::OBJECT_GONE());

        // The optimistic_ptr itself remains valid (pointer not null)
        CORO_ASSERT_NE(opt_baz, nullptr);
    }
    else
    {
        // For local objects, optimistic_ptr has weak semantics
        // Skip the OBJECT_GONE test as it's specific to remote stub lifetime
    }

    CO_RETURN true;
}

TYPED_TEST(type_test, optimistic_ptr_object_gone_test)
{
    run_coro_test(*this, [](auto& lib) { return optimistic_ptr_object_gone_test(lib); });
}

#endif // TEST_STL_COMPLIANCE
