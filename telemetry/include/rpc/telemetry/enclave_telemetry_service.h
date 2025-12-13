/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
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

        void on_service_creation(
            const std::string& name, rpc::zone zone_id, rpc::destination_zone parent_zone_id) const override;
        void on_service_deletion(rpc::zone zone_id) const override;
        void on_service_try_cast(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_service_add_ref(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id,
            rpc::add_ref_options options) const override;
        void on_service_release(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id) const override;

        void on_service_proxy_creation(const std::string& service_name,
            const std::string& service_proxy_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id) const override;
        void on_cloned_service_proxy_creation(const std::string& service_name,
            const std::string& service_proxy_name,
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
            rpc::caller_zone caller_zone_id,
            rpc::object object_id,
            rpc::add_ref_options options) const override;
        void on_service_proxy_release(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::object object_id) const override;
        void on_service_proxy_add_external_ref(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            int ref_count) const override;
        void on_service_proxy_release_external_ref(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::caller_zone caller_zone_id,
            int ref_count) const override;

        void on_impl_creation(const std::string& name, uint64_t address, rpc::zone zone_id) const override;
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

        void on_interface_proxy_creation(const std::string& name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_deletion(rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id) const override;
        void on_interface_proxy_send(const std::string& method_name,
            rpc::zone zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id) const override;

        void message(level_enum level, const std::string& message) const override;

        // Transport events
        void on_transport_creation(
            const std::string& name, rpc::zone zone_id, rpc::zone adjacent_zone_id, rpc::transport_status status) const override;
        void on_transport_deletion(rpc::zone zone_id, rpc::zone adjacent_zone_id) const override;
        void on_transport_status_change(const std::string& name,
            rpc::zone zone_id,
            rpc::zone adjacent_zone_id,
            rpc::transport_status old_status,
            rpc::transport_status new_status) const override;
        void on_transport_add_destination(
            rpc::zone zone_id, rpc::zone adjacent_zone_id, rpc::destination_zone destination, rpc::caller_zone caller) const override;
        void on_transport_remove_destination(
            rpc::zone zone_id, rpc::zone adjacent_zone_id, rpc::destination_zone destination, rpc::caller_zone caller) const override;

        // Pass-through events
        void on_pass_through_creation(rpc::destination_zone forward_destination,
            rpc::destination_zone reverse_destination,
            uint64_t shared_count,
            uint64_t optimistic_count) const override;
        void on_pass_through_deletion(
            rpc::destination_zone forward_destination, rpc::destination_zone reverse_destination) const override;
        void on_pass_through_add_ref(rpc::destination_zone forward_destination,
            rpc::destination_zone reverse_destination,
            rpc::add_ref_options options,
            int64_t shared_delta,
            int64_t optimistic_delta) const override;
        void on_pass_through_release(rpc::destination_zone forward_destination,
            rpc::destination_zone reverse_destination,
            int64_t shared_delta,
            int64_t optimistic_delta) const override;
        void on_pass_through_status_change(rpc::destination_zone forward_destination,
            rpc::destination_zone reverse_destination,
            rpc::transport_status forward_status,
            rpc::transport_status reverse_status) const override;
    };
}
