#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>

#include <rpc/basic_service_proxies.h>

#include <rpc/telemetry/telemetry_handler.h>
#include <rpc/telemetry/host_telemetry_service.h>
#include <rpc/telemetry/console_telemetry_service.h>

using namespace std::chrono_literals;
namespace rpc
{
    bool telemetry_service_manager::create(
        const std::string& test_suite_name, const std::string& name, const std::filesystem::path& directory)
    {
        if (telemetry_service_)
            return false;
#ifdef USE_CONSOLE_TELEMETRY
        return rpc::console_telemetry_service::create(telemetry_service_, test_suite_name, name, directory);
#else
        return rpc::host_telemetry_service::create(telemetry_service_, test_suite_name, name, directory);
#endif
    }
}
