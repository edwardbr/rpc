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
#include <common/tcp/service_proxy.h>
#include <common/tcp/channel_manager.h>

#include <tcp/tcp.h>

namespace rpc::tcp
{
    template<class CallerInterface, class CalleeInterface> class listener
    {
        coro::event stop_confirmation_evt_;
        bool stop_ = false;
        std::chrono::milliseconds timeout_;
        std::chrono::milliseconds poll_timeout_ = std::chrono::milliseconds(10);

        using connection_handler = std::function<CORO_TASK(int)(const rpc::shared_ptr<CallerInterface>& caller_interface,
            rpc::shared_ptr<CalleeInterface>& callee_interface,
            const rpc::shared_ptr<rpc::service>& child_service_ptr)>;
        connection_handler connection_handler_;

    public:
        listener(connection_handler handler, std::chrono::milliseconds timeout)
            : timeout_(timeout)
            , connection_handler_(handler)
        {
        }

        listener(const listener&) = delete;

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

            worker_release->channel_manager_
                = std::make_shared<channel_manager>(std::move(client), timeout_, worker_release, service);

            envelope_prefix prefix;
            envelope_payload payload;
            int err = CO_AWAIT worker_release->channel_manager_->receive_anonymous_payload(prefix, payload, 0);
            if (err != rpc::error::OK())
            {
                RPC_ERROR("failed run_client receive_anonymous_payload");
                CO_RETURN;
            }

            if (payload.payload_fingerprint == rpc::id<tcp::init_client_channel_send>::get(prefix.version))
            {
                // std::string msg("run_client init_client_channel_send ");
                // msg += std::to_string(service->get_zone_id().get_val());
                // LOG_CSTR(msg.c_str());

                tcp::init_client_channel_send request;
                auto err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if (!err.empty())
                {
                    RPC_ERROR("failed run_client init_client_channel_send");
                    CO_RETURN;
                }

                {
                    destination_zone destination_zone_id{request.caller_zone_id};

                    rpc::interface_descriptor output_interface;

                    auto ret
                        = CO_AWAIT service->attach_remote_zone<rpc::tcp::service_proxy, CallerInterface, CalleeInterface>(
                            "service_proxy",
                            {{request.caller_object_id}, {request.caller_zone_id}},
                            output_interface,
                            connection_handler_,
                            destination_zone_id,
                            worker_release);
                    if (ret != rpc::error::OK())
                    {
                        // report error  
                        RPC_ERROR("failed to connect to zone {}", ret);
                        CO_RETURN;
                    }

                    // msg = "run_client init_server_channel_response ";
                    // msg += std::to_string(service->get_zone_id().get_val());
                    // LOG_CSTR(msg.c_str());

                    err = CO_AWAIT worker_release->channel_manager_->immediate_send_payload(prefix.version,
                        message_direction::receive,
                        tcp::init_client_channel_response{rpc::error::OK(),
                            output_interface.destination_zone_id.get_val(),
                            output_interface.object_id.get_val(),
                            0},
                        prefix.sequence_number);
                    std::ignore = err;

                    CO_RETURN;
                }
            }
            else
            {
                // dodgy request
                RPC_ERROR("invalid fingerprint {}", payload.payload_fingerprint);
                CO_RETURN;
            }
        }

        coro::task<void> run_listener(rpc::shared_ptr<rpc::service> service)
        {
            // Start by creating a tcp server, we'll do this before putting it into the scheduler so
            // it is immediately available for the client to connect since this will create a socket,
            // bind the socket and start listening on that socket.  See tcp::server for more details on
            // how to specify the local address and port to bind to as well as enabling SSL/TLS.
            coro::net::tcp::server server{service->get_scheduler()};

            auto scheduler = service->get_scheduler();
            co_await scheduler->schedule();

            while (!stop_)
            {
                // Wait for an incoming connection and accept it.
                auto poll_status = co_await server.poll(poll_timeout_);
                if (poll_status == coro::poll_status::timeout)
                {
                    continue; // Handle error, see poll_status for detailed error states.
                }
                if (poll_status != coro::poll_status::event)
                {
                    RPC_ERROR("failed run_listener poll_status");
                    break; // Handle error, see poll_status for detailed error states.
                }

                // Accept the incoming client connection.
                auto client = server.accept();

                // Verify the incoming connection was accepted correctly.
                if (!client.socket().is_valid())
                {
                    RPC_ERROR("failed run_listener client is_valid");
                    break; // Handle error.
                }

                service->schedule(run_client(service, std::move(client)));
            };
            stop_confirmation_evt_.set();
            CO_RETURN;
        }
    };
}
