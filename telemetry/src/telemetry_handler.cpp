#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>

#include <rpc/basic_service_proxies.h>

#include <rpc/telemetry/telemetry_handler.h>
#include <rpc/telemetry/host_telemetry_service.h>

using namespace std::chrono_literals;

// an ocall for logging the test
extern "C"
{
    void on_service_creation_host(const char* name, uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_creation(name, {zone_id});
    }
    void on_service_deletion_host(uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_deletion({zone_id});
    }
    void on_service_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_try_cast(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, {interface_id});
    }
    void on_service_add_ref_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t caller_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t options)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_add_ref({zone_id},
                {destination_channel_zone_id},
                {destination_zone_id},
                {object_id},
                {caller_channel_zone_id},
                {caller_zone_id},
                (rpc::add_ref_options)options);
    }
    void on_service_release_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_release(
                {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {object_id}, {caller_zone_id});
    }

    void on_service_proxy_creation_host(const char* service_name,
        const char* service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_creation(
                service_name, service_proxy_name, {zone_id}, {destination_zone_id}, {caller_zone_id});
    }

    void on_cloned_service_proxy_creation_host(const char* service_name,
        const char* service_proxy_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_cloned_service_proxy_creation(
                service_name, service_proxy_name, {zone_id}, {destination_zone_id}, {caller_zone_id});
    }
    void on_service_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_deletion({zone_id}, {destination_zone_id}, {caller_zone_id});
    }
    void on_service_proxy_try_cast_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_try_cast(
                {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, {interface_id});
    }
    void on_service_proxy_add_ref_host(uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t object_id,
        uint64_t options)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_add_ref({zone_id},
                {destination_zone_id},
                {destination_channel_zone_id},
                {caller_zone_id},
                {object_id},
                (rpc::add_ref_options)options);
    }
    void on_service_proxy_release_host(uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t caller_zone_id,
        uint64_t object_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_release(
                {zone_id}, {destination_zone_id}, {destination_channel_zone_id}, {caller_zone_id}, {object_id});
    }

    void on_service_proxy_add_external_ref_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id,
        int ref_count)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_add_external_ref(
                {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {caller_zone_id}, ref_count);
    }

    void on_service_proxy_release_external_ref_host(uint64_t zone_id,
        uint64_t destination_channel_zone_id,
        uint64_t destination_zone_id,
        uint64_t caller_zone_id,
        int ref_count)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_service_proxy_release_external_ref(
                {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {caller_zone_id}, ref_count);
    }

    void on_impl_creation_host(const char* name, uint64_t address, uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_impl_creation(name, address, {zone_id});
    }
    void on_impl_deletion_host(uint64_t address, uint64_t zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_impl_deletion(address, {zone_id});
    }

    void on_stub_creation_host(uint64_t zone_id, uint64_t object_id, uint64_t address)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_stub_creation({zone_id}, {object_id}, address);
    }
    void on_stub_deletion_host(uint64_t zone_id, uint64_t object_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_stub_deletion({zone_id}, {object_id});
    }
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_stub_send({zone_id}, {object_id}, {interface_id}, {method_id});
    }
    void on_stub_add_ref_host(
        uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_stub_add_ref({zone_id}, {object_id}, {interface_id}, count, {caller_zone_id});
    }
    void on_stub_release_host(
        uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_stub_release({zone_id}, {object_id}, {interface_id}, count, {caller_zone_id});
    }

    void on_object_proxy_creation_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, int add_ref_done)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_object_proxy_creation({zone_id}, {destination_zone_id}, {object_id}, !!add_ref_done);
    }

    void on_object_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_object_proxy_deletion({zone_id}, {destination_zone_id}, {object_id});
    }

    void on_interface_proxy_creation_host(
        const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_interface_proxy_creation(
                name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id});
    }
    void on_interface_proxy_deletion_host(
        uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_interface_proxy_deletion({zone_id}, {destination_zone_id}, {object_id}, {interface_id});
    }
    void on_interface_proxy_send_host(const char* method_name,
        uint64_t zone_id,
        uint64_t destination_zone_id,
        uint64_t object_id,
        uint64_t interface_id,
        uint64_t method_id)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->on_interface_proxy_send(
                method_name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id}, {method_id});
    }
    void message_host(uint64_t level, const char* name)
    {
        if (auto telemetry_service = rpc::telemetry_service_manager::get())
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }
}
