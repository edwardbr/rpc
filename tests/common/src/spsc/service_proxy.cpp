/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifndef _IN_ENCLAVE
#include <thread>
#include <span>

#include <random>

#include "common/spsc/service_proxy.h"

namespace rpc::spsc
{
    service_proxy::service_proxy(const char* name, destination_zone destination_zone_id,
                                 const rpc::shared_ptr<service>& svc,
                                 std::shared_ptr<rpc::spsc::channel_manager> channel, std::chrono::milliseconds timeout,
                                 queue_type* send_spsc_queue, queue_type* receive_spsc_queue)
        : rpc::service_proxy(name, destination_zone_id, svc)
        , channel_manager_(channel)
        , timeout_(timeout)
        , send_spsc_queue_(send_spsc_queue)
        , receive_spsc_queue_(receive_spsc_queue)
    {
    }

    service_proxy::~service_proxy()
    {
        get_operating_zone_service()->get_scheduler()->schedule(channel_manager_->shutdown());
    }

    rpc::shared_ptr<rpc::service_proxy> service_proxy::clone()
    {
        return rpc::shared_ptr<rpc::service_proxy>(new service_proxy(*this));
    }

    rpc::shared_ptr<service_proxy> service_proxy::create(const char* name, destination_zone destination_zone_id,
                                                         const rpc::shared_ptr<service>& svc,
                                                         std::chrono::milliseconds timeout, queue_type* send_spsc_queue,
                                                         queue_type* receive_spsc_queue)
    {
        RPC_ASSERT(svc);

        auto ret = rpc::shared_ptr<service_proxy>(
            new service_proxy(name, destination_zone_id, svc, nullptr, timeout, send_spsc_queue, receive_spsc_queue));
        return ret;
    }

    CORO_TASK(rpc::shared_ptr<service_proxy>)
    service_proxy::attach_remote(const char* name, const rpc::shared_ptr<service>& svc,
                                 destination_zone destination_zone_id,
                                 std::shared_ptr<rpc::spsc::channel_manager> channel, queue_type* send_spsc_queue,
                                 queue_type* receive_spsc_queue)
    {
        RPC_ASSERT(svc);

        // std::string msg("attach_remote this service ");
        // msg += std::to_string(svc->get_zone_id().get_val());
        // msg += " to ";
        // msg += std::to_string(destination_zone_id.get_val());
        // LOG_CSTR(msg.c_str());

        auto ret = rpc::shared_ptr<service_proxy>(new service_proxy(name, destination_zone_id, svc, channel,
                                                                    std::chrono::milliseconds(0), send_spsc_queue,
                                                                    receive_spsc_queue));

        co_return ret;
    }

    CORO_TASK(int)
    service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {
        // std::string msg("connect ");
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

        auto service = get_operating_zone_service();
        assert(channel_manager_ == nullptr);

        //  Immediately schedule onto the scheduler.
        CO_AWAIT service->get_scheduler()->schedule();

        // create the proxy channel
        {
            // Connect to the server to create a proxy port.

            channel_manager_ = rpc::spsc::channel_manager::create(timeout_, get_operating_zone_service(),
                                                                  send_spsc_queue_, receive_spsc_queue_, nullptr);
        }

        if(!channel_manager_->pump_send_and_receive())
        {
            LOG_CSTR("unable to pump_send_and_receive proxy");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        {
            // register the proxy connection
            init_client_channel_response init_receive;
            int ret = CO_AWAIT channel_manager_->call_peer(
                rpc::get_version(),
                init_client_channel_send {.caller_zone_id = get_zone_id().get_val(),
                                               .caller_object_id = input_descr.object_id.get_val(),
                                               .destination_zone_id = get_destination_zone_id().get_val()},
                init_receive);
            if(ret != rpc::error::OK())
            {
                LOG_CSTR("channel_manager->call_peer");
                CO_RETURN ret;
            }

            if(init_receive.err_code != rpc::error::OK())
            {
                LOG_CSTR("init_client_channel_send failed");
                CO_RETURN init_receive.err_code;
            }

            rpc::object output_object_id = {init_receive.destination_object_id};
            output_descr = {output_object_id, get_destination_zone_id()};
        }

        CO_RETURN rpc::error::OK();
    }

    CORO_TASK(int)
    service_proxy::send(uint64_t protocol_version, encoding encoding, uint64_t tag,
                        caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id,
                        destination_zone destination_zone_id, object object_id, interface_ordinal interface_id,
                        method method_id, size_t in_size_, const char* in_buf_, std::vector<char>& out_buf_)
    {
        // std::string msg("send ");
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

        if(destination_zone_id != get_destination_zone_id())
        {
            LOG_CSTR("failed service_proxy::send ZONE_NOT_SUPPORTED");
            CO_RETURN rpc::error::ZONE_NOT_SUPPORTED();
        }

        if(!channel_manager_)
        {
            LOG_CSTR("failed service_proxy::send SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        call_receive call_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(
            protocol_version,
            call_send {.encoding = encoding,
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
        {
            LOG_CSTR("failed service_proxy::send call_send");
            CO_RETURN ret;
        }

        out_buf_.swap(call_receive.payload);

        // msg = "send complete ";
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

        CO_RETURN call_receive.err_code;
    }

    CORO_TASK(int)
    service_proxy::try_cast(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                            interface_ordinal interface_id)
    {
        // std::string msg("try_cast ");
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

        if(!channel_manager_)
        {
            LOG_CSTR("failed try_cast SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        try_cast_receive try_cast_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(protocol_version,
                                                       try_cast_send {
                                                           .destination_zone_id = destination_zone_id.get_val(),
                                                           .object_id = object_id.get_val(),
                                                           .interface_id = interface_id.get_val(),
                                                       },
                                                       try_cast_receive);
        if(ret != rpc::error::OK())
        {
            LOG_CSTR("failed try_cast call_peer");
            CO_RETURN ret;
        }

        // msg = std::string("try_cast complete ");
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

        CO_RETURN try_cast_receive.err_code;
    }

    CORO_TASK(uint64_t)
    service_proxy::add_ref(uint64_t protocol_version, destination_channel_zone destination_channel_zone_id,
                           destination_zone destination_zone_id, object object_id,
                           caller_channel_zone caller_channel_zone_id, caller_zone caller_zone_id,
                           rpc::add_ref_options build_out_param_channel)
    {
        // auto msg = std::string("add_ref ");
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

#ifdef USE_RPC_TELEMETRY
        if(auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(get_zone_id(), destination_zone_id, destination_channel_zone_id,
                                                        get_caller_zone_id(), object_id, build_out_param_channel);
        }
#endif
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();

        if(!channel_manager_)
        {
            LOG_CSTR("failed add_ref SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        addref_receive addref_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(
            protocol_version,
            addref_send {.destination_channel_zone_id = destination_channel_zone_id.get_val(),
                              .destination_zone_id = destination_zone_id.get_val(),
                              .object_id = object_id.get_val(),
                              .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                              .caller_zone_id = caller_zone_id.get_val(),
                              .build_out_param_channel = (add_ref_options)build_out_param_channel},
            addref_receive);
        if(ret != rpc::error::OK())
        {
            LOG_CSTR("failed add_ref addref_send");
            CO_RETURN ret;
        }

        if(addref_receive.err_code != rpc::error::OK())
        {
            LOG_CSTR("failed addref_receive.err_code failed");
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
        // msg = std::string("add_ref complete ");
        // msg += std::to_string(get_zone_id().get_val());
        // LOG_CSTR(msg.c_str());

        CO_RETURN addref_receive.ref_count;
    }

    CORO_TASK(uint64_t)
    service_proxy::release(uint64_t protocol_version, destination_zone destination_zone_id, object object_id,
                           caller_zone caller_zone_id)
    {
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();

        auto msg = std::string("release ");
        msg += std::to_string(get_zone_id().get_val());
        LOG_CSTR(msg.c_str());

        if(!channel_manager_)
        {
            LOG_CSTR("failed release SERVICE_PROXY_LOST_CONNECTION");
            CO_RETURN rpc::error::SERVICE_PROXY_LOST_CONNECTION();
        }

        release_receive release_receive;
        int ret = CO_AWAIT channel_manager_->call_peer(protocol_version,
                                                       release_send {
                                                           .destination_zone_id = destination_zone_id.get_val(),
                                                           .object_id = object_id.get_val(),
                                                           .caller_zone_id = caller_zone_id.get_val(),
                                                       },
                                                       release_receive);
        if(ret != rpc::error::OK())
        {
            LOG_CSTR("failed release release_send");
            CO_RETURN ret;
        }

        if(release_receive.err_code != rpc::error::OK())
        {
            LOG_CSTR("failed release_receive.err_code failed");
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

        msg = std::string("release complete ");
        msg += std::to_string(get_zone_id().get_val());
        LOG_CSTR(msg.c_str());

        CO_RETURN release_receive.ref_count;
    }
}
#endif
