/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <mutex>
#include <chrono>
#include <unordered_map>

#include <rpc/proxy.h>
#include <rpc/service.h>
#include <coro/coro.hpp>
#include <common/tcp_service_proxy.h>
#include <common/tcp_channel_manager.h>

#include <tcp/tcp.h>

namespace rpc
{
    template<class CallerInterface, class CalleeInterface> 
    class tcp_listener
    {
        struct initialising_client
        {
            tcp::init_client_channel_send payload;
            std::shared_ptr<worker_release> worker_release;
            uint64_t rpc_version;
        };
        std::mutex mtx;
        std::unordered_map<uint64_t, initialising_client> initialising_clients;

        coro::event stop_confirmation_evt_;
        bool stop_ = false;
        std::chrono::milliseconds timeout_;
        std::chrono::milliseconds delayed_connection_timeout_ = std::chrono::milliseconds(100000);
        std::chrono::milliseconds poll_timeout_ = std::chrono::milliseconds(10);

        using connection_handler
            = std::function<CORO_TASK(int)(const rpc::shared_ptr<CallerInterface>& caller_interface,
                                           rpc::shared_ptr<CalleeInterface>& callee_interface,
                                           const rpc::shared_ptr<rpc::service>& child_service_ptr)>;
        connection_handler connection_handler_;

    public:
        tcp_listener(connection_handler handler, std::chrono::milliseconds timeout)
            : timeout_(timeout)
            , connection_handler_(handler)
        {
        }
        
        tcp_listener(const tcp_listener&) = delete;

        // This is to open a listening socket on for incoming tcp connection requests
        bool start_listening(rpc::shared_ptr<rpc::service> service) { return service->schedule(run_listener(service)); }

        coro::task<void> stop_listening()
        {
            stop_ = true;
            CO_AWAIT stop_confirmation_evt_;
        }

    private:
        coro::task<void> run_client(rpc::shared_ptr<rpc::service> service, coro::net::tcp::client client)
        {
            assert(client.socket().is_valid());
            std::shared_ptr<worker_release> worker_release(std::make_shared<worker_release>());
            worker_release->channel_manager
                = std::make_shared<tcp_channel_manager>(std::move(client), timeout_, worker_release, service);

            tcp::envelope_prefix prefix;
            tcp::envelope_payload payload;
            int err = CO_AWAIT worker_release->channel_manager->receive_anonymous_payload(prefix, payload, 0);
            if(err != rpc::error::OK())
                CO_RETURN;

            if(payload.payload_fingerprint == rpc::id<tcp::init_client_channel_send>::get(prefix.version))
            {
                tcp::init_client_channel_send request;
                auto err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if(!err.empty())
                {
                    // bad format
                    CO_RETURN;
                }

                auto random_number = request.random_number_id;

                {
                    std::scoped_lock lock(mtx);
                    auto [_, inserted] = initialising_clients.try_emplace(
                        request.random_number_id, tcp_listener::initialising_client {.payload = std::move(request),
                                                                                     .worker_release = worker_release,
                                                                                     .rpc_version = prefix.version});
                    assert(inserted);
                }

                err = CO_AWAIT worker_release->channel_manager->immediate_send_payload(
                    prefix.version, tcp::init_client_channel_response {rpc::error::OK()}, prefix.sequence_number);
                std::ignore = err;

                CO_AWAIT service->get_scheduler()->schedule_after(delayed_connection_timeout_);

                {
                    // this is to clean up if the client has failed to send a second connection
                    std::scoped_lock lock(mtx);
                    initialising_clients.erase(random_number);
                }
                CO_RETURN;
            }
            else if(payload.payload_fingerprint == rpc::id<tcp::init_server_channel_send>::get(prefix.version))
            {
                tcp::init_server_channel_send request;
                auto err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if(!err.empty())
                {
                    // bad format
                    CO_RETURN;
                }

                auto random_number = request.random_number_id;

                {
                    auto lock = std::make_unique<std::scoped_lock<std::mutex>>(mtx);
                    auto found = initialising_clients.find(random_number);
                    if(found == initialising_clients.end())
                    {
                        // no initialising client found
                        auto randonmumber
                            = "random number " + std::to_string(request.random_number_id) + " not found\n";
                        LOG_STR(randonmumber.c_str(), randonmumber.size());
                        CO_RETURN;
                    }

                    // now we have all the info to make a tcp service proxy

                    auto initialisation_info = std::move(found->second);
                    initialising_clients.erase(found);
                    destination_zone destination_zone_id {initialisation_info.payload.caller_zone_id};

                    lock.reset();

                    rpc::interface_descriptor output_interface;

                    auto ret
                        = CO_AWAIT service->attach_remote_zone<rpc::tcp_service_proxy, CallerInterface, CalleeInterface>(
                            "tcp_service_proxy",
                            {{initialisation_info.payload.caller_object_id},
                             {initialisation_info.payload.caller_zone_id}},
                            output_interface, connection_handler_, destination_zone_id,
                            initialisation_info.worker_release, worker_release);
                    if(ret != rpc::error::OK())
                    {
                        // report error
                        auto randonmumber = "failed to connect to zone " + std::to_string(ret) + " \n";
                        LOG_STR(randonmumber.c_str(), randonmumber.size());
                        CO_RETURN;
                    }

                    err = CO_AWAIT worker_release->channel_manager->immediate_send_payload(
                        prefix.version,
                        tcp::init_server_channel_response {rpc::error::OK(),
                                                           output_interface.destination_zone_id.get_val(),
                                                           output_interface.object_id.get_val(), 0},
                        prefix.sequence_number);
                    std::ignore = err;

                    CO_RETURN;
                }
            }
            else
            {
                // dodgy request
                auto randonmumber = "invalid fingerprint " + std::to_string(payload.payload_fingerprint) + " \n";
                LOG_STR(randonmumber.c_str(), randonmumber.size());
                CO_RETURN;
            }
        }
        coro::task<void> run_listener(rpc::shared_ptr<rpc::service> service)
        {
            // Start by creating a tcp server, we'll do this before putting it into the scheduler so
            // it is immediately available for the client to connect since this will create a socket,
            // bind the socket and start listening on that socket.  See tcp::server for more details on
            // how to specify the local address and port to bind to as well as enabling SSL/TLS.
            coro::net::tcp::server server {service->get_scheduler()};

            auto scheduler = service->get_scheduler();
            co_await scheduler->schedule();

            while(!stop_)
            {
                // Wait for an incoming connection and accept it.
                auto poll_status = co_await server.poll(poll_timeout_);
                if(poll_status == coro::poll_status::timeout)
                {
                    continue; // Handle error, see poll_status for detailed error states.
                }
                if(poll_status != coro::poll_status::event)
                {
                    break; // Handle error, see poll_status for detailed error states.
                }

                // Accept the incoming client connection.
                auto client = server.accept();

                // Verify the incoming connection was accepted correctly.
                if(!client.socket().is_valid())
                {
                    break; // Handle error.
                }

                service->schedule(run_client(service, std::move(client)));
            };
            stop_confirmation_evt_.set();
            CO_RETURN;
        }
    };
}
