/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

// 1. Fundamental types and forward declarations
#include <rpc/version.h>
#include <rpc/assert.h>
#include <rpc/error_codes.h>
#include <rpc/logger.h>
#include <rpc/types.h>
#include <rpc/marshaller.h>
#include <rpc/casting_interface.h>
#include <rpc/rpc_fwd.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

#include <rpc/smart_pointers.h> // This is the file we've been building (rpc_smart_pointers_v4)
#include <rpc/proxy.h>          // User's existing proxy.h (defines object_proxy, proxy_base, proxy_impl)

#include <rpc/service.h>
#include <rpc/stub.h>
#include <rpc/serialiser.h>
#include <rpc/basic_service_proxies.h>