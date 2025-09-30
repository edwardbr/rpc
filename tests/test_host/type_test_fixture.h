#pragma once

#include "gtest/gtest.h"

// Universal template-based coroutine test dispatcher
// All coroutines now return CORO_TASK(bool) and use check_for_error
// The lambda wrapper sets is_ready = true when the coroutine completes
template<typename TestFixture, typename CoroFunc, typename... Args>
void run_coro_test(TestFixture& test_fixture, CoroFunc&& coro_function, Args&&... args)
{
    auto& lib = test_fixture.get_lib();
#ifdef BUILD_COROUTINE
    bool is_ready = false;
    // Create a lambda that calls the coroutine function and sets is_ready when done
    auto wrapper_function = [&]() -> CORO_TASK(bool)
    {
        auto result = CO_AWAIT coro_function(lib, std::forward<Args>(args)...);
        is_ready = true; // Set is_ready when coroutine completes
        CO_RETURN result;
    };

    lib.get_scheduler()->schedule(lib.check_for_error(wrapper_function()));
    while (!is_ready)
    {
        lib.get_scheduler()->process_events(std::chrono::milliseconds(1));
    }
#else
    coro_function(lib, std::forward<Args>(args)...);
#endif
    ASSERT_EQ(lib.error_has_occured(), false);
}

// Generic fixture used to instantiate RPC host tests for multiple setups.
template<class T> class type_test : public testing::Test
{
    T lib_;

public:
    T& get_lib() { return lib_; }
    const T& get_lib() const { return lib_; }

    void SetUp() override { this->lib_.set_up(); }
    void TearDown() override { this->lib_.tear_down(); }
};
