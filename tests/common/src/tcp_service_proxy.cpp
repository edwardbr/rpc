/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#ifndef _IN_ENCLAVE
#include <thread>
#include <span>

#include "common/tcp_service_proxy.h"

#include <tcp/tcp.h>

namespace rpc
{
    tcp_service_proxy::tcp_service_proxy(
        const char* name
        , destination_zone destination_zone_id
        , std::string url
        , const rpc::shared_ptr<service>& svc)
        : service_proxy(name, destination_zone_id, svc)
        , url_(url)
    {}

    rpc::shared_ptr<service_proxy> tcp_service_proxy::clone() {return rpc::shared_ptr<service_proxy>(new tcp_service_proxy(*this));}
    
    rpc::shared_ptr<tcp_service_proxy> tcp_service_proxy::create(
        const char* name
        , destination_zone destination_zone_id
        , const rpc::shared_ptr<service>& svc
        , std::string url)
    {
        RPC_ASSERT(svc);
        auto ret = rpc::shared_ptr<tcp_service_proxy>(new tcp_service_proxy(name, destination_zone_id, url, svc));
        return ret;
    }
            
    CORO_TASK(int) tcp_service_proxy::connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr)
    {   
        auto service = get_operating_zone_service();
        
        //auto noop_task = []() ->coro::task<void>{CO_RETURN;};
        // Immediately schedule onto the scheduler.
        CO_AWAIT service->get_scheduler()->schedule();
        
        auto random_number_id = (uint64_t)std::rand();
        
        auto cp = std::make_shared<channel_pair>();
        
        // create the client channel
        {
            // Create the tcp::client with the default settings, see tcp::client for how to set the
            // ip address, port, and optionally enabling SSL/TLS.
            cp->send_channel_ = std::make_shared<coro::net::tcp::client>(service->get_scheduler());
            
            // Ommitting error checking code for the client, each step should check the status and
            // verify the number of bytes sent or received.

            // Connect to the server.
            auto connection_status = co_await cp->send_channel_->connect();
            assert(connection_status == coro::net::connect_status::connected);

            // Make sure the client socket can be written to.
            auto status = co_await cp->send_channel_->poll(coro::poll_op::write);
            assert(status == coro::poll_status::event);
            
            auto message = tcp::envelope 
                            {   .version = rpc::get_version(),
                                .payload_fingerprint = rpc::id<tcp::init_client_channel_send>::get(rpc::get_version()),
                                .payload = rpc::to_compressed_yas_binary(
                                    tcp::init_client_channel_send 
                                    {
                                        .random_number_id = random_number_id,
                                        .caller_zone_id = get_zone_id().get_val(),
                                        .caller_object_id = input_descr.object_id.get_val(),
                                        .destination_zone_id = get_destination_zone_id().get_val()
                                    }
                                )
                            };
            auto payload = rpc::to_yas_binary(message);                                           
            auto marshal_status = cp->send_channel_->send(std::span{(const char*)payload.data(), payload.size()});
            assert(marshal_status.first == coro::net::send_status::ok);
            
            co_await cp->send_channel_->poll(coro::poll_op::read);

            std::vector<char> buf;
            std::vector<char> response(256, '\0');
            auto [recv_status, recv_bytes] = cp->send_channel_->recv(response);
            if(recv_status == coro::net::recv_status::ok)
            {
                do
                {
                    for(auto& ch : recv_bytes)
                    {
                        buf.push_back(ch);
                    }
                }while(recv_status == coro::net::recv_status::ok);
            }
            
            tcp::envelope resp;
            auto err = rpc::from_yas_binary(rpc::span(buf), resp);
            assert(!err.empty());
            
            tcp::init_client_channel_response init_receive;
            err = rpc::from_yas_compressed_binary(rpc::span(resp.payload), init_receive);
            assert(!err.empty());
            
            rpc::object output_object_id = {init_receive.destination_object_id};
            output_descr = {output_object_id, get_destination_zone_id()};
            
            if(init_receive.err_code != rpc::error::OK())
            {
                CO_RETURN init_receive.err_code;
            }
        }
        
        // create the server channel
        {
            // Create the tcp::client with the default settings, see tcp::client for how to set the
            // ip address, port, and optionally enabling SSL/TLS.
            cp->receive_channel_ = std::make_shared<coro::net::tcp::client>(service->get_scheduler());
            
            // Ommitting error checking code for the client, each step should check the status and
            // verify the number of bytes sent or received.

            // Connect to the server.
            auto connection_status = co_await cp->receive_channel_->connect();
            assert(connection_status == coro::net::connect_status::connected);

            // Make sure the client socket can be written to.
            auto status = co_await cp->receive_channel_->poll(coro::poll_op::write);
            assert(status == coro::poll_status::event);
        
            auto message = tcp::envelope 
                            {   .version = rpc::get_version(),
                                .payload_fingerprint = rpc::id<tcp::init_server_channel_send>::get(rpc::get_version()),
                                .payload = rpc::to_compressed_yas_binary(
                                    tcp::init_server_channel_send 
                                    {
                                        .random_number_id = random_number_id
                                    }
                                )
                            };
            auto payload = rpc::to_yas_binary(message);                                           
            auto marshal_status = cp->receive_channel_->send(std::span{(const char*)payload.data(), payload.size()});
            assert(marshal_status.first == coro::net::send_status::ok);
            
            co_await cp->receive_channel_->poll(coro::poll_op::read);

            std::vector<char> buf;
            std::vector<char> response(256, '\0');
            auto [recv_status, recv_bytes] = cp->receive_channel_->recv(response);
            if(recv_status == coro::net::recv_status::ok)
            {
                do
                {
                    for(auto& ch : recv_bytes)
                    {
                        buf.push_back(ch);
                    }
                }while(recv_status == coro::net::recv_status::ok);
            }
            
            tcp::envelope resp;
            auto err = rpc::from_yas_binary(rpc::span(buf), resp);
            assert(!err.empty());
            
            tcp::init_server_channel_response init_receive;
            err = rpc::from_yas_compressed_binary(rpc::span(resp.payload), init_receive);
            assert(!err.empty());
            
            if(init_receive.err_code != rpc::error::OK())
            {
                CO_RETURN init_receive.err_code;
            }
        }
        channel_pair_ = cp;
        
        CO_RETURN rpc::error::OK();
    }   


    CORO_TASK(int) tcp_service_proxy::send(
        uint64_t protocol_version, 
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
        if(destination_zone_id != get_destination_zone_id())
            CO_RETURN rpc::error::ZONE_NOT_SUPPORTED();   
        
        auto cp = channel_pair_;
        if(!cp)
           CO_RETURN rpc::error::INVALID_CHANNEL();
        auto scoped_lock = co_await cp->send_mtx_.lock();
        
        auto message = tcp::envelope 
            {   .version = protocol_version,
                .payload_fingerprint = rpc::id<tcp::call_send>::get(protocol_version),
                .payload = rpc::to_compressed_yas_binary(
                    tcp::call_send 
                    {
                        .encoding = encoding,
                        .tag = tag,
                        .caller_channel_zone_id = caller_channel_zone_id.get_val(), 
                        .caller_zone_id = caller_zone_id.get_val(), 
                        .destination_zone_id = destination_zone_id.get_val(), 
                        .object_id = object_id.get_val(), 
                        .interface_id = interface_id.get_val(), 
                        .method_id = method_id.get_val(),
                        .payload = std::vector<char>(in_buf_, in_buf_ + in_size_)
                    }
                )
            };
            
        auto payload = rpc::to_yas_binary(message);                                           
        auto marshal_status = cp->send_channel_->send(std::span{(const char*)payload.data(), payload.size()});
        assert(marshal_status.first == coro::net::send_status::ok);
        
        co_await cp->send_channel_->poll(coro::poll_op::read);

        std::vector<char> buf;
        std::vector<char> response(256, '\0');
        auto [recv_status, recv_bytes] = cp->send_channel_->recv(response);
        if(recv_status == coro::net::recv_status::ok)
        {
            do
            {
                for(auto& ch : recv_bytes)
                {
                    buf.push_back(ch);
                }
            }while(recv_status == coro::net::recv_status::ok);
        }
        
        tcp::envelope resp;
        auto err = rpc::from_yas_binary(rpc::span(buf), resp);
        assert(!err.empty());
        
        tcp::call_receive call_receive;
        err = rpc::from_yas_compressed_binary(rpc::span(resp.payload), call_receive);
        assert(!err.empty());
        
        out_buf_.swap(call_receive.payload);
        CO_RETURN call_receive.err_code;
    }

    CORO_TASK(int) tcp_service_proxy::try_cast(
        uint64_t protocol_version
        , destination_zone destination_zone_id
        , object object_id
        , interface_ordinal interface_id)
    {
        auto cp = channel_pair_;
        if(!cp)
           CO_RETURN rpc::error::INVALID_CHANNEL();
        auto scoped_lock = co_await cp->send_mtx_.lock();
        
        auto message = tcp::envelope 
            {   .version = protocol_version,
                .payload_fingerprint = rpc::id<tcp::try_cast_send>::get(protocol_version),
                .payload = rpc::to_compressed_yas_binary(
                    tcp::try_cast_send 
                    {
                        .destination_zone_id = destination_zone_id.get_val(), 
                        .object_id = object_id.get_val(), 
                        .interface_id = interface_id.get_val(), 
                    }
                )
            };
            
        auto payload = rpc::to_yas_binary(message);                                           
        auto marshal_status = cp->send_channel_->send(std::span{(const char*)payload.data(), payload.size()});
        assert(marshal_status.first == coro::net::send_status::ok);
        
        co_await cp->send_channel_->poll(coro::poll_op::read);

        std::vector<char> buf;
        std::vector<char> response(256, '\0');
        auto [recv_status, recv_bytes] = cp->send_channel_->recv(response);
        if(recv_status == coro::net::recv_status::ok)
        {
            do
            {
                for(auto& ch : recv_bytes)
                {
                    buf.push_back(ch);
                }
            }while(recv_status == coro::net::recv_status::ok);
        }
        
        tcp::envelope resp;
        auto err = rpc::from_yas_binary(rpc::span(buf), resp);
        assert(!err.empty());
        
        tcp::try_cast_receive try_cast_receive;
        err = rpc::from_yas_compressed_binary(rpc::span(resp.payload), try_cast_receive);
        assert(!err.empty());
        
        CO_RETURN try_cast_receive.err_code;
    }
    
    CORO_TASK(uint64_t) tcp_service_proxy::add_ref(
        uint64_t protocol_version
        , destination_channel_zone destination_channel_zone_id
        , destination_zone destination_zone_id
        , object object_id
        , caller_channel_zone caller_channel_zone_id
        , caller_zone caller_zone_id
        , add_ref_options build_out_param_channel)
    {
#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
        {
            telemetry_service->on_service_proxy_add_ref(
                get_zone_id()
                , destination_zone_id
                , destination_channel_zone_id
                , get_caller_zone_id()
                , object_id
                , build_out_param_channel);
        }
#endif        
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();

        auto cp = channel_pair_;
        if(!cp)
           CO_RETURN rpc::error::INVALID_CHANNEL();
        auto scoped_lock = co_await cp->send_mtx_.lock();
        
        auto message = tcp::envelope 
            {   .version = protocol_version,
                .payload_fingerprint = rpc::id<tcp::addref_send>::get(protocol_version),
                .payload = rpc::to_compressed_yas_binary(
                    tcp::addref_send 
                    {
                        .destination_channel_zone_id = destination_channel_zone_id.get_val(),
                        .destination_zone_id = destination_zone_id.get_val(),
                        .object_id = object_id.get_val(),
                        .caller_channel_zone_id = caller_channel_zone_id.get_val(),
                        .caller_zone_id = caller_zone_id.get_val(),
                        .build_out_param_channel = (tcp::add_ref_options)build_out_param_channel 
                    }
                )
            };
            
        auto payload = rpc::to_yas_binary(message);                                           
        auto marshal_status = cp->send_channel_->send(std::span{(const char*)payload.data(), payload.size()});
        assert(marshal_status.first == coro::net::send_status::ok);
        
        co_await cp->send_channel_->poll(coro::poll_op::read);

        std::vector<char> buf;
        std::vector<char> response(256, '\0');
        auto [recv_status, recv_bytes] = cp->send_channel_->recv(response);
        if(recv_status == coro::net::recv_status::ok)
        {
            do
            {
                for(auto& ch : recv_bytes)
                {
                    buf.push_back(ch);
                }
            }while(recv_status == coro::net::recv_status::ok);
        }
        
        tcp::envelope resp;
        auto err = rpc::from_yas_binary(rpc::span(buf), resp);
        assert(!err.empty());
        
        tcp::addref_receive addref_receive;
        err = rpc::from_yas_compressed_binary(rpc::span(resp.payload), addref_receive);
        assert(!err.empty());
        
        if (addref_receive.err_code != rpc::error::OK())
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
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

    CORO_TASK(uint64_t) tcp_service_proxy::release(
        uint64_t protocol_version
        , destination_zone destination_zone_id
        , object object_id
        , caller_zone caller_zone_id)
    {
        constexpr auto add_ref_failed_val = std::numeric_limits<uint64_t>::max();

        auto cp = channel_pair_;
        if(!cp)
           CO_RETURN rpc::error::INVALID_CHANNEL();
        auto scoped_lock = co_await cp->send_mtx_.lock();
        
        auto message = tcp::envelope 
            {   .version = protocol_version,
                .payload_fingerprint = rpc::id<tcp::release_send>::get(protocol_version),
                .payload = rpc::to_compressed_yas_binary(
                    tcp::release_send 
                    {
                        .destination_zone_id = destination_zone_id.get_val(),
                        .object_id = object_id.get_val(),
                        .caller_zone_id = caller_zone_id.get_val(),
                    }
                )
            };
            
        auto payload = rpc::to_yas_binary(message);                                           
        auto marshal_status = cp->send_channel_->send(std::span{(const char*)payload.data(), payload.size()});
        assert(marshal_status.first == coro::net::send_status::ok);
        
        co_await cp->send_channel_->poll(coro::poll_op::read);

        std::vector<char> buf;
        std::vector<char> response(256, '\0');
        auto [recv_status, recv_bytes] = cp->send_channel_->recv(response);
        if(recv_status == coro::net::recv_status::ok)
        {
            do
            {
                for(auto& ch : recv_bytes)
                {
                    buf.push_back(ch);
                }
            }while(recv_status == coro::net::recv_status::ok);
        }
        
        tcp::envelope resp;
        auto err = rpc::from_yas_binary(rpc::span(buf), resp);
        assert(!err.empty());
        
        tcp::addref_receive release_receive;
        err = rpc::from_yas_compressed_binary(rpc::span(resp.payload), release_receive);
        assert(!err.empty());
        
        if (release_receive.err_code != rpc::error::OK())
        {
#ifdef USE_RPC_TELEMETRY
            if (auto telemetry_service = rpc::telemetry_service_manager::get(); telemetry_service)
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
