#pragma once

#include <rpc/proxy.h>

namespace rpc
{
    //This is for enclaves to call the host 
    class host_service_proxy : public service_proxy
    {
        host_service_proxy(destination_zone host_zone_id, const rpc::shared_ptr<service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service);

        rpc::shared_ptr<service_proxy> deep_copy_for_clone() override {return rpc::make_shared<host_service_proxy>(*this);}

    public:
        host_service_proxy(const host_service_proxy& other) = default;

        static rpc::shared_ptr<service_proxy> create(destination_zone host_zone_id, object host_id, const rpc::shared_ptr<rpc::child_service>& operating_zone_service, const rpc::i_telemetry_service* telemetry_service);

    public:
        virtual ~host_service_proxy();
        int initialise();

        int send(caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id, method method_id, size_t in_size_,
                        const char* in_buf_, std::vector<char>& out_buf_) override;
        int try_cast(destination_zone destination_zone_id, object object_id, interface_ordinal interface_id) override;
        uint64_t add_ref(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id, bool proxy_add_ref) override;
        uint64_t release(destination_zone destination_zone_id, object object_id, caller_zone caller_zone_id) override;
    };
}