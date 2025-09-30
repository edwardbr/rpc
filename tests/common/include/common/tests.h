/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <example/example.h>
#include <rpc/rpc.h>

#include <common/foo_impl.h>

#ifdef BUILD_COROUTINE
#include <coro/coro.hpp>

#define CORO_ASSERT_EQ(x, y)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x != _coro_temp_y)                                                                              \
        {                                                                                                              \
            CO_RETURN false;                                                                                           \
        }                                                                                                              \
    } while (0)
#define CORO_ASSERT_NE(x, y)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_NE(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x == _coro_temp_y)                                                                              \
        {                                                                                                              \
            CO_RETURN false;                                                                                           \
        }                                                                                                              \
    } while (0)
#define CORO_VOID_ASSERT_EQ(x, y)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x != _coro_temp_y)                                                                              \
        {                                                                                                              \
            is_ready = true;                                                                                           \
            CO_RETURN;                                                                                                 \
        }                                                                                                              \
    } while (0)
#define CORO_VOID_ASSERT_NE(x, y)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_NE(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x == _coro_temp_y)                                                                              \
        {                                                                                                              \
            is_ready = true;                                                                                           \
            CO_RETURN;                                                                                                 \
        }                                                                                                              \
    } while (0)
#else
#define CORO_ASSERT_EQ(x, y)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x != _coro_temp_y)                                                                              \
        {                                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)
#define CORO_ASSERT_NE(x, y)                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_NE(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x == _coro_temp_y)                                                                              \
        {                                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)
#define CORO_VOID_ASSERT_EQ(x, y)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x != _coro_temp_y)                                                                              \
        {                                                                                                              \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)
#define CORO_VOID_ASSERT_NE(x, y)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        auto _coro_temp_x = (x);                                                                                       \
        auto _coro_temp_y = (y);                                                                                       \
        EXPECT_NE(_coro_temp_x, _coro_temp_y);                                                                         \
        if (_coro_temp_x == _coro_temp_y)                                                                              \
        {                                                                                                              \
            return;                                                                                                    \
        }                                                                                                              \
    } while (0)
#endif


namespace marshalled_tests
{
    CORO_TASK(bool) standard_tests(xxx::i_foo& foo, bool enclave);

    template<class T> inline CORO_TASK(bool) coro_standard_tests(T& lib)
    {
        auto root_service = lib.get_root_service();

        foo f;

        CO_AWAIT standard_tests(f, lib.get_has_enclave());
        CO_RETURN !lib.error_has_occured();
    }

    CORO_TASK(bool) remote_tests(bool use_host_in_child, rpc::shared_ptr<yyy::i_example> example_ptr);

    template<class T> inline CORO_TASK(bool) coro_remote_standard_tests(T& lib)
    {
        rpc::shared_ptr<xxx::i_foo> i_foo_ptr;
        auto ret = CO_AWAIT lib.get_example()->create_foo(i_foo_ptr);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed create_foo");
            CO_RETURN false;
        }
        if (!i_foo_ptr)
        {
            RPC_ERROR("create_foo returned OK but i_foo_ptr is null");
            CO_RETURN false;
        }
        CO_AWAIT standard_tests(*i_foo_ptr, lib.get_has_enclave());
        CO_RETURN true;
    }
}
