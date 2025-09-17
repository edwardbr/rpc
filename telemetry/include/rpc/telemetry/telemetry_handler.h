/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <stdint.h>

extern "C"
{
    void on_service_creation_host(const char* name, uint64_t zone_id, uint64_t parent_zone_id);
    void on_service_deletion_host(uint64_t zone_id);
    void on_service_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_service_add_ref_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t caller_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t options);
    void on_service_release_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t caller_zone_id);

    void on_service_proxy_creation_host(const char* service_name,
        const char* service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id);
    void on_cloned_service_proxy_creation_host(const char* service_name,
        const char* service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id);
    void on_service_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id);
    void on_service_proxy_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_service_proxy_add_ref_host(uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t object_id,
        uint64_t options);
    void on_service_proxy_release_host(uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t object_id);
    void on_service_proxy_add_external_ref_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id,
        int ref_count);
    void on_service_proxy_release_external_ref_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id,
        int ref_count);

    void on_impl_creation_host(const char* name, uint64_t address, uint64_t zone_id);
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
        const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_interface_proxy_deletion_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id);
    void on_interface_proxy_send_host(const char* method_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t interface_id,
        uint64_t method_id);

    void message_host(uint64_t level, const char* name);
}
