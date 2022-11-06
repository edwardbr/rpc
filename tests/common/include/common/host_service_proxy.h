#pragma once

#include <rpc/proxy.h>

namespace rpc
{
    //This is for enclaves to call the host 
    class host_service_proxy : public service_proxy
    {
        host_service_proxy(uint64_t host_zone_id, const rpc::shared_ptr<service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service);

        rpc::shared_ptr<service_proxy> clone_for_zone(uint64_t zone_id) override
        {
            auto ret = rpc::make_shared<host_service_proxy>(*this);
            ret->set_zone_id(zone_id);
            ret->weak_this_ = ret;
            if (auto* telemetry_service = get_telemetry_service(); telemetry_service)
            {
                telemetry_service->on_service_proxy_creation("host_service_proxy", ret->get_operating_zone_id(), ret->get_zone_id());
            }
            return ret;
        }
    public:
        host_service_proxy(const host_service_proxy& other) = default;

        static rpc::shared_ptr<service_proxy> create(uint64_t host_zone_id, const rpc::shared_ptr<service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service)
        {
            auto ret = rpc::shared_ptr<host_service_proxy>(new host_service_proxy(host_zone_id, operating_zone_service, telemetry_service));
            auto pthis = rpc::static_pointer_cast<service_proxy>(ret);
            ret->weak_this_ = pthis;
            operating_zone_service->add_zone_proxy(ret);
            return pthis;
        }

    public:
        virtual ~host_service_proxy();
        int initialise();

        int send(uint64_t originating_zone_id, uint64_t zone_id, uint64_t object_id, uint64_t interface_id, uint64_t method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(uint64_t zone_id, uint64_t object_id, uint64_t interface_id) override;
        uint64_t add_ref(uint64_t zone_id, uint64_t object_id) override;
        uint64_t release(uint64_t zone_id, uint64_t object_id) override;
    };
}