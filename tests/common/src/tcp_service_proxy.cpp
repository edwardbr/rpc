/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifndef _IN_ENCLAVE
#include <thread>
#include <span>

#include <random>

#include "common/tcp_service_proxy.h"

namespace rpc
{
    tcp_service_proxy::tcp_service_proxy(const char* name, destination_zone destination_zone_id,
                                         const rpc::shared_ptr<service>& svc,
                                         std::shared_ptr<worker_release> proxy_worker_release,
                                         std::shared_ptr<worker_release> stub_worker_release, 
                                         std::chrono::milliseconds timeout,
                                         coro::net::tcp::client::options opts)
        : service_proxy(name, destination_zone_id, svc)
        , proxy_worker_release_(proxy_worker_release)
        , stub_worker_release_(stub_worker_release)
        , timeout_(timeout)
        , opts_(opts)
    {
    }
    
    tcp_service_proxy::~tcp_service_proxy()
    {
        proxy_worker_release_.reset();
        stub_worker_release_.reset();
    }

    rpc::shared_ptr<service_proxy> tcp_service_proxy::clone()
    {
        return rpc::shared_ptr<service_proxy>(new tcp_service_proxy(*this));
    }

    rpc::shared_ptr<tcp_service_proxy> tcp_service_proxy::create(const char* name, destination_zone destination_zone_id,
                                                                 const rpc::shared_ptr<service>& svc, std::chrono::milliseconds timeout, coro::net::tcp::client::options opts)
    {
        RPC_ASSERT(svc);

        auto ret = rpc::shared_ptr<tcp_service_proxy>(
            new tcp_service_proxy(name, destination_zone_id, svc, nullptr, nullptr, timeout, opts));
        return ret;
    }

    CORO_TASK(rpc::shared_ptr<tcp_service_proxy>)
    tcp_service_proxy::attach_remote(const char* name, const rpc::shared_ptr<service>& svc,
                                     destination_zone destination_zone_id,
                                     std::shared_ptr<worker_release> proxy_worker_release,
                                     std::shared_ptr<worker_release> stub_worker_release)
    {
        RPC_ASSERT(svc);
        
        auto ret = rpc::shared_ptr<tcp_service_proxy>(
            new tcp_service_proxy(name, destination_zone_id, svc, proxy_worker_release, stub_worker_release, std::chrono::milliseconds(0), coro::net::tcp::client::options()));

        if(!svc->get_scheduler()->schedule(proxy_worker_release->channel_manager->pump_stub_receive_and_send()))
        {
            co_return nullptr;
        }

        co_return ret;
    }

    CORO_TASK(int)
    tcp_service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {
        auto service = get_operating_zone_service();
        assert(proxy_worker_release_ == nullptr);
        assert(stub_worker_release_ == nullptr);

        //  Immediately schedule onto the scheduler.
        CO_AWAIT service->get_scheduler()->schedule();

        std::mt19937 mt(time(nullptr)); 
        auto random_number_id = mt();

        // create the proxy channel
        auto proxy_worker_release = std::make_shared<worker_release>();

        {
            // Connect to the server to create a proxy port.
            coro::net::tcp::client proxy_client(service->get_scheduler(), opts_);
            auto connection_status = co_await proxy_client.connect();
            if(connection_status != coro::net::connect_status::connected)
            {
                CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
            }

            proxy_worker_release->channel_manager
                = std::make_shared<tcp_channel_manager>(std::move(proxy_client), timeout_, proxy_worker_release, get_operating_zone_service());
            service->get_scheduler()->schedule(proxy_worker_release->channel_manager->pump_send_and_receive());

            tcp::init_client_channel_response init_receive;
            int ret = CO_AWAIT proxy_worker_release->channel_manager->call_peer(
                rpc::get_version(),
                tcp::init_client_channel_send {.random_number_id = random_number_id,
                                               .caller_zone_id = get_zone_id().get_val(),
                                               .caller_object_id = input_descr.object_id.get_val(),
                                               .destination_zone_id = get_destination_zone_id().get_val()},
                init_receive);
            if(ret != rpc::error::OK())
                CO_RETURN ret;

            if(init_receive.err_code != rpc::error::OK())
            {
                CO_RETURN init_receive.err_code;
            }
        }

        // create the stub channel
        {
            auto stub_worker_release = std::make_shared<worker_release>();
            {
                // Connect to the server to create a stub port.
                coro::net::tcp::client stub_client(service->get_scheduler(), opts_);

                // Connect to the server.
                auto connection_status = co_await stub_client.connect();
                if(connection_status != coro::net::connect_status::connected)
                {
                    CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
                }

                stub_worker_release->channel_manager = std::make_shared<tcp_channel_manager>(
                    std::move(stub_client), timeout_, stub_worker_release, get_operating_zone_service());
            }

            tcp::init_server_channel_response init_receive;
            int ret = CO_AWAIT stub_worker_release->channel_manager->immediate_send_payload(rpc::get_version(), tcp::init_server_channel_send {.random_number_id = random_number_id}, 0);
            if(ret != rpc::error::OK())
                CO_RETURN ret;
                
            ret = CO_AWAIT stub_worker_release->channel_manager->receive_payload(init_receive, 0);
            if(ret != rpc::error::OK())
                CO_RETURN ret;

            if(init_receive.err_code != rpc::error::OK())
            {
                CO_RETURN init_receive.err_code;
            }
            
            rpc::object output_object_id = {init_receive.destination_object_id};
            output_descr = {output_object_id, get_destination_zone_id()};

            stub_worker_release_ = stub_worker_release;
            proxy_worker_release_ = proxy_worker_release;
        }

        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int)
    tcp_service_proxy::send(uint64_t protocol_version, encoding encoding, uint64_t tag,
                            caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id,
                            destination_zone destination_zone_id, object object_id, interface_ordinal interface_id,
                            method method_id, size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
    {
        if(destination_zone_id != get_destination_zone_id())
            CO_RETURN rpc::error::ZONE_NOT_SUPPORTED();

        if(!proxy_worker_release_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::call_receive call_receive;
        int ret = CO_AWAIT proxy_worker_release_->channel_manager->call_peer(
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
        if(!proxy_worker_release_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::try_cast_receive try_cast_receive;
        int ret = CO_AWAIT proxy_worker_release_->channel_manager->call_peer(protocol_version,
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

        if(!proxy_worker_release_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::addref_receive addref_receive;
        int ret = CO_AWAIT proxy_worker_release_->channel_manager->call_peer(
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

        if(!proxy_worker_release_)
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();

        tcp::addref_receive release_receive;
        int ret = CO_AWAIT proxy_worker_release_->channel_manager->call_peer(protocol_version,
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
