/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifndef _IN_ENCLAVE
#include <thread>
#include <span>

#include <random>

#include "common/tcp/service_proxy.h"

namespace rpc::tcp
{
    service_proxy::service_proxy(const char* name,
        destination_zone destination_zone_id,
        const std::shared_ptr<rpc::service>& svc,
        std::shared_ptr<worker_release> connection,
        std::chrono::milliseconds timeout,
        coro::net::tcp::client::options opts)
        : rpc::service_proxy(name, destination_zone_id, svc)
        , connection_(connection)
        , timeout_(timeout)
        , opts_(opts)
    {
    }

    service_proxy::~service_proxy()
    {
        connection_.reset();
    }

    std::shared_ptr<rpc::service_proxy> service_proxy::clone()
    {
        return std::shared_ptr<rpc::service_proxy>(new service_proxy(*this));
    }

    std::shared_ptr<rpc::service_proxy> service_proxy::create(const char* name,
        destination_zone destination_zone_id,
        const std::shared_ptr<rpc::service>& svc,
        std::chrono::milliseconds timeout,
        coro::net::tcp::client::options opts)
    {
        RPC_ASSERT(svc);

        auto ret = std::shared_ptr<rpc::service_proxy>(
            new service_proxy(name, destination_zone_id, svc, nullptr, timeout, opts));
        return ret;
    }

    CORO_TASK(std::shared_ptr<rpc::service_proxy>)
    service_proxy::attach_remote(const char* name,
        const std::shared_ptr<rpc::service>& svc,
        destination_zone destination_zone_id,
        std::shared_ptr<worker_release> connection)
    {
        RPC_ASSERT(svc);

        // RPC_DEBUG("attach_remote this service {} to {}", svc->get_zone_id(), destination_zone_id.get_val());

        auto ret = std::shared_ptr<rpc::service_proxy>(new service_proxy(
            name, destination_zone_id, svc, connection, std::chrono::milliseconds(0), coro::net::tcp::client::options()));

        if (!svc->get_scheduler()->schedule(connection->channel_manager_->pump_send_and_receive()))
        {
            RPC_ERROR("unable to attach_remote->pump_send_and_receive");
            CO_RETURN nullptr;
        }

        CO_RETURN ret;
    }

    CORO_TASK(int)
    service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {
        // RPC_DEBUG("connect {}", get_zone_id());

        auto service = get_operating_zone_service();
        assert(connection_ == nullptr);

        //  Immediately schedule onto the scheduler.
        CO_AWAIT service->get_scheduler()->schedule();

        // create the proxy channel
        auto connection = std::make_shared<worker_release>();

        {
            // Connect to the server to create a proxy port.
            coro::net::tcp::client proxy_client(service->get_scheduler(), opts_);
            auto connection_status = co_await proxy_client.connect();
            if (connection_status != coro::net::connect_status::connected)
            {
                RPC_ERROR("unable to Connect to the server to create a proxy port");
                CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
            }

            connection->channel_manager_ = std::make_shared<channel_manager>(
                std::move(proxy_client), timeout_, connection, get_operating_zone_service());
        }

        if (!service->get_scheduler()->schedule(connection->channel_manager_->pump_send_and_receive()))
        {
            RPC_ERROR("unable to pump_send_and_receive proxy");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        connection_ = connection;

        {
            // register the proxy connection
            tcp::init_client_channel_response init_receive;
            int ret = CO_AWAIT connection->channel_manager_->call_peer(rpc::get_version(),
                tcp::init_client_channel_send{.caller_zone_id = get_zone_id().get_val(),
                    .caller_object_id = input_descr.object_id.get_val(),
                    .destination_zone_id = get_destination_zone_id().get_val()},
                init_receive);
            if (ret != rpc::error::OK())
            {
                RPC_ERROR("channel_manager->call_peer");
                CO_RETURN ret;
            }

            if (init_receive.err_code != rpc::error::OK())
            {
                RPC_ERROR("init_client_channel_send failed");
                CO_RETURN init_receive.err_code;
            }

            rpc::object output_object_id = {init_receive.destination_object_id};
            output_descr = {output_object_id, get_destination_zone_id()};
        }

        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int)
    service_proxy::send(uint64_t protocol_version,
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
        std::vector<char>& out_buf_)
    {
        // RPC_DEBUG("send {}", get_zone_id());

        if (destination_zone_id != get_destination_zone_id())
        {
            RPC_ERROR("failed service_proxy::send ZONE_NOT_SUPPORTED");
            CO_RETURN rpc::error::ZONE_NOT_SUPPORTED();
        }

        if (!connection_)
        {
            RPC_ERROR("failed service_proxy::send SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        tcp::call_receive call_receive;
        int ret = CO_AWAIT connection_->channel_manager_->call_peer(protocol_version,
            tcp::call_send{.encoding = encoding,
                .tag = tag,
                .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .interface_id = interface_id.get_val(),
                .method_id = method_id.get_val(),
                .payload = std::vector<char>(in_buf_, in_buf_ + in_size_)},
            call_receive);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed service_proxy::send call_send");
            CO_RETURN ret;
        }

        out_buf_.swap(call_receive.payload);

        // RPC_DEBUG("send complete {}", get_zone_id());

        CO_RETURN call_receive.err_code;
    }

    CORO_TASK(int)
    service_proxy::try_cast(
        uint64_t protocol_version, destination_zone destination_zone_id, object object_id, interface_ordinal interface_id)
    {
        // RPC_DEBUG("try_cast {}", get_zone_id());

        if (!connection_)
        {
            RPC_ERROR("failed try_cast SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        tcp::try_cast_receive try_cast_receive;
        int ret = CO_AWAIT connection_->channel_manager_->call_peer(protocol_version,
            tcp::try_cast_send{
                .destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .interface_id = interface_id.get_val(),
            },
            try_cast_receive);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed try_cast call_peer");
            CO_RETURN ret;
        }

        // RPC_DEBUG("try_cast complete {}", get_zone_id());

        CO_RETURN try_cast_receive.err_code;
    }

    CORO_TASK(int)
    service_proxy::add_ref(uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,
        rpc::add_ref_options build_out_param_channel,
        uint64_t& reference_count)
    {
        // RPC_DEBUG("add_ref {}", get_zone_id());

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(),
                destination_zone_id,
                destination_channel_zone_id,
                get_caller_zone_id(),
                object_id,
                build_out_param_channel);
        }
#endif
        if (!connection_)
        {
            RPC_ERROR("failed add_ref SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        tcp::addref_receive response;
        int ret = CO_AWAIT connection_->channel_manager_->call_peer(protocol_version,
            tcp::addref_send{.destination_channel_zone_id = destination_channel_zone_id.get_val(),
                .destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .known_direction_zone_id = known_direction_zone_id.get_val(),
                .build_out_param_channel = (tcp::add_ref_options)build_out_param_channel},
            response);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed add_ref addref_send");
            CO_RETURN ret;
        }

        reference_count = response.ref_count;
        if (response.err_code != rpc::error::OK())
        {
            RPC_ERROR("failed response.err_code failed");
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = fmt::format("add_ref failed {}", response.err_code);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ASSERT(false);
            CO_RETURN response.err_code;
        }
        // RPC_DEBUG("add_ref complete {}", get_zone_id());

        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int)
    service_proxy::release(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        rpc::release_options options,
        uint64_t& reference_count)
    {
        RPC_ERROR("release zone: {}", get_zone_id().get_val());

        if (!connection_)
        {
            RPC_ERROR("failed release SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        tcp::release_receive response;
        int ret = CO_AWAIT connection_->channel_manager_->call_peer(protocol_version,
            tcp::release_send{
                .destination_zone_id = destination_zone_id.get_val(),
                .object_id = object_id.get_val(),
                .caller_zone_id = caller_zone_id.get_val(),
                .options = static_cast<tcp::release_options>(options),
            },
            response);
        if (ret != rpc::error::OK())
        {
            RPC_ERROR("failed release release_send");
            CO_RETURN ret;
        }

        if (response.err_code != rpc::error::OK())
        {
            RPC_ERROR("failed response.err_code failed");
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service)
            {
                auto error_message = fmt::format("release failed {}", response.err_code);
                telemetry_service->message(rpc::i_telemetry_service::err, error_message.c_str());
            }
#endif
            RPC_ASSERT(false);
            CO_RETURN response.err_code;
        }

        RPC_ERROR("release complete zone: {}", get_zone_id().get_val());

        reference_count = response.ref_count;
        CO_RETURN rpc::error::OK();
    }
}
#endif
