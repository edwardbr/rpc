/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <functional>

#include <rpc/rpc.h>

namespace local
{
    class child_transport;
    
    // Transport from child zone to parent zone
    // Used by child to communicate with parent
    class parent_transport : public rpc::transport
    {
        std::weak_ptr<child_transport> parent_;

    public:

        parent_transport(std::string name, std::shared_ptr<rpc::service> service, std::shared_ptr<child_transport> parent);
    
        /*parent_transport(rpc::zone zone_id,
                       std::shared_ptr<rpc::service> child_service,
                       std::shared_ptr<rpc::transport> parent_transport)
            : rpc::transport(child_service)
            , parent_(parent_transport)
        {
            zone_id_ = zone_id;
            service_ = child_service;

            // Register local child service in destinations_ map
            if (auto svc = child_service.lock())
            {
                destinations_[zone_id.as_destination()] = std::static_pointer_cast<rpc::i_marshaller>(svc);
            }

            set_status(rpc::transport_status::CONNECTED);
        }*/

        virtual ~parent_transport() = default;

        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override
        {
            // Parent transport is connected immediately - no handshake needed
            CO_RETURN rpc::error::OK();
        }

        template<class in_param_type, class out_param_type>
        static std::function<CORO_TASK(int)(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr, const std::shared_ptr<child_transport>& parent, std::shared_ptr<parent_transport>& child)> 
            bind(std::function<CORO_TASK(int)(const rpc::shared_ptr<in_param_type>&, rpc::shared_ptr<out_param_type>&, const std::shared_ptr<rpc::child_service>&)>&& child_entry_point_fn)
        {
            
            return [child_entry_point_fn = std::move(child_entry_point_fn)](rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr, const std::shared_ptr<child_transport>& parent, std::shared_ptr<parent_transport>& child) mutable -> CORO_TASK(int)
            {
                child = std::make_shared<parent_transport>("child", nullptr, parent);
                
                CO_RETURN CO_AWAIT rpc::child_service::create_child_zone<in_param_type, out_param_type>("child", child, input_descr, output_descr, std::move(child_entry_point_fn)
    #ifdef BUILD_COROUTINE
                , parent->get_service()->get_scheduler()
    #endif
                );
            };
        }        

        // Outbound i_marshaller interface - sends from child to parent
        CORO_TASK(int) send(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(void) post(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            rpc::post_options options,
            size_t in_size_,
            const char* in_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel) override;

        CORO_TASK(int) try_cast(
            uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int) add_ref(uint64_t protocol_version,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::known_direction_zone known_direction_zone_id,
            rpc::add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int) release(uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id,
            rpc::release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
    };

    // Transport from parent zone to child zone
    // Used by parent to communicate with child
    
    class child_transport : public rpc::transport, public std::enable_shared_from_this<child_transport>
    {
        stdex::member_ptr<parent_transport> child_;
        std::function<CORO_TASK(int)(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr, const std::shared_ptr<child_transport>& parent, std::shared_ptr<parent_transport>& child)> child_entry_point_fn_;

    public:
        child_transport(std::string name, std::shared_ptr<rpc::service> service, rpc::zone adjacent_zone_id, 
            std::function<CORO_TASK(int)(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr, const std::shared_ptr<child_transport>& parent, std::shared_ptr<parent_transport>& child)>&& child_entry_point_fn)
            : rpc::transport(name,service,adjacent_zone_id)
            , child_entry_point_fn_(std::move(child_entry_point_fn))
        {
            set_status(rpc::transport_status::CONNECTED);
        }

        virtual ~child_transport() = default;
        
        CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) override
        {
            std::shared_ptr<parent_transport> child;
            auto ret = CO_AWAIT child_entry_point_fn_(input_descr, output_descr, shared_from_this(), child);
            child_entry_point_fn_ = nullptr;
            if(ret == rpc::error::OK())
                child_ = child;
            CO_RETURN ret;
        }

        // Outbound i_marshaller interface - sends from parent to child
        CORO_TASK(int) send(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
            
        CORO_TASK(void) post(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            rpc::post_options options,
            size_t in_size_,
            const char* in_buf_,
            const std::vector<rpc::back_channel_entry>& in_back_channel) override;

        CORO_TASK(int) try_cast(
            uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int) add_ref(uint64_t protocol_version,
            rpc::destination_channel_zone destination_channel_zone_id,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_channel_zone caller_channel_zone_id,
            rpc::caller_zone caller_zone_id,
            rpc::known_direction_zone known_direction_zone_id,
            rpc::add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int) release(uint64_t protocol_version,
            rpc::destination_zone destination_zone_id,
            rpc::object object_id,
            rpc::caller_zone caller_zone_id,
            rpc::release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;
    };
}
