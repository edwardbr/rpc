/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#pragma once

#include <atomic>
#include <string>
#include <rpc/rpc.h>

#ifdef USE_RPC_TELEMETRY
#include <rpc/telemetry/i_telemetry_service.h>
#endif

// Other global variables used by the test setup classes
extern std::weak_ptr<rpc::service> current_host_service;
extern std::atomic<uint64_t>* zone_gen;
extern std::string telemetry_config;

#ifdef _WIN32 // windows
extern std::string enclave_path;
#else // Linux
extern std::string enclave_path;
#endif