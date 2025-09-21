/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <rpc/internal/types.h>
#include <rpc/internal/marshaller.h>

#ifndef _IN_ENCLAVE
#include <filesystem>
#endif

#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
#include <rpc/internal/thread_local_logger.h>
#endif

// copied from spdlog
#define I_TELEMETRY_LEVEL_DEBUG 0
#define I_TELEMETRY_LEVEL_TRACE 1
#define I_TELEMETRY_LEVEL_INFO 2
#define I_TELEMETRY_LEVEL_WARN 3
#define I_TELEMETRY_LEVEL_ERROR 4
#define I_TELEMETRY_LEVEL_CRITICAL 5
#define I_TELEMETRY_LEVEL_OFF 6

namespace rpc
{
    class i_telemetry_service
    {
    public:
        enum level_enum
        {
            debug = I_TELEMETRY_LEVEL_DEBUG,
            trace = I_TELEMETRY_LEVEL_TRACE,
            info = I_TELEMETRY_LEVEL_INFO,
            warn = I_TELEMETRY_LEVEL_WARN,
            err = I_TELEMETRY_LEVEL_ERROR,
            critical = I_TELEMETRY_LEVEL_CRITICAL,
            off = I_TELEMETRY_LEVEL_OFF,
            n_levels
        };
        virtual ~i_telemetry_service() = default;

        virtual void on_service_creation(const char* name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) const = 0;
        virtual void on_service_deletion(zone zone_id) const = 0;
        virtual void on_service_try_cast(zone zone_id,
            destination_zone destination_zone_id,
            caller_zone caller_zone_id,
            object object_id,
            interface_ordinal interface_id) const
            = 0;
        virtual void on_service_add_ref(zone zone_id,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            rpc::add_ref_options options) const
            = 0;
        virtual void on_service_release(zone zone_id,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id) const
            = 0;

        virtual void on_service_proxy_creation(const char* service_name,
            const char* service_proxy_name,
            zone zone_id,
            destination_zone destination_zone_id,
            caller_zone caller_zone_id) const
            = 0;
        virtual void on_cloned_service_proxy_creation(const char* service_name,
            const char* service_proxy_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const
            = 0;
        virtual void on_service_proxy_deletion(
            zone zone_id, destination_zone destination_zone_id, caller_zone caller_zone_id) const
            = 0;
        virtual void on_service_proxy_try_cast(zone zone_id,
            destination_zone destination_zone_id,
            caller_zone caller_zone_id,
            object object_id,
            interface_ordinal interface_id) const
            = 0;
        virtual void on_service_proxy_add_ref(zone zone_id,
            destination_zone destination_zone_id,
            destination_channel_zone destination_channel_zone_id,
            caller_zone caller_zone_id,
            object object_id,
            rpc::add_ref_options options) const
            = 0;
        virtual void on_service_proxy_release(zone zone_id,
            destination_zone destination_zone_id,
            destination_channel_zone destination_channel_zone_id,
            caller_zone caller_zone_id,
            object object_id) const
            = 0;
        virtual void on_service_proxy_add_external_ref(zone zone_id,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            caller_zone caller_zone_id,
            int ref_count) const
            = 0;
        virtual void on_service_proxy_release_external_ref(zone zone_id,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            caller_zone caller_zone_id,
            int ref_count) const
            = 0;

        virtual void on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const = 0;
        virtual void on_impl_deletion(uint64_t address, rpc::zone zone_id) const = 0;

        virtual void on_stub_creation(zone zone_id, object object_id, uint64_t address) const = 0;
        virtual void on_stub_deletion(zone zone_id, object object_id) const = 0;
        virtual void on_stub_send(zone zone_id, object object_id, interface_ordinal interface_id, method method_id) const
            = 0;
        virtual void on_stub_add_ref(zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            uint64_t count,
            caller_zone caller_zone_id) const
            = 0;
        virtual void on_stub_release(zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            uint64_t count,
            caller_zone caller_zone_id) const
            = 0;

        virtual void on_object_proxy_creation(
            zone zone_id, destination_zone destination_zone_id, object object_id, bool add_ref_done) const
            = 0;
        virtual void on_object_proxy_deletion(zone zone_id, destination_zone destination_zone_id, object object_id) const
            = 0;

        virtual void on_interface_proxy_creation(const char* name,
            zone zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id) const
            = 0;
        virtual void on_interface_proxy_deletion(
            zone zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id) const
            = 0;
        virtual void on_interface_proxy_send(const char* method_name,
            zone zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id) const
            = 0;

        virtual void message(level_enum level, const char* message) const = 0;
    };

#if defined(USE_THREAD_LOCAL_LOGGING) && !defined(_IN_ENCLAVE)
    // Helper function to log telemetry messages to circular buffers
    inline void telemetry_to_thread_local_buffer(i_telemetry_service::level_enum level, const std::string& message) {
        // Map telemetry levels to RPC logging levels
        int rpc_level = static_cast<int>(level);
        rpc::thread_local_log(rpc_level, "[TELEMETRY] " + message, __FILE__, __LINE__, __FUNCTION__);
    }
#endif

    // Global telemetry service - defined in main.cpp (host) or set by enclave initialization
    extern std::shared_ptr<i_telemetry_service> telemetry_service_;

    // Simple function to get the global telemetry service
    inline std::shared_ptr<i_telemetry_service> get_telemetry_service() {
        return telemetry_service_;
    }

}