/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <stdint.h>

extern "C"
{
    void on_service_creation_host(const std::string& name, uint64_t zone_id, uint64_t parent_zone_id);
    void on_service_deletion_host(uint64_t zone_id);
    void on_service_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_service_add_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id, uint64_t options);
    void on_service_release_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id);

    void on_service_proxy_creation_host(const std::string& service_name,
        const std::string& service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id);
    void on_cloned_service_proxy_creation_host(const std::string& service_name,
        const std::string& service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id);
    void on_service_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id);
    void on_service_proxy_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_service_proxy_add_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t options);
    void on_service_proxy_release_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id);
    void on_service_proxy_add_external_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count);
    void on_service_proxy_release_external_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count);

    void on_impl_creation_host(const std::string& name, uint64_t address, uint64_t zone_id);
    void on_impl_deletion_host(uint64_t address, uint64_t zone_id);

    void on_stub_creation_host(uint64_t zone_id, uint64_t object_id, uint64_t address);
    void on_stub_deletion_host(uint64_t zone_id, uint64_t object_id);
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id);
    void on_stub_add_ref_host(
        uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id);
    void on_stub_release_host(
        uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id);

    void on_object_proxy_creation_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, int add_ref_done);
    void on_object_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id);

    void on_interface_proxy_creation_host(
        const std::string& name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_interface_proxy_deletion_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_interface_proxy_send_host(const std::string& method_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t interface_id,
        uint64_t method_id);

    // New transport events
    void on_transport_creation_host(const std::string& name,
        uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t status);
    void on_transport_deletion_host(uint64_t zone_id, uint64_t adjacent_zone_id);
    void on_transport_status_change_host(const std::string& name,
        uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t old_status,
        uint64_t new_status);
    void on_transport_add_destination_host(uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t destination,
        uint64_t caller);
    void on_transport_remove_destination_host(uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t destination,
        uint64_t caller);

    // New pass-through events
    void on_pass_through_creation_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t shared_count,
        uint64_t optimistic_count);
    void on_pass_through_deletion_host(uint64_t forward_destination,
        uint64_t reverse_destination);
    void on_pass_through_add_ref_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t options,
        int64_t shared_delta,
        int64_t optimistic_delta);
    void on_pass_through_release_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        int64_t shared_delta,
        int64_t optimistic_delta);
    void on_pass_through_status_change_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t forward_status,
        uint64_t reverse_status);

    // New transport events
    void on_transport_creation_host(const std::string& name,
        uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t status);
    void on_transport_deletion_host(uint64_t zone_id, uint64_t adjacent_zone_id);
    void on_transport_status_change_host(const std::string& name,
        uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t old_status,
        uint64_t new_status);
    void on_transport_add_destination_host(uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t destination,
        uint64_t caller);
    void on_transport_remove_destination_host(uint64_t zone_id,
        uint64_t adjacent_zone_id,
        uint64_t destination,
        uint64_t caller);

    // New pass-through events
    void on_pass_through_creation_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t shared_count,
        uint64_t optimistic_count);
    void on_pass_through_deletion_host(uint64_t forward_destination,
        uint64_t reverse_destination);
    void on_pass_through_add_ref_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t options,
        int64_t shared_delta,
        int64_t optimistic_delta);
    void on_pass_through_release_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        int64_t shared_delta,
        int64_t optimistic_delta);
    void on_pass_through_status_change_host(uint64_t forward_destination,
        uint64_t reverse_destination,
        uint64_t forward_status,
        uint64_t reverse_status);

    // New service events
    void on_service_creation_host(const std::string& name, uint64_t zone_id, uint64_t parent_zone_id);
    void on_service_deletion_host(uint64_t zone_id);
    void on_service_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_service_add_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id, uint64_t options);
    void on_service_release_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id);

    // New service_proxy events
    void on_service_proxy_creation_host(const std::string& service_name,
        const std::string& service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id);
    void on_cloned_service_proxy_creation_host(const std::string& service_name,
        const std::string& service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id);
    void on_service_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id);
    void on_service_proxy_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_service_proxy_add_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t options);
    void on_service_proxy_release_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id);
    void on_service_proxy_add_external_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count);
    void on_service_proxy_release_external_ref_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count);

    void message_host(uint64_t level, const std::string& name);
}
