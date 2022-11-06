#pragma once

#include <rpc/proxy.h>
#include <rpc/error_codes.h>
#include <rpc/i_telemetry_service.h>

namespace rpc
{
    class local_service_proxy : public service_proxy
    {
        local_service_proxy(const rpc::shared_ptr<service>& serv,
                            const rpc::i_telemetry_service* telemetry_service)
            : service_proxy(serv, serv, telemetry_service)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_service_proxy", get_operating_zone_id(), get_zone_id());
            }
        }
        rpc::shared_ptr<service_proxy> clone_for_zone(uint64_t zone_id) override
        {
            auto ret = rpc::make_shared<local_service_proxy>(*this);
            ret->set_zone_id(zone_id);
            ret->weak_this_ = ret;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_service_proxy", ret->get_operating_zone_id(), ret->get_zone_id());
            }
            return ret;
        }

    public:
        local_service_proxy(const local_service_proxy& other) = default;

        virtual ~local_service_proxy()
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_deletion("local_service_proxy", get_operating_zone_id(), get_zone_id());
            }
        }
        static rpc::shared_ptr<local_service_proxy> create(const rpc::shared_ptr<service>& serv,
                                                           const rpc::i_telemetry_service* telemetry_service)
        {
            auto ret = rpc::shared_ptr<local_service_proxy>(new local_service_proxy(serv, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            return ret;
        }

        int send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                 const char* in_buf_, std::vector<char>& out_buf_) override
        {
            return get_service()->send(originating_zone_id, zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast("local_service_proxy", get_operating_zone_id(), zone_id,
                                                             object_id, interface_id);
            }
            return get_service()->try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref("local_service_proxy", get_operating_zone_id(), zone_id,
                                                            object_id);
            }
            return get_service()->add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release("local_service_proxy", get_operating_zone_id(), zone_id,
                                                            object_id);
            }
            return get_service()->release(zone_id, object_id);
        }
    };

    class local_child_service_proxy : public service_proxy
    {
        local_child_service_proxy(const rpc::shared_ptr<service>& serv,
                                  const rpc::shared_ptr<service>& operating_zone_service,
                                  const rpc::i_telemetry_service* telemetry_service)
            : service_proxy(serv, operating_zone_service, telemetry_service)
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_child_service_proxy", get_operating_zone_id(), get_zone_id());
            }
        }

        rpc::shared_ptr<service_proxy> clone_for_zone(uint64_t zone_id) override
        {
            auto ret = rpc::make_shared<local_child_service_proxy>(*this);
            ret->set_zone_id(zone_id);
            ret->weak_this_ = ret;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("local_child_service_proxy", ret->get_operating_zone_id(), ret->get_zone_id());
            }
            return ret;
        }

    public:
        local_child_service_proxy(const local_child_service_proxy& other) = default;
        virtual ~local_child_service_proxy()
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_deletion("local_child_service_proxy", get_operating_zone_id(), get_zone_id());
            }
        }
        static rpc::shared_ptr<local_child_service_proxy> create(const rpc::shared_ptr<service>& serv,
                                                                 const rpc::shared_ptr<service>& operating_zone_service,
                                                                 const rpc::i_telemetry_service* telemetry_service)
        {
            auto ret = rpc::shared_ptr<local_child_service_proxy>(
                new local_child_service_proxy(serv, operating_zone_service, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            operating_zone_service->add_zone_proxy(pthis);
            return ret;
        }

        int send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                 const char* in_buf_, std::vector<char>& out_buf_) override
        {
            return get_service()->send(originating_zone_id, zone_id, object_id, interface_id, method_id, in_size_, in_buf_, out_buf_);
        }
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_try_cast("local_child_service_proxy", get_operating_zone_id(),
                                                             zone_id, object_id, interface_id);
            }
            return get_service()->try_cast(zone_id, object_id, interface_id);
        }
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_add_ref("local_child_service_proxy", get_operating_zone_id(),
                                                            zone_id, object_id);
            }

            return get_service()->add_ref(zone_id, object_id);
        }
        uint64_t release(uint64_t zone_id, uint64_t object_id) override
        {
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_release("local_child_service_proxy", get_operating_zone_id(),
                                                            zone_id, object_id);
            }
            return get_service()->release(zone_id, object_id);
        }
    };
}