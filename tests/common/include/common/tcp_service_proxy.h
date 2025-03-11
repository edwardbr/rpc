/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/proxy.h>
#include <coro/coro.hpp>
#include <coro/net/ip_address.hpp>

#include <tcp/tcp.h>

#include "common/tcp_channel_manager.h"

namespace rpc
{
    // This is for hosts to call services on an enclave
    class tcp_service_proxy : public service_proxy
    {
        tcp_service_proxy(const char* name, destination_zone destination_zone_id, const rpc::shared_ptr<service>& svc,
                          std::shared_ptr<worker_release> proxy_worker_release,
                          std::shared_ptr<worker_release> stub_worker_release, 
                          std::chrono::milliseconds timeout,
                          coro::net::tcp::client::options opts);

        tcp_service_proxy(const tcp_service_proxy& other) = default;

        rpc::shared_ptr<service_proxy> clone() override;

        static rpc::shared_ptr<tcp_service_proxy> create(const char* name, destination_zone destination_zone_id,
                                                         const rpc::shared_ptr<service>& svc, std::chrono::milliseconds timeout, coro::net::tcp::client::options opts);

        static CORO_TASK(rpc::shared_ptr<tcp_service_proxy>)
            attach_remote(const char* name, const rpc::shared_ptr<service>& svc, destination_zone destination_zone_id,
                          std::shared_ptr<worker_release> proxy_worker_release,
                          std::shared_ptr<worker_release> stub_worker_release);

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
                caller_zone caller_zone_id, add_ref_options build_out_param_channel) override;
        CORO_TASK(uint64_t)
        release(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                caller_zone caller_zone_id) override;

        friend rpc::service;

        std::shared_ptr<worker_release> proxy_worker_release_;
        std::shared_ptr<worker_release> stub_worker_release_;
        std::chrono::milliseconds timeout_;
        coro::net::tcp::client::options opts_;

    public:
        virtual ~tcp_service_proxy() = default;
    };
}