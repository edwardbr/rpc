/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/proxy.h>
#include <coro/coro.hpp>
#include <coro/net/ip_address.hpp>

#include <tcp/tcp.h>

#include "common/spsc/channel_manager.h"

namespace rpc::spsc
{
    // This is for hosts to call services on an enclave
    class service_proxy : public rpc::service_proxy
    {
        service_proxy(const char* name, destination_zone destination_zone_id, const rpc::shared_ptr<service>& svc,
                      std::shared_ptr<rpc::spsc::worker_release> connection, std::chrono::milliseconds timeout,
                      queue_type* send_spsc_queue, queue_type* receive_spsc_queue);

        service_proxy(const service_proxy& other) = default;

        rpc::shared_ptr<rpc::service_proxy> clone() override;

        static rpc::shared_ptr<service_proxy> create(const char* name, destination_zone destination_zone_id,
                                                     const rpc::shared_ptr<service>& svc,
                                                     std::chrono::milliseconds timeout, queue_type* send_spsc_queue,
                                 queue_type* receive_spsc_queue);

        static CORO_TASK(rpc::shared_ptr<service_proxy>)
            attach_remote(const char* name, const rpc::shared_ptr<service>& svc, destination_zone destination_zone_id,
                          std::shared_ptr<rpc::spsc::worker_release> connection, queue_type* send_spsc_queue,
                                 queue_type* receive_spsc_queue);

        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override;

        CORO_TASK(int)
        send(uint64_t protocol_version, encoding encoding, uint64_t tag, caller_channel_zone caller_channel_zone_id,
             caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id,
             interface_ordinal interface_id, method method_id, size_t in_size_, const char* in_buf_,
             std::vector<char>& out_buf_) override;
        CORO_TASK(int)
        try_cast(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                 interface_ordinal interface_id) override;
        CORO_TASK(uint64_t)
        add_ref(uint64_t protocol_version, destination_channel_zone destination_channel_zone_id,
                destination_zone destination_zone_id, object object_id, caller_channel_zone caller_channel_zone_id,
                caller_zone caller_zone_id, rpc::add_ref_options build_out_param_channel) override;
        CORO_TASK(uint64_t)
        release(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                caller_zone caller_zone_id) override;

        friend rpc::service;

        std::shared_ptr<rpc::spsc::worker_release> connection_;
        std::chrono::milliseconds timeout_;
        queue_type* send_spsc_queue_;
        queue_type* receive_spsc_queue_;

    public:
        virtual ~service_proxy();
    };
}