/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <sgx_urts.h>
#include <rpc/rpc.h>

namespace rpc
{
    // This is for hosts to call services on an enclave
    class enclave_service_proxy : public service_proxy
    {
        struct enclave_owner
        {
        public:
            enclave_owner(uint64_t eid)
                : eid_(eid)
            {
            }
            uint64_t eid_ = 0;
            ~enclave_owner();
        };

        enclave_service_proxy(const char* name,
            destination_zone destination_zone_id,
            std::string filename,
            const std::shared_ptr<rpc::service>& svc);

        enclave_service_proxy(const enclave_service_proxy& other) = default;

        std::shared_ptr<rpc::service_proxy> clone() override;

        static std::shared_ptr<enclave_service_proxy> create(const char* name,
            destination_zone destination_zone_id,
            const std::shared_ptr<rpc::service>& svc,
            std::string filename);

        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override;

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
            std::vector<char>& out_buf_,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel) override;
        void post(uint64_t protocol_version,
            encoding encoding,
            uint64_t tag,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            method method_id,
            post_options options,
            size_t in_size_,
            const char* in_buf_,
            const std::vector<back_channel_entry>& in_back_channel) override;
        int try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel) override;
        int add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel) override;
        int release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            rpc::release_options options,
            uint64_t& reference_count,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel) override;

        std::shared_ptr<enclave_owner> enclave_owner_;
        uint64_t eid_ = 0;
        std::string filename_;

        friend rpc::service;

    public:
        virtual ~enclave_service_proxy() = default;
    };
}
