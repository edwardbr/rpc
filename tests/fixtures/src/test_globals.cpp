/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_globals.h"
#include <string>

// Global variable definitions
std::weak_ptr<rpc::service> current_host_service;
std::atomic<uint64_t>* zone_gen = nullptr;
bool enable_telemetry_server = true;

#ifdef _WIN32 // windows
std::string enclave_path = "./marshal_test_enclave.signed.dll";
#else // Linux
std::string enclave_path = "./libmarshal_test_enclave.signed.so";
#endif

#ifdef USE_RPC_TELEMETRY
rpc::telemetry_service_manager telemetry_service_manager_;
#endif