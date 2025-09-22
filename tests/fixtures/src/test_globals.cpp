/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include "test_globals.h"
#include <string>

// Global variable definitions
std::weak_ptr<rpc::service> current_host_service;
std::atomic<uint64_t>* zone_gen = nullptr;

#ifdef _WIN32 // windows
std::string enclave_path = "./marshal_test_enclave.signed.dll";
#else // Linux
std::string enclave_path = "./libmarshal_test_enclave.signed.so";
#endif
