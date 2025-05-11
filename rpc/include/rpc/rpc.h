/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

// for future coroutine deliniation
#define NAMESPACE_INLINE_BEGIN inline namespace synchronous {
#define NAMESPACE_INLINE_END }

#include <rpc/internal/version.h>
#include <rpc/internal/assert.h>
#include <rpc/internal/error_codes.h>
#include <rpc/internal/types.h>
#include <rpc/internal/logger.h>
#include <rpc/internal/type_id.h>

// synchronous/coroutine sensitive headers

// the key interzone communication definiton that all services and service_proxies need to implement
#include <rpc/internal/marshaller.h>

#include <rpc/internal/forward_declarations.h>

#include <rpc/internal/casting_interface.h>
#include <rpc/internal/proxy_base.h>
#include <rpc/internal/object_proxy.h>
#include <rpc/internal/service_proxy.h>
#include <rpc/internal/proxy_impl.h>

#include <rpc/internal/smart_pointers.h>

#include <rpc/internal/service.h>
#include <rpc/internal/stub.h>
#include <rpc/internal/proxy.h>
#include <rpc/internal/serialiser.h>