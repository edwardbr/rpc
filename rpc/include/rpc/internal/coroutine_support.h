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
#else
#define CORO_TASK(x) x
#define CO_RETURN return
#define CO_AWAIT
#define SYNC_WAIT(x) x
#endif
