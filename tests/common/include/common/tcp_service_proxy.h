/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

// #include <yas/serialize.hpp>
// #include <yas/std_types.hpp>
#include <rpc/proxy.h>
#include <coro/coro.hpp>

namespace rpc
{
    
    //This is for hosts to call services on an enclave
    class tcp_service_proxy : public service_proxy
    {
        tcp_service_proxy(
            const char* name
            , destination_zone destination_zone_id
            , std::string url
            , const rpc::shared_ptr<service>& svc);
            
        tcp_service_proxy(const tcp_service_proxy& other) = default;
       
        struct channel_pair
        {
            std::shared_ptr<coro::net::tcp::client> send_channel_;
            std::shared_ptr<coro::net::tcp::client> receive_channel_;
            
            coro::mutex send_mtx_;
        };       
       
        rpc::shared_ptr<service_proxy> clone() override;
        
        static rpc::shared_ptr<tcp_service_proxy> create(
            const char* name
            , destination_zone destination_zone_id
            , const rpc::shared_ptr<service>& svc
            , std::string url);
        
        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override;

        CORO_TASK(int) send(
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
            override;
        CORO_TASK(int) try_cast(            
            uint64_t protocol_version, 
            destination_zone destination_zone_id, 
            object object_id, 
            interface_ordinal interface_id) override;
        CORO_TASK(uint64_t) add_ref(
            uint64_t protocol_version, 
            destination_channel_zone destination_channel_zone_id, 
            destination_zone destination_zone_id, 
            object object_id, 
            caller_channel_zone caller_channel_zone_id, 
            caller_zone caller_zone_id, 
            add_ref_options build_out_param_channel) override;
        CORO_TASK(uint64_t) release(
            uint64_t protocol_version, 
            destination_zone destination_zone_id, 
            object object_id, 
            caller_zone caller_zone_id) override;

        std::string url_;
        std::shared_ptr<channel_pair> channel_pair_;
        
        friend rpc::service;        
    public:
        
        virtual ~tcp_service_proxy() = default;
    };
}