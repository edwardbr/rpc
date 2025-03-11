/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <memory>
#include <common/tcp_listener.h>
#include <coro/coro.hpp>
#include <common/tcp_service_proxy.h>
#include "common/tcp_channel_manager.h"

#include <example/example.h>
#include <common/foo_impl.h>

using namespace std::chrono_literals;

namespace rpc
{
    coro::task<void> tcp_listener::run_client(rpc::shared_ptr<rpc::service> service, coro::net::tcp::client client)
    {
        assert(client.socket().is_valid());
        std::shared_ptr<worker_release> worker_release(std::make_shared<worker_release>());
        worker_release->channel_manager = std::make_shared<tcp_channel_manager>(std::move(client), timeout_, worker_release, service);

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
                initialising_clients.try_emplace(request.random_number_id,
                                                 tcp_listener::initialising_client {.payload = std::move(request),
                                                                                    .worker_release = worker_release,
                                                                                    .rpc_version = prefix.version});
            }
            
            err = CO_AWAIT worker_release->channel_manager->immediate_send_payload(prefix.version, tcp::init_client_channel_response{rpc::error::OK()}, prefix.sequence_number);
            std::ignore = err;

            CO_AWAIT service->get_scheduler()->schedule_after(100000ms);

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
                    CO_RETURN;
                }

                // now we have all the info to make a tcp service proxy

                auto initialisation_info = std::move(found->second);
                initialising_clients.erase(found);
                destination_zone destination_zone_id{0};

                lock.reset();

                rpc::interface_descriptor output_interface;

                auto ret = CO_AWAIT service->attach_remote_zone<rpc::tcp_service_proxy, yyy::i_host, yyy::i_example>(
                    "tcp_service_proxy",
                    {{initialisation_info.payload.caller_object_id}, {initialisation_info.payload.caller_zone_id}},
                    output_interface,
                    [](const rpc::shared_ptr<yyy::i_host>& host, rpc::shared_ptr<yyy::i_example>& new_example,
                       const rpc::shared_ptr<rpc::service>& child_service_ptr) -> CORO_TASK(int)
                    {
                        // example_import_idl_register_stubs(child_service_ptr);
                        // example_shared_idl_register_stubs(child_service_ptr);
                        // example_idl_register_stubs(child_service_ptr);
                        new_example
                            = rpc::shared_ptr<yyy::i_example>(new marshalled_tests::example(child_service_ptr, host));
                        CO_RETURN rpc::error::OK();
                    },
                    destination_zone_id,
                    initialisation_info.worker_release, worker_release);
                if(ret != rpc::error::OK())
                {
                    // report error
                }

                err = CO_AWAIT worker_release->channel_manager->immediate_send_payload(prefix.version, tcp::init_server_channel_response{rpc::error::OK(), output_interface.destination_zone_id.get_val(), output_interface.object_id.get_val(), 0}, prefix.sequence_number);
                std::ignore = err;

                CO_RETURN;
            }
        }
        else
        {
            // dodgy request
            CO_RETURN;
        }
    }

    coro::task<void> tcp_listener::run_listener(rpc::shared_ptr<rpc::service> service)
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
            auto poll_status = co_await server.poll(std::chrono::milliseconds{10});
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

    // This is to open a listening socket on for incoming tcp connection requests
    bool tcp_listener::start_listening(rpc::shared_ptr<rpc::service> service)
    {
        return service->schedule(run_listener(service));
    }

    // This is to open a listening socket on for incoming tcp connection requests
    coro::task<void> tcp_listener::stop_listening()
    {
        stop_ = true;
        CO_AWAIT stop_confirmation_evt_;
    }
}
