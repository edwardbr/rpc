#pragma once
#include <string>

//copied from spdlog
#define I_TELEMETRY_LEVEL_TRACE 0
#define I_TELEMETRY_LEVEL_DEBUG 1
#define I_TELEMETRY_LEVEL_INFO 2
#define I_TELEMETRY_LEVEL_WARN 3
#define I_TELEMETRY_LEVEL_ERROR 4
#define I_TELEMETRY_LEVEL_CRITICAL 5
#define I_TELEMETRY_LEVEL_OFF 6

class i_telemetry_service
{
public:
    enum level_enum
    {
        trace = I_TELEMETRY_LEVEL_TRACE,
        debug = I_TELEMETRY_LEVEL_DEBUG,
        info = I_TELEMETRY_LEVEL_INFO,
        warn = I_TELEMETRY_LEVEL_WARN,
        err = I_TELEMETRY_LEVEL_ERROR,
        critical = I_TELEMETRY_LEVEL_CRITICAL,
        off = I_TELEMETRY_LEVEL_OFF,
        n_levels
    };
    virtual ~i_telemetry_service() = default;

    virtual void on_service_creation(const char* name, uint64_t zone_id) const = 0;
    virtual void on_service_deletion(const char* name, uint64_t zone_id) const = 0;
    virtual void on_service_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const = 0;
    virtual void on_service_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id) const = 0;
    virtual void on_service_proxy_try_cast(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const = 0;
    virtual void on_service_proxy_add_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const = 0;
    virtual void on_service_proxy_release(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const = 0;  

    virtual void on_impl_creation(const char* name, uint64_t object_id, uint64_t interface_id) const = 0;
    virtual void on_impl_deletion(const char* name, uint64_t object_id, uint64_t interface_id) const = 0;

    virtual void on_stub_creation(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const = 0;    
    virtual void on_stub_deletion(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const = 0;
    virtual void on_stub_send(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const = 0;
    virtual void on_stub_add_ref(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const = 0;
    virtual void on_stub_release(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count) const = 0;

    virtual void on_object_proxy_creation(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const = 0;
    virtual void on_object_proxy_deletion(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id) const = 0;

    virtual void on_interface_proxy_creation(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const = 0;
    virtual void on_interface_proxy_deletion(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id) const = 0;
    virtual void on_interface_proxy_send(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id) const = 0;

    virtual void message(level_enum level, const char* message) const = 0;  
};