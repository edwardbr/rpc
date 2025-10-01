/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/rpc.h>

namespace rpc
{
    // This is for enclaves to call the host
    class host_service_proxy : public service_proxy
    {
        host_service_proxy(const char* name, destination_zone host_zone_id, const std::shared_ptr<rpc::child_service>& svc);
        host_service_proxy(const host_service_proxy& other) = default;

        std::shared_ptr<rpc::service_proxy> clone() override
        {
            return std::shared_ptr<host_service_proxy>(new host_service_proxy(*this));
        }

        static std::shared_ptr<rpc::service_proxy> create(
            const char* name, destination_zone host_zone_id, const std::shared_ptr<rpc::child_service>& svc);

        int initialise();

        int send(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_) override;
        int try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id) override;
        int add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count) override;
        int release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            rpc::release_options options,
            uint64_t& reference_count) override;

        friend rpc::child_service;

    public:
        virtual ~host_service_proxy() = default;
    };
}
