#include <iostream>
#include <unordered_map>
#include <rpc/basic_service_proxies.h>

#include <host_telemetry_service.h>

extern rpc::weak_ptr<rpc::service> current_host_service;
extern const rpc::i_telemetry_service* telemetry_service;

// an ocall for logging the test
extern "C"
{
    void log_str(const char* str, size_t sz)
    {
        puts(str);
    }

    int call_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id,
                  uint64_t method_id, size_t sz_int, const char* data_in, size_t sz_out, char* data_out,
                  size_t* data_out_sz)
    {
        thread_local std::vector<char> out_buf;

        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            out_buf.clear();
            return rpc::error::TRANSPORT_ERROR();
        }
        if (out_buf.empty())
        {
            int ret = root_service->send(originating_zone_id, zone_id, object_id, interface_id, method_id, sz_int,
                                         data_in, out_buf);
            if (ret >= rpc::error::MIN() && ret <= rpc::error::MAX())
                return ret;
        }
        *data_out_sz = out_buf.size();
        if (*data_out_sz > sz_out)
            return rpc::error::NEED_MORE_MEMORY();
        memcpy(data_out, out_buf.data(), out_buf.size());
        out_buf.clear();
        return rpc::error::OK();
    }
    int try_cast_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        int ret = root_service->try_cast(zone_id, object_id, interface_id);
        return ret;
    }
    uint64_t add_ref_host(uint64_t zone_id, uint64_t object_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->add_ref(zone_id, object_id, false);
    }
    uint64_t release_host(uint64_t zone_id, uint64_t object_id)
    {
        auto root_service = current_host_service.lock();
        if (!root_service)
        {
            return rpc::error::TRANSPORT_ERROR();
        }
        return root_service->release(zone_id, object_id);
    }

    void on_service_creation_host(const char* name, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_creation(name, zone_id);
    }
    void on_service_deletion_host(const char* name, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_deletion(name, zone_id);
    }
    void on_service_proxy_creation_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_creation(name, originating_zone_id, zone_id);
    }
    void on_service_proxy_deletion_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_deletion(name, originating_zone_id, zone_id);
    }
    void on_service_proxy_try_cast_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                        uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_try_cast(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_service_proxy_add_ref_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                       uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_ref(name, originating_zone_id, zone_id, object_id);
    }
    void on_service_proxy_release_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                       uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release(name, originating_zone_id, zone_id, object_id);
    }

    void on_impl_creation_host(const char* name, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_creation(name, interface_id);
    }
    void on_impl_deletion_host(const char* name, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_impl_deletion(name, interface_id);
    }

    void on_stub_creation_host(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_creation(name, zone_id, object_id, interface_id);
    }
    void on_stub_deletion_host(const char* name, uint64_t zone_id, uint64_t object_id, uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_deletion(name, zone_id, object_id, interface_id);
    }
    void on_stub_send_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id)
    {
        if (telemetry_service)
            telemetry_service->on_stub_send(zone_id, object_id, interface_id, method_id);
    }
    void on_stub_add_ref_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count)
    {
        if (telemetry_service)
            telemetry_service->on_stub_add_ref(zone_id, object_id, interface_id, count);
    }
    void on_stub_release_host(uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t count)
    {
        if (telemetry_service)
            telemetry_service->on_stub_release(zone_id, object_id, interface_id, count);
    }

    void on_object_proxy_creation_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_creation(originating_zone_id, zone_id, object_id);
    }
    void on_object_proxy_deletion_host(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id)
    {
        if (telemetry_service)
            telemetry_service->on_object_proxy_deletion(originating_zone_id, zone_id, object_id);
    }

    void on_proxy_creation_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id,
                                uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_creation(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_proxy_deletion_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id,
                                uint64_t interface_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_deletion(name, originating_zone_id, zone_id, object_id, interface_id);
    }
    void on_proxy_send_host(const char* name, uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id,
                            uint64_t interface_id, uint64_t method_id)
    {
        if (telemetry_service)
            telemetry_service->on_interface_proxy_send(name, originating_zone_id, zone_id, object_id, interface_id,
                                                       method_id);
    }

    void on_service_proxy_add_external_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                           int ref_count)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_add_external_ref(name, originating_zone_id, zone_id, ref_count);
    }
    void on_service_proxy_release_external_ref(const char* name, uint64_t originating_zone_id, uint64_t zone_id,
                                               int ref_count)
    {
        if (telemetry_service)
            telemetry_service->on_service_proxy_release_external_ref(name, originating_zone_id, zone_id, ref_count);
    }

    void message_host(uint64_t level, const char* name)
    {
        if (telemetry_service)
            telemetry_service->message((rpc::i_telemetry_service::level_enum)level, name);
    }
}