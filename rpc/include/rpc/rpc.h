/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

// for future coroutine deliniation
#define NAMESPACE_INLINE_BEGIN                                                                                         \
    inline namespace synchronous                                                                                       \
    {
#define NAMESPACE_INLINE_END }

#include <rpc/internal/version.h>
#include <rpc/internal/assert.h>
#include <rpc/internal/error_codes.h>

#include <rpc/internal/types.h>
#include <rpc/internal/logger.h>
#include <rpc/internal/member_ptr.h>
#include <rpc/internal/remote_pointer.h>
#include <rpc/internal/coroutine_support.h>

// synchronous/coroutine sensitive headers

// the key interzone communication definiton that all services and service_proxies need to implement
#include <rpc/internal/marshaller.h>

// all remoteable objects need to implement this interface
#include <rpc/internal/casting_interface.h>

//remote proxy of an object
#include <rpc/internal/object_proxy.h>

//some helpre forward declarations
#include <rpc/internal/bindings_fwd.h>

// services manage the logical zones between which data is marshalled
#include <rpc/internal/service.h>

// the remote proxy to another zone
#include <rpc/internal/service_proxy.h>

//the deserialisation logic to an object
#include <rpc/internal/stub.h>

//the serialisation declarations
#include <rpc/internal/serialiser.h>

// internal plumbing
#include <rpc/internal/bindings.h>
