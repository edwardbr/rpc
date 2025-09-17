/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <unordered_map>

#include <rpc/service_proxies/basic_service_proxies.h>

#ifdef _IN_ENCLAVE
#include <rpc/telemetry/enclave_telemetry_service.h>
#else
#include <rpc/telemetry/telemetry_handler.h>
#include <rpc/telemetry/sequence_diagram_telemetry_service.h>
#include <rpc/telemetry/console_telemetry_service.h>
#endif

namespace rpc
{
#ifdef _IN_ENCLAVE
    bool telemetry_service_manager::create()
    {
        if (telemetry_service_)
            return false;
        return rpc::enclave_telemetry_service::create(telemetry_service_);
    }
#else

    bool telemetry_service_manager::create(
        const std::string& test_suite_name, const std::string& name, const std::filesystem::path& directory)
    {
        if (telemetry_service_)
            return false;
#ifdef USE_CONSOLE_TELEMETRY
        return rpc::console_telemetry_service::create(telemetry_service_, test_suite_name, name, directory);
#else
        return rpc::sequence_diagram_telemetry_service::create(telemetry_service_, test_suite_name, name, directory);
#endif
    }
#endif
}
