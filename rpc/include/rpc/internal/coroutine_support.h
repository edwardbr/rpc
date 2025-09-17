/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once


#ifdef BUILD_COROUTINE
    #include <coro/coro.hpp>

    #define CORO_TASK(x) coro::task<x>
    #define CO_RETURN co_return
    #define CO_AWAIT co_await
    #define SYNC_WAIT(x) coro::sync_wait(x)
    #define CORO_ASSERT_EQ(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x != _coro_temp_y) { \
            CO_RETURN false; \
        } \
    } while(0)
    #define CORO_ASSERT_NE(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_NE(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x == _coro_temp_y) { \
            CO_RETURN false; \
        } \
    } while(0)
    #define CORO_VOID_ASSERT_EQ(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x != _coro_temp_y) { \
            is_ready = true; \
            CO_RETURN; \
        } \
    } while(0)
    #define CORO_VOID_ASSERT_NE(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_NE(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x == _coro_temp_y) { \
            is_ready = true; \
            CO_RETURN; \
        } \
    } while(0)
#else
    #define CORO_TASK(x) x
    #define CO_RETURN return
    #define CO_AWAIT
    #define SYNC_WAIT(x) x
    #define CORO_ASSERT_EQ(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x != _coro_temp_y) { \
            return false; \
        } \
    } while(0)
    #define CORO_ASSERT_NE(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_NE(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x == _coro_temp_y) { \
            return false; \
        } \
    } while(0)
    #define CORO_VOID_ASSERT_EQ(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_EQ(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x != _coro_temp_y) { \
            return; \
        } \
    } while(0)
    #define CORO_VOID_ASSERT_NE(x, y) do { \
        auto _coro_temp_x = (x); \
        auto _coro_temp_y = (y); \
        EXPECT_NE(_coro_temp_x, _coro_temp_y); \
        if(_coro_temp_x == _coro_temp_y) { \
            return; \
        } \
    } while(0)
#endif