/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <mutex>
#include <unordered_map>

#include <rpc/proxy.h>
#include <rpc/service.h>
#include <coro/coro.hpp>
#include <common/tcp_service_proxy.h>
#include <common/tcp_channel_manager.h>

#include <tcp/tcp.h>

namespace rpc
{
    class tcp_listener
    {
        struct initialising_client
        {
            tcp::init_client_channel_send payload;
            std::shared_ptr<tcp_channel_manager> channel_manager;
            uint64_t rpc_version;
        };
        std::mutex mtx;
        std::unordered_map<uint64_t, initialising_client> initialising_clients;

    public:
        // This is to open a listening socket on for incoming tcp connection requests
        bool start_listening(rpc::shared_ptr<rpc::service> service);

    private:
        coro::task<void> run_client(rpc::shared_ptr<rpc::service> service, coro::net::tcp::client&& client);
        coro::task<void> run_listener(rpc::shared_ptr<rpc::service> service);
    };
}
