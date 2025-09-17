/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/rpc.h>
#include <coro/coro.hpp>
#include <coro/net/ip_address.hpp>

#include <tcp/tcp.h>

#include "common/tcp/channel_manager.h"

namespace rpc::tcp
{
    // This is for hosts to call services on an enclave
    class service_proxy : public rpc::service_proxy
    {
        service_proxy(const char* name, destination_zone destination_zone_id, const rpc::shared_ptr<service>& svc,
                      std::shared_ptr<worker_release> connection, std::chrono::milliseconds timeout,
                      coro::net::tcp::client::options opts);

        service_proxy(const service_proxy& other) = default;

        rpc::shared_ptr<rpc::service_proxy> clone() override;

        static rpc::shared_ptr<service_proxy> create(const char* name, destination_zone destination_zone_id,
                                                     const rpc::shared_ptr<service>& svc,
                                                     std::chrono::milliseconds timeout,
                                                     coro::net::tcp::client::options opts);

        static CORO_TASK(rpc::shared_ptr<service_proxy>)
            attach_remote(const char* name, const rpc::shared_ptr<service>& svc, destination_zone destination_zone_id,
                          std::shared_ptr<worker_release> connection);

        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override;

        CORO_TASK(int)
        send(uint64_t protocol_version, encoding encoding, uint64_t tag, caller_channel_zone caller_channel_zone_id,
             caller_zone caller_zone_id, destination_zone destination_zone_id, object object_id,
             interface_ordinal interface_id, method method_id, size_t in_size_, const char* in_buf_,
             std::vector<char>& out_buf_) override;
        CORO_TASK(int)
        try_cast(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                 interface_ordinal interface_id) override;
        CORO_TASK(int)
        add_ref(uint64_t protocol_version, destination_channel_zone destination_channel_zone_id,
                destination_zone destination_zone_id, object object_id, caller_channel_zone caller_channel_zone_id,
                caller_zone caller_zone_id, known_direction_zone known_direction_zone_id,
                rpc::add_ref_options build_out_param_channel, uint64_t& reference_count) override;
        CORO_TASK(int)
        release(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                caller_zone caller_zone_id, uint64_t& reference_count) override;

        friend rpc::service;

        std::shared_ptr<worker_release> connection_;
        std::chrono::milliseconds timeout_;
        coro::net::tcp::client::options opts_;

    public:
        virtual ~service_proxy();
    };
}