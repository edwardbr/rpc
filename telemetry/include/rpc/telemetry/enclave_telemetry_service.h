#pragma once

#include <rpc/telemetry/i_telemetry_service.h>

namespace rpc
{
    class enclave_telemetry_service : public i_telemetry_service
    {
        enclave_telemetry_service();

    public:
        static bool create(std::shared_ptr<i_telemetry_service>& out)
        {
            out = std::shared_ptr<enclave_telemetry_service>(new enclave_telemetry_service());
            return true;
        }
        virtual ~enclave_telemetry_service() = default;

        void on_service_creation(const char* name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) const override;
        void on_service_deletion(rpc::zone zone_id) const override;
        void on_service_try_cast(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_service_add_ref(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::add_ref_options options) const override;
        void on_service_release(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id) const override;

        void on_service_proxy_creation(const char* service_name,
            const char* service_proxy_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const override;
        void on_cloned_service_proxy_creation(const char* service_name,
            const char* service_proxy_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const override;
        void on_service_proxy_deletion(
            rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const override;
        void on_service_proxy_try_cast(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_service_proxy_add_ref(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::add_ref_options options) const override;
        void on_service_proxy_release(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id) const override;
        void on_service_proxy_add_external_ref(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            int ref_count) const override;
        void on_service_proxy_release_external_ref(rpc::zone zone_id,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            int ref_count) const override;

        void on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const override;
        void on_impl_deletion(uint64_t address, rpc::zone zone_id) const override;

        void on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const override;
        void on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const override;
        void on_stub_send(rpc::zone zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id) const override;
        void on_stub_add_ref(rpc::zone zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            uint64_t count,
            rpc::caller_zone caller_zone_id) const override;
        void on_stub_release(rpc::zone zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            uint64_t count,
            rpc::caller_zone caller_zone_id) const override;

        void on_object_proxy_creation(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            bool add_ref_done) const override;
        void on_object_proxy_deletion(
            rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const override;

        void on_interface_proxy_creation(const char* name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_deletion(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_send(const char* method_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id) const override;

        void message(level_enum level, const char* message) const override;
    };
}
