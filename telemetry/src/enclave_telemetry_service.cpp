#include <rpc/telemetry/enclave_telemetry_service.h>
#include <trusted/rpc_telemetry_t.h>

namespace rpc
{
    void enclave_telemetry_service::on_service_creation(const char* name, rpc::zone zone_id) const
    {
        on_service_creation_host(name, zone_id.get_val());
    }

    void enclave_telemetry_service::on_service_deletion(rpc::zone zone_id) const
    {
        on_service_deletion_host(zone_id.get_val());
    }
    void enclave_telemetry_service::on_service_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        on_service_try_cast_host(zone_id.get_val(),
            destination_zone_id.get_val(),
            caller_zone_id.get_val(),
            object_id.get_val(),
            interface_id.get_val());
    }

    void enclave_telemetry_service::on_service_add_ref(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::add_ref_options options) const
    {
        on_service_add_ref_host(zone_id.get_val(),
            destination_channel_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            caller_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            (uint64_t)options);
    }

    void enclave_telemetry_service::on_service_release(rpc::zone zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id) const
    {
        on_service_release_host(zone_id.get_val(),
            destination_channel_zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            caller_zone_id.get_val());
    }
    void enclave_telemetry_service::on_service_proxy_creation(
        const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_proxy_creation_host(name, zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    void enclave_telemetry_service::on_service_proxy_deletion(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id) const
    {
        on_service_proxy_deletion_host(zone_id.get_val(), destination_zone_id.get_val(), caller_zone_id.get_val());
    }
    void enclave_telemetry_service::on_service_proxy_try_cast(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        on_service_proxy_try_cast_host(zone_id.get_val(),
            destination_zone_id.get_val(),
            caller_zone_id.get_val(),
            object_id.get_val(),
            interface_id.get_val());
    }
    void enclave_telemetry_service::on_service_proxy_add_ref(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id,
        rpc::add_ref_options options) const
    {
        on_service_proxy_add_ref_host(zone_id.get_val(),
            destination_zone_id.get_val(),
            destination_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            object_id.get_val(),
            (uint64_t)options);
    }
    void enclave_telemetry_service::on_service_proxy_release(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::object object_id) const
    {
        on_service_proxy_release_host(zone_id.get_val(),
            destination_zone_id.get_val(),
            destination_channel_zone_id.get_val(),
            caller_zone_id.get_val(),
            object_id.get_val());
    }

    void enclave_telemetry_service::on_impl_creation(const char* name, uint64_t address, rpc::zone zone_id) const
    {
        on_impl_creation_host(name, address, zone_id.get_val());
    }
    void enclave_telemetry_service::on_impl_deletion(uint64_t address, rpc::zone zone_id) const
    {
        on_impl_deletion_host(address, zone_id.get_val());
    }

    void enclave_telemetry_service::on_stub_creation(rpc::zone zone_id, rpc::object object_id, uint64_t address) const
    {
        on_stub_creation_host(zone_id.get_val(), object_id.get_val(), address);
    }
    void enclave_telemetry_service::on_stub_deletion(rpc::zone zone_id, rpc::object object_id) const
    {
        on_stub_deletion_host(zone_id.get_val(), object_id.get_val());
    }
    void enclave_telemetry_service::on_stub_send(
        rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id) const
    {
        on_stub_send_host(zone_id.get_val(), object_id.get_val(), interface_id.get_val(), method_id.get_val());
    }
    void enclave_telemetry_service::on_stub_add_ref(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        on_stub_add_ref_host(
            zone_id.get_val(), object_id.get_val(), interface_id.get_val(), count, caller_zone_id.get_val());
    }
    void enclave_telemetry_service::on_stub_release(rpc::zone zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        uint64_t count,
        rpc::caller_zone caller_zone_id) const
    {
        on_stub_release_host(
            zone_id.get_val(), object_id.get_val(), interface_id.get_val(), count, caller_zone_id.get_val());
    }

    void enclave_telemetry_service::on_object_proxy_creation(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id, bool add_ref_done) const
    {
        on_object_proxy_creation_host(zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), add_ref_done);
    }
    void enclave_telemetry_service::on_object_proxy_deletion(
        rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id) const
    {
        on_object_proxy_deletion_host(zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val());
    }

    void enclave_telemetry_service::on_interface_proxy_creation(const char* name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        on_interface_proxy_creation_host(
            name, zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    void enclave_telemetry_service::on_interface_proxy_deletion(rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id) const
    {
        on_interface_proxy_deletion_host(
            zone_id.get_val(), destination_zone_id.get_val(), object_id.get_val(), interface_id.get_val());
    }
    void enclave_telemetry_service::on_interface_proxy_send(const char* method_name,
        rpc::zone zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        rpc::method method_id) const
    {
        on_interface_proxy_send_host(method_name,
            zone_id.get_val(),
            destination_zone_id.get_val(),
            object_id.get_val(),
            interface_id.get_val(),
            method_id.get_val());
    }

    void enclave_telemetry_service::on_service_proxy_add_external_ref(rpc::zone operating_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        on_service_proxy_add_external_ref_host(operating_zone_id.get_val(),
            destination_channel_zone_id.get_val(),
            destination_zone_id.get_val(),
            caller_zone_id.get_val(),
            ref_count);
    }

    void enclave_telemetry_service::on_service_proxy_release_external_ref(rpc::zone operating_zone_id,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::caller_zone caller_zone_id,
        int ref_count) const
    {
        on_service_proxy_release_external_ref_host(operating_zone_id.get_val(),
            destination_channel_zone_id.get_val(),
            destination_zone_id.get_val(),
            caller_zone_id.get_val(),
            ref_count);
    }

    void enclave_telemetry_service::message(rpc::i_telemetry_service::level_enum level, const char* message) const
    {
        message_host(level, message);
    }
}
