/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <unordered_map>

#include <rpc/rpc.h>
#include <rpc/telemetry/i_telemetry_service.h>
#include <rpc/telemetry/telemetry_handler.h>

// an ocall for logging the test
extern "C"
{
    void on_service_creation_host(const std::string& name, uint64_t zone_id, uint64_t parent_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_creation(name, {zone_id}, {parent_zone_id});
    }
    void on_service_deletion_host(uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_deletion({zone_id});
    }
    void on_service_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_try_cast(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, {interface_id});
    }
    void on_service_add_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id, uint64_t options)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_add_ref(
                {zone_id}, {destination_zone_id}, {object_id}, {caller_zone_id}, (rpc::add_ref_options)options);
    }
    void on_service_release_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_release({zone_id}, {destination_zone_id}, {object_id}, {caller_zone_id});
    }

    void on_service_proxy_creation_host(const std::string& service_name,
        const std::string& service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_creation(
                service_name, service_proxy_name, {zone_id}, {destination_zone_id}, {caller_zone_id});
    }

    void on_cloned_service_proxy_creation_host(const std::string& service_name,
        const std::string& service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_cloned_service_proxy_creation(
                service_name, service_proxy_name, {zone_id}, {destination_zone_id}, {caller_zone_id});
    }
    void on_service_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_deletion({zone_id}, {destination_zone_id}, {caller_zone_id});
    }
    void on_service_proxy_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_try_cast(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, {interface_id});
    }
    void on_service_proxy_add_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t options)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_add_ref(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, (rpc::add_ref_options)options);
    }
    void on_service_proxy_release_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_release({zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id});
    }

    void on_service_proxy_add_external_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_add_external_ref(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, ref_count);
    }

    void on_service_proxy_release_external_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_service_proxy_release_external_ref(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, ref_count);
    }

    void on_impl_creation_host(const std::string& name, uint64_t address, uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_impl_creation(name, address, {zone_id});
    }
    void on_impl_deletion_host(uint64_t address, uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_impl_deletion(address, {zone_id});
    }

    void on_stub_creation_host(uint64_t zone_id, uint64_t object_id, uint64_t address)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_stub_creation({zone_id}, {object_id}, address);
    }
    void on_stub_deletion_host(uint64_t zone_id, uint64_t object_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_stub_deletion({zone_id}, {object_id});
    }
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_stub_send({zone_id}, {object_id}, {interface_id}, {method_id});
    }
    void on_stub_add_ref_host(
        uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_stub_add_ref({zone_id}, {object_id}, {interface_id}, count, {caller_zone_id});
    }
    void on_stub_release_host(
        uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_stub_release({zone_id}, {object_id}, {interface_id}, count, {caller_zone_id});
    }

    void on_object_proxy_creation_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, int add_ref_done)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_object_proxy_creation({zone_id}, {destination_zone_id}, {object_id}, !!add_ref_done);
    }

    void on_object_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_object_proxy_deletion({zone_id}, {destination_zone_id}, {object_id});
    }

    void on_interface_proxy_creation_host(
        const std::string& name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_interface_proxy_creation(
                name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id});
    }
    void on_interface_proxy_deletion_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_interface_proxy_deletion({zone_id}, {destination_zone_id}, {object_id}, {interface_id});
    }
    void on_interface_proxy_send_host(const std::string& method_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t interface_id,
        uint64_t method_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_interface_proxy_send(
                method_name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id}, {method_id});
    }

    // New transport events
    void on_transport_creation_host(const std::string& name, uint64_t zone_id, uint64_t adjacent_zone_id, uint64_t status)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_transport_creation(name, {zone_id}, {adjacent_zone_id}, (rpc::transport_status)status);
    }
    void on_transport_deletion_host(uint64_t zone_id, uint64_t adjacent_zone_id)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_transport_deletion({zone_id}, {adjacent_zone_id});
    }
    void on_transport_status_change_host(
        const std::string& name, uint64_t zone_id, uint64_t adjacent_zone_id, uint64_t old_status, uint64_t new_status)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_transport_status_change(
                name, {zone_id}, {adjacent_zone_id}, (rpc::transport_status)old_status, (rpc::transport_status)new_status);
    }
    void on_transport_add_destination_host(uint64_t zone_id, uint64_t adjacent_zone_id, uint64_t destination, uint64_t caller)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_transport_add_destination({zone_id}, {adjacent_zone_id}, {destination}, {caller});
    }
    void on_transport_remove_destination_host(
        uint64_t zone_id, uint64_t adjacent_zone_id, uint64_t destination, uint64_t caller)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_transport_remove_destination({zone_id}, {adjacent_zone_id}, {destination}, {caller});
    }

    // New pass-through events
    void on_pass_through_creation_host(
        uint64_t forward_destination, uint64_t reverse_destination, uint64_t shared_count, uint64_t optimistic_count)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_pass_through_creation(
                {forward_destination}, {reverse_destination}, shared_count, optimistic_count);
    }
    void on_pass_through_deletion_host(uint64_t forward_destination, uint64_t reverse_destination)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_pass_through_deletion({forward_destination}, {reverse_destination});
    }
    void on_pass_through_add_ref_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t options,
        int64_t shared_delta,
        int64_t optimistic_delta)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_pass_through_add_ref(
                {forward_destination}, {reverse_destination}, (rpc::add_ref_options)options, shared_delta, optimistic_delta);
    }
    void on_pass_through_release_host(
        uint64_t forward_destination, uint64_t reverse_destination, int64_t shared_delta, int64_t optimistic_delta)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_pass_through_release(
                {forward_destination}, {reverse_destination}, shared_delta, optimistic_delta);
    }
    void on_pass_through_status_change_host(
        uint64_t forward_destination, uint64_t reverse_destination, uint64_t forward_status, uint64_t reverse_status)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->on_pass_through_status_change({forward_destination},
                {reverse_destination},
                (rpc::transport_status)forward_status,
                (rpc::transport_status)reverse_status);
    }

    void message_host(uint64_t level, const std::string& name)
    {
        if (auto telemetry_service = rpc::get_telemetry_service())
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }
}
