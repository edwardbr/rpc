#include <memory>
#include <rpc/telemetry/console_telemetry_service.h>

int main()
{
    std::shared_ptr<rpc::i_telemetry_service> service;
    rpc::console_telemetry_service::create(service, "", "", "");
    
    // Test all log levels
    service->message(rpc::i_telemetry_service::debug, "This is a debug message");
    service->message(rpc::i_telemetry_service::trace, "This is a trace message");
    service->message(rpc::i_telemetry_service::info, "This is an info message");
    service->message(rpc::i_telemetry_service::warn, "This is a warning message");
    service->message(rpc::i_telemetry_service::err, "This is an error message");
    service->message(rpc::i_telemetry_service::critical, "This is a critical message");
    
    return 0;
}