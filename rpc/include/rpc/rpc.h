/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

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

#include <rpc/proxy_base.h>
#include <rpc/object_proxy.h>
#include <rpc/service_proxy.h>
#include <rpc/proxy_impl.h>

#include <rpc/smart_pointers.h>

#include <rpc/service.h>
#include <rpc/stub.h>
#include <rpc/proxy.h>
#include <rpc/serialiser.h>