/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <iostream>
#include <unordered_map>
#include <rpc/basic_service_proxies.h>

#include <host_telemetry_service.h>

extern rpc::weak_ptr<rpc::service> current_host_service;
extern const rpc::i_telemetry_service* telemetry_service;

// an ocall for logging the test
extern "C"
{
    int call_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t encoding                                  //format of the serialised data
        , uint64_t tag                                       //info on the type of the call 
        , uint64_t caller_channel_zone_id
        , uint64_t caller_zone_id
        , uint64_t destination_zone_id
        , uint64_t object_id
        , uint64_t interface_id
        , uint64_t method_id
        , size_t sz_int
        , const char* data_in
        , size_t sz_out
        , char* data_out
        , size_t* data_out_sz)
    {
        thread_local rpc::retry_buffer out_buf;

        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            out_buf.data.clear();
            return rpc::error::TRANSPORT_ERROR();
        }
        if (out_buf.data.empty())
        {
            out_buf.return_value = root_service->send(protocol_version, rpc::encoding(encoding), tag, {caller_channel_zone_id}, {caller_zone_id}, {destination_zone_id}, {object_id}, {interface_id}, {method_id}, sz_int,
                                         data_in, out_buf.data);
            if (out_buf.return_value >= rpc::error::MIN() && out_buf.return_value <= rpc::error::MAX())
                return out_buf.return_value;
        }
        *data_out_sz = out_buf.data.size();
        if (*data_out_sz > sz_out)
            return rpc::error::NEED_MORE_MEMORY();
        memcpy(data_out, out_buf.data.data(), out_buf.data.size());
        out_buf.data.clear();
        return out_buf.return_value;
    }
    int try_cast_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t zone_id
        , uint64_t object_id
        , uint64_t interface_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        int ret = root_service->try_cast(protocol_version, {zone_id}, {object_id}, {interface_id});
        return ret;
    }
    uint64_t add_ref_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t destination_channel_zone_id
        , uint64_t destination_zone_id
        , uint64_t object_id
        , uint64_t caller_channel_zone_id
        , uint64_t caller_zone_id
        , char build_out_param_channel)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->add_ref(protocol_version, {destination_channel_zone_id}, {destination_zone_id}, {object_id}, {caller_channel_zone_id}, {caller_zone_id}, static_cast<rpc::add_ref_options>(build_out_param_channel), false);
    }
    uint64_t release_host(
        uint64_t protocol_version                          //version of the rpc call protocol
        , uint64_t zone_id
        , uint64_t object_id
        , uint64_t caller_zone_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->release(protocol_version, {zone_id}, {object_id}, {caller_zone_id});
    }

    void on_service_creation_host(const char* name, rpc::zone zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_creation(name, zone_id);
    }
    void on_service_deletion_host(const char* name, rpc::zone zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_deletion(name, zone_id);
    }
    void on_service_proxy_creation_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_creation(name, zone_id, destination_zone_id, caller_zone_id);
    }
    void on_service_proxy_deletion_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_deletion(name, zone_id, destination_zone_id, caller_zone_id);
    }
    void on_service_proxy_try_cast_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id,
                                        rpc::object object_id, rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_try_cast(name, zone_id, destination_zone_id, object_id, interface_id);
    }
    void on_service_proxy_add_ref_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id,
                                       rpc::object object_id, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_ref(name, zone_id, destination_zone_id, object_id, caller_zone_id);
    }
    void on_service_proxy_release_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id,
                                       rpc::object object_id, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release(name, zone_id, destination_zone_id, object_id, caller_zone_id);
    }

    void on_impl_creation_host(const char* name, rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_creation(name, interface_id);
    }
    void on_impl_deletion_host(const char* name, rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_deletion(name, interface_id);
    }

    void on_stub_creation_host(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_creation(name, zone_id, object_id, interface_id);
    }
    void on_stub_deletion_host(const char* name, rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_deletion(name, zone_id, object_id, interface_id);
    }
    void on_stub_send_host(rpc::zone zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, rpc::method method_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_send(zone_id, object_id, interface_id, method_id);
    }
    void on_stub_add_ref_host(rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_add_ref(destination_zone_id, object_id, interface_id, count, caller_zone_id);
    }
    void on_stub_release_host(rpc::destination_zone destination_zone_id, rpc::object object_id, rpc::interface_ordinal interface_id, uint64_t count, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_release(destination_zone_id, object_id, interface_id, count, caller_zone_id);
    }

    void on_object_proxy_creation_host(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_creation(zone_id, destination_zone_id, object_id);
    }
    void on_object_proxy_deletion_host(rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_deletion(zone_id, destination_zone_id, object_id);
    }

    void on_proxy_creation_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id,
                                rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_creation(name, zone_id, destination_zone_id, object_id, interface_id);
    }
    void on_proxy_deletion_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id,
                                rpc::interface_ordinal interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_deletion(name, zone_id, destination_zone_id, object_id, interface_id);
    }
    void on_proxy_send_host(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id, rpc::object object_id,
                            rpc::interface_ordinal interface_id, rpc::method method_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_send(name, zone_id, destination_zone_id, object_id, interface_id,
                                                       method_id);
    }

    void on_service_proxy_add_external_ref(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id,
                                           int ref_count, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_external_ref(name, zone_id, destination_zone_id, ref_count, caller_zone_id);
    }
    void on_service_proxy_release_external_ref(const char* name, rpc::zone zone_id, rpc::destination_zone destination_zone_id,
                                               int ref_count, rpc::caller_zone caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release_external_ref(name, zone_id, destination_zone_id, ref_count, caller_zone_id);
    }

    void message_host(uint64_t level, const char* name)
    {
        if (telemetry_service)
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }
}