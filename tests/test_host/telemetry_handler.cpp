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
        return root_service->add_ref(
            protocol_version, 
            {destination_channel_zone_id}, 
            {destination_zone_id}, 
            {object_id}, 
            {caller_channel_zone_id}, 
            {caller_zone_id}, 
            static_cast<rpc::add_ref_options>(build_out_param_channel), 
            false);
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

    void on_service_creation_host(const char* name, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_creation(name, {zone_id});
    }
    void on_service_deletion_host(const char* name, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_deletion(name, {zone_id});
    }
    void on_service_try_cast_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_try_cast(name, {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, {interface_id});
    }
    void on_service_add_ref_host(const char* name, uint64_t zone_id, uint64_t destination_channel_zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_channel_zone_id, uint64_t caller_zone_id, uint64_t options)
    {
        if (telemetry_service)
            telemetry_service->on_service_add_ref(name, {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {object_id}, {caller_channel_zone_id}, {caller_zone_id}, (rpc::add_ref_options) options);
    }
    void on_service_release_host(const char* name, uint64_t zone_id, uint64_t destination_channel_zone_id, uint64_t destination_zone_id, uint64_t object_id, uint64_t caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_release(name, {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {object_id}, {caller_zone_id});
    }
    
    
    void on_service_proxy_creation_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_creation(name, {zone_id}, {destination_zone_id}, {caller_zone_id});
    }
    void on_service_proxy_deletion_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_deletion(name, {zone_id}, {destination_zone_id}, {caller_zone_id});
    }
    void on_service_proxy_try_cast_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id,
                                        uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_try_cast(name, {zone_id}, {destination_zone_id}, {caller_zone_id}, {object_id}, {interface_id});
    }
    void on_service_proxy_add_ref_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t destination_channel_zone_id, 
                                       uint64_t caller_zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_ref(name, {zone_id}, {destination_zone_id}, {destination_channel_zone_id}, {caller_zone_id}, {object_id});
    }
    void on_service_proxy_release_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t destination_channel_zone_id,
                                       uint64_t caller_zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release(name, {zone_id}, {destination_zone_id}, {destination_channel_zone_id}, {caller_zone_id}, {object_id});
    }

    void on_impl_creation_host(const char* name, uint64_t address, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_creation(name, address, {zone_id});
    }
    void on_impl_deletion_host(const char* name, uint64_t address, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_deletion(name, address, {zone_id});
    }

    void on_stub_creation_host(uint64_t zone_id, uint64_t object_id, uint64_t address)
    {
        if (telemetry_service)
            telemetry_service->on_stub_creation({zone_id}, {object_id}, address);
    }
    void on_stub_deletion_host(uint64_t zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_deletion({zone_id}, {object_id});
    }
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_send({zone_id}, {object_id}, {interface_id}, {method_id});
    }
    void on_stub_add_ref_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_add_ref({zone_id}, {object_id}, {interface_id}, count, {caller_zone_id});
    }
    void on_stub_release_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count, uint64_t caller_zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_release({zone_id}, {object_id}, {interface_id}, count, {caller_zone_id});
    }

    void on_object_proxy_creation_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id, int add_ref_done)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_creation({zone_id}, {destination_zone_id}, {object_id}, !!add_ref_done);
    }
    
    void on_object_proxy_deletion_host(uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_deletion({zone_id}, {destination_zone_id}, {object_id});
    }

    void on_proxy_creation_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id,
                                uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_creation(name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id});
    }
    void on_proxy_deletion_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id,
                                uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_deletion(name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id});
    }
    void on_proxy_send_host(const char* name, uint64_t zone_id, uint64_t destination_zone_id, uint64_t object_id,
                            uint64_t interface_id, uint64_t method_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_send(name, {zone_id}, {destination_zone_id}, {object_id}, {interface_id},
                                                       {method_id});
    }

    void on_service_proxy_add_external_ref_host(const char* name, uint64_t zone_id, uint64_t destination_channel_zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_external_ref(name, {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {caller_zone_id}, ref_count);
    }
    
    void on_service_proxy_release_external_ref_host(const char* name, uint64_t zone_id, uint64_t destination_channel_zone_id, uint64_t destination_zone_id, uint64_t caller_zone_id, int ref_count)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release_external_ref(name, {zone_id}, {destination_channel_zone_id}, {destination_zone_id}, {caller_zone_id}, ref_count);
    }

    void message_host(uint64_t level, const char* name)
    {
        if (telemetry_service)
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }
}