/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifndef _IN_ENCLAVE
#include <thread>
#include <span>

#include "common/tcp_service_proxy.h"

namespace rpc
{
    tcp_service_proxy::tcp_service_proxy(const char* name, destination_zone destination_zone_id,
                                         const rpc::shared_ptr<service>& svc)
        : service_proxy(name, destination_zone_id, svc)
    {
    }

    tcp_service_proxy::tcp_service_proxy(const char* name, destination_zone destination_zone_id,
                                         const rpc::shared_ptr<service>& svc,
                                         const std::shared_ptr<tcp_channel_manager>& channel_manager)
        : service_proxy(name, destination_zone_id, svc)
        , channel_manager_(channel_manager)
    {
    }

    rpc::shared_ptr<service_proxy> tcp_service_proxy::clone()
    {
        return rpc::shared_ptr<service_proxy>(new tcp_service_proxy(*this));
    }

    rpc::shared_ptr<tcp_service_proxy> tcp_service_proxy::create(const char* name, destination_zone destination_zone_id,
                                                                 const rpc::shared_ptr<service>& svc)
    {
        RPC_ASSERT(svc);
        auto ret = rpc::shared_ptr<tcp_service_proxy>(new tcp_service_proxy(name, destination_zone_id, svc));
        return ret;
    }

    CORO_TASK(rpc::shared_ptr<tcp_service_proxy>)
    tcp_service_proxy::create(const char* name, const rpc::shared_ptr<service>& svc,
                              const std::shared_ptr<tcp_channel_manager>& initiator_channel_manager,
                              const std::shared_ptr<tcp_channel_manager>& responding_channel_manager)
    {
        RPC_ASSERT(svc);
        destination_zone destination_zone_id {0};
        auto ret = rpc::shared_ptr<tcp_service_proxy>(
            new tcp_service_proxy(name, destination_zone_id, svc, responding_channel_manager));

        if(!svc->schedule(ret->init_stub(initiator_channel_manager)))
        {
            co_return nullptr;
        }

        co_return ret;
    }

    CORO_TASK(int)
    tcp_service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {
        auto service = get_operating_zone_service();
        assert(channel_manager_ == nullptr);

        // auto noop_task = []() ->coro::task<void>{CO_RETURN;};
        //  Immediately schedule onto the scheduler.
        CO_AWAIT service->get_scheduler()->schedule();

        auto random_number_id = (uint64_t)std::rand();

        // create the client channel

        std::shared_ptr<tcp_channel_manager> channel_manager;

        {
            // Create the tcp::client with the default settings, see tcp::client for how to set the
            // ip address, port, and optionally enabling SSL/TLS.

            // Ommitting error checking code for the client, each step should check the status and
            // verify the number of bytes sent or received.

            // Connect to the server.
            coro::net::tcp::client init_client(service->get_scheduler());
            auto connection_status = co_await init_client.connect();
            if(connection_status != coro::net::connect_status::connected)
            {
                CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
            }

            channel_manager
                = std::make_shared<tcp_channel_manager>(std::move(init_client), std::chrono::milliseconds(10000));
            service->get_scheduler()->schedule(channel_manager->launch_send_receive());

            tcp::init_client_channel_response init_receive;
            int ret = CO_AWAIT channel_manager->call_peer(
                rpc::get_version(),
                tcp::init_client_channel_send {.random_number_id = random_number_id,
                                               .caller_zone_id = get_zone_id().get_val(),
                                               .caller_object_id = input_descr.object_id.get_val(),
                                               .destination_zone_id = get_destination_zone_id().get_val()},
                init_receive);
            if(ret != rpc::error::OK())
                CO_RETURN ret;

            rpc::object output_object_id = {init_receive.destination_object_id};
            output_descr = {output_object_id, get_destination_zone_id()};

            if(init_receive.err_code != rpc::error::OK())
            {
                CO_RETURN init_receive.err_code;
            }
        }

        // create the server channel
        {
            std::shared_ptr<tcp_channel_manager> receive_channel_manager;
            {
                // Create the tcp::client with the default settings, see tcp::client for how to set the
                // ip address, port, and optionally enabling SSL/TLS.
                coro::net::tcp::client receive_client(service->get_scheduler());

                // Ommitting error checking code for the client, each step should check the status and
                // verify the number of bytes sent or received.

                // Connect to the server.
                auto connection_status = co_await receive_client.connect();
                if(connection_status != coro::net::connect_status::connected)
                {
                    CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
                }

                receive_channel_manager = std::make_shared<tcp_channel_manager>(std::move(receive_client),
                                                                                std::chrono::milliseconds(10000));
                service->get_scheduler()->schedule(receive_channel_manager->launch_send_receive());
            }

            tcp::init_server_channel_response init_receive;
            int ret = CO_AWAIT receive_channel_manager->call_peer(
                rpc::get_version(), tcp::init_server_channel_send {.random_number_id = random_number_id}, init_receive);
            if(ret != rpc::error::OK())
                CO_RETURN ret;

            if(init_receive.err_code != rpc::error::OK())
            {
                CO_RETURN init_receive.err_code;
            }

            if(!service->schedule(init_stub(std::move(receive_channel_manager))))
            {
                CO_RETURN rpc::error::UNABLE_TO_CREATE_SERVICE_PROXY();
            }

            channel_manager_ = channel_manager;
        }

        CO_RETURN rpc::error::OK();
    }

    coro::task<void> tcp_service_proxy::init_stub(const std::shared_ptr<tcp_channel_manager>& channel_manager)
    {
        do
        {
            tcp::envelope_prefix prefix;
            tcp::envelope_payload payload;
            int err = CO_AWAIT channel_manager->receive_anonymous_payload(prefix, payload, 0);
            if(err != rpc::error::OK())
                CO_RETURN;

            // do a call
            if(payload.payload_fingerprint == rpc::id<tcp::call_send>::get(prefix.version))
            {
                tcp::call_send request;
                auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if(!str_err.empty())
                {
                    // bad format
                    CO_RETURN;
                }

                auto service = get_operating_zone_service();
                std::vector<char> out_buf;
                auto ret = co_await service->send(prefix.version, request.encoding, request.tag,
                                                  {request.caller_channel_zone_id}, {request.caller_zone_id},
                                                  {request.destination_zone_id}, {request.object_id},
                                                  {request.interface_id}, {request.method_id}, request.payload.size(),
                                                  request.payload.data(), out_buf);

                auto err = CO_AWAIT channel_manager->send_payload(
                    prefix.version, tcp::call_receive {.payload = std::move(out_buf), .err_code = ret},
                    prefix.sequence_number);
                if(err != rpc::error::OK())
                {
                    // report error
                    CO_RETURN;
                }
            }
            // do a try cast
            else if(payload.payload_fingerprint == rpc::id<tcp::try_cast_send>::get(prefix.version))
            {
                tcp::try_cast_send request;
                auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if(!str_err.empty())
                {
                    // bad format
                    CO_RETURN;
                }

                auto service = get_operating_zone_service();
                std::vector<char> out_buf;
                auto ret = co_await service->try_cast(prefix.version, {request.destination_zone_id},
                                                      {request.object_id}, {request.interface_id});

                auto err = CO_AWAIT channel_manager->send_payload(
                    prefix.version, tcp::try_cast_receive {.err_code = ret}, prefix.sequence_number);
                if(err != rpc::error::OK())
                {
                    // report error
                    CO_RETURN;
                }
            }
            // do an add_ref
            else if(payload.payload_fingerprint == rpc::id<tcp::addref_send>::get(prefix.version))
            {
                tcp::addref_send request;
                auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if(!str_err.empty())
                {
                    // bad format
                    CO_RETURN;
                }

                auto service = get_operating_zone_service();
                std::vector<char> out_buf;
                auto ret = co_await service->add_ref(prefix.version, {request.destination_channel_zone_id},
                                                     {request.destination_zone_id}, {request.object_id},
                                                     {request.caller_channel_zone_id}, {request.caller_zone_id},
                                                     (rpc::add_ref_options)request.build_out_param_channel);

                auto err = CO_AWAIT channel_manager->send_payload(
                    prefix.version, tcp::addref_receive {.ref_count = ret, .err_code = rpc::error::OK()},
                    prefix.sequence_number);
                if(err != rpc::error::OK())
                {
                    // report error
                    CO_RETURN;
                }
            }
            // do a release
            else if(payload.payload_fingerprint == rpc::id<tcp::release_send>::get(prefix.version))
            {
                tcp::release_send request;
                auto str_err = rpc::from_yas_compressed_binary(rpc::span(payload.payload), request);
                if(!str_err.empty())
                {
                    // bad format
                    CO_RETURN;
                }

                auto service = get_operating_zone_service();
                std::vector<char> out_buf;
                auto ret = co_await service->release(prefix.version, {request.destination_zone_id}, {request.object_id},
                                                     {request.caller_zone_id});

                auto err = CO_AWAIT channel_manager->send_payload(
                    prefix.version, tcp::release_receive {.ref_count = ret, .err_code = rpc::error::OK()},
                    prefix.sequence_number);
                if(err != rpc::error::OK())
                {
                    // report error
                    CO_RETURN;
                }
            }
            else
            {
                // bad message
                CO_RETURN;
            }
        } while(true);
    }

    CORO_TASK(int)
    tcp_service_proxy::send(uint64_t protocol_version, encoding encoding, uint64_t tag,
                            caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id,
                            destination_zone destination_zone_id, object object_id, interface_ordinal interface_id,
                            method method_id, size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(destination_zone_id != get_destination_zone_id())
            CO_RETURN rpc::error::ZONE_NOT_SUPPORTED();

        if(!channel_manager_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::call_receive call_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(
            protocol_version,
            tcp::call_send {.encoding = encoding,
                            .tag = tag,
                            .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                            .caller_zone_id = caller_zone_id.get_val(),
                            .destination_zone_id = destination_zone_id.get_val(),
                            .object_id = object_id.get_val(),
                            .interface_id = interface_id.get_val(),
                            .method_id = method_id.get_val(),
                            .payload = std::vector<char>(in_buf_, in_buf_ + in_size_)},
            call_receive);
        if(ret != rpc::error::OK())
            CO_RETURN ret;

        out_buf_.swap(call_receive.payload);
        CO_RETURN call_receive.err_code;
    }

    CORO_TASK(int)
    tcp_service_proxy::try_cast(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                                interface_ordinal interface_id)
    {
        if(!channel_manager_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::try_cast_receive try_cast_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(protocol_version,
                                                       tcp::try_cast_send {
                                                           .destination_zone_id = destination_zone_id.get_val(),
                                                           .object_id = object_id.get_val(),
                                                           .interface_id = interface_id.get_val(),
                                                       },
                                                       try_cast_receive);
        if(ret != rpc::error::OK())
            CO_RETURN ret;

        CO_RETURN try_cast_receive.err_code;
    }

    CORO_TASK(uint64_t)
    tcp_service_proxy::add_ref(uint64_t protocol_version, destination_channel_zone destination_channel_zone_id,
                               destination_zone destination_zone_id, object object_id,
                               caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id,
                               add_ref_options build_out_param_channel)
    {
#ifdef USE_RPC_TELEMETRY
        if(auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(), destination_zone_id, destination_channel_zone_id,
                                                        get_caller_zone_id(), object_id, build_out_param_channel);
        }
#endif
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();

        if(!channel_manager_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::addref_receive addref_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(
            protocol_version,
            tcp::addref_send {.destination_channel_zone_id = destination_channel_zone_id.get_val(),
                              .destination_zone_id = destination_zone_id.get_val(),
                              .object_id = object_id.get_val(),
                              .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                              .caller_zone_id = caller_zone_id.get_val(),
                              .build_out_param_channel = (tcp::add_ref_options)build_out_param_channel},
            addref_receive);
        if(ret != rpc::error::OK())
            CO_RETURN ret;

        if(addref_receive.err_code != rpc::error::OK())
        {
#ifdef USE_RPC_TELEMETRY
            if(auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                auto error_message = std::string("add_ref failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ASSERT(false);
            CO_RETURN add_ref_failed_val;
        }
        CO_RETURN addref_receive.ref_count;
    }

    CORO_TASK(uint64_t)
    tcp_service_proxy::release(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                               caller_zone caller_zone_id)
    {
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();

        if(!channel_manager_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::addref_receive release_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(protocol_version,
                                                       tcp::release_send {
                                                           .destination_zone_id = destination_zone_id.get_val(),
                                                           .object_id = object_id.get_val(),
                                                           .caller_zone_id = caller_zone_id.get_val(),
                                                       },
                                                       release_receive);
        if(ret != rpc::error::OK())
            CO_RETURN ret;

        if(release_receive.err_code != rpc::error::OK())
        {
#ifdef USE_RPC_TELEMETRY
            if(auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
            {
                auto error_message = std::string("release failed ") + std::to_string(status);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ASSERT(false);
            CO_RETURN add_ref_failed_val;
        }
        CO_RETURN release_receive.ref_count;
    }
}
#endif
