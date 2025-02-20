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
    #define CORO_ASSERT_EQ(x, y) if(x != y) CO_RETURN false
    #define CORO_ASSERT_NE(x, y) if(x == y) CO_RETURN false
#else
    #define CORO_TASK(x) x
    #define CO_RETURN return
    #define CO_AWAIT
    #define SYNC_WAIT(x) x
    #define CORO_ASSERT_EQ(x, y) ASSERT_EQ(x, y)
    #define CORO_ASSERT_NE(x, y) ASSERT_NE(x, y)
#endif