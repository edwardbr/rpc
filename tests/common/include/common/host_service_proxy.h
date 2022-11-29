#pragma once

#include <rpc/proxy.h>

namespace rpc
{
    //This is for enclaves to call the host 
    class host_service_proxy : public service_proxy
    {
        host_service_proxy(uint64_t host_zone_id, const rpc::shared_ptr<service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service);

        rpc::shared_ptr<service_proxy> clone_for_zone(uint64_t zone_id) override;
    public:
        host_service_proxy(const host_service_proxy& other) = default;

        static rpc::shared_ptr<service_proxy> create(uint64_t host_zone_id, const rpc::shared_ptr<rpc::child_service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service);

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