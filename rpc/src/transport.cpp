/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// Standard C++ headers
#include <algorithm>

// RPC headers
#include <rpc/rpc.h>

namespace rpc
{
    // NOTE: The local service MUST be registered in destinations_ map
    // during transport initialization:
    //   transport->add_destination(zone_id.as_destination(), service);
    //
    // This allows all inbound_* methods to route uniformly through
    // get_destination_handler(), keeping code simple and consistent.
    //
    // Only inbound_add_ref() has special logic for channel setup and
    // fork detection - all other methods are simple lookups.
    // Destination management

    transport::transport(std::string name, std::shared_ptr<rpc::service> service, zone adjacent_zone_id)
        : name_(name)
        , zone_id_(service->get_zone_id())
        , adjacent_zone_id_(adjacent_zone_id)
        , service_(service)
    {
        destinations_[zone_id_.as_destination()] = std::static_pointer_cast<i_marshaller>(service);
    }
    
    // transport::transport(std::string name, zone zone_id_, zone adjacent_zone_id)
    //     : name_(name)
    //     , zone_id_(zone_id_)
    //     , adjacent_zone_id_(adjacent_zone_id)
    // {
    // }
    
    void transport::set_service(std::shared_ptr<rpc::service> service)
    {
        RPC_ASSERT(service);
        service_ = service;
        destinations_[service->get_zone_id().as_destination()] = std::static_pointer_cast<i_marshaller>(service);
    }    
    
    void transport::add_destination(destination_zone dest, std::weak_ptr<i_marshaller> handler)
    {
        std::unique_lock lock(destinations_mutex_);
        destinations_[dest] = handler;
    }

    void transport::remove_destination(destination_zone dest)
    {
        std::unique_lock lock(destinations_mutex_);
        destinations_.erase(dest);
    }

    // Status management
    transport_status transport::get_status() const
    {
        return status_.load(std::memory_order_acquire);
    }

    // Helper to route incoming messages to registered handlers
    std::shared_ptr<i_marshaller> transport::get_destination_handler(destination_zone dest) const
    {
        std::shared_lock lock(destinations_mutex_);
        auto it = destinations_.find(dest);
        if (it != destinations_.end())
        {
            return it->second.lock();
        }
        return nullptr;
    }

    void transport::set_status(transport_status new_status)
    {
        status_.store(new_status, std::memory_order_release);
    }

    void transport::notify_all_destinations_of_disconnect()
    {
        std::shared_lock lock(destinations_mutex_);
        for (auto& [dest_zone, handler_weak] : destinations_)
        {
            if (auto handler = handler_weak.lock())
            {
                // Send zone_terminating post to each destination
                handler->post(VERSION_3,
                    encoding::yas_binary,
                    0,
                    caller_channel_zone{0},
                    caller_zone{0},
                    dest_zone,
                    object{0},
                    interface_ordinal{0},
                    method{0},
                    post_options::zone_terminating,
                    0,
                    nullptr,
                    {});
            }
        }
    }

    // inbound i_marshaller interface implementation
    // Routes via destinations_ map (service is registered as a destination)
    CORO_TASK(int)
    transport::inbound_send(uint64_t protocol_version,
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
        std::vector<char>& out_buf_,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (!dest)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        CO_RETURN CO_AWAIT dest->send(protocol_version,
            encoding,
            tag,
            caller_channel_zone_id,
            caller_zone_id,
            destination_zone_id,
            object_id,
            interface_id,
            method_id,
            in_size_,
            in_buf_,
            out_buf_,
            in_back_channel,
            out_back_channel);
    }

    CORO_TASK(void)
    transport::inbound_post(uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        post_options options,
        size_t in_size_,
        const char* in_buf_,
        const std::vector<rpc::back_channel_entry>& in_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (!dest)
        {
            CO_RETURN;
        }

        CO_AWAIT dest->post(protocol_version,
            encoding,
            tag,
            caller_channel_zone_id,
            caller_zone_id,
            destination_zone_id,
            object_id,
            interface_id,
            method_id,
            options,
            in_size_,
            in_buf_,
            in_back_channel);
    }

    CORO_TASK(int)
    transport::inbound_try_cast(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (!dest)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        CO_RETURN CO_AWAIT dest->try_cast(
            protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
    }

    CORO_TASK(int)
    transport::inbound_add_ref(uint64_t protocol_version,
        destination_channel_zone destination_channel_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        caller_channel_zone caller_channel_zone_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,
        add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto build_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                             || !!(build_out_param_channel & add_ref_options::build_caller_route);
                             
        auto dest_channel = destination_channel_zone_id.is_set() ? destination_channel_zone_id.get_val() : destination_zone_id.get_val();
        auto caller_channel = caller_channel_zone_id.is_set() ? caller_channel_zone_id.get_val() : caller_zone_id.get_val();

        auto svc = service_.lock();
        if(!svc)
        {
            CO_RETURN error::TRANSPORT_ERROR();
        }
        
        if (destination_zone_id != svc->get_zone_id().as_destination() 
            && 
            (
                // if the call is standard passthough add_ref
                !build_channel
                // else the fork is beyond this node
                || (dest_channel == caller_channel && build_channel)
            ))
        {
            // then pass it through to the relevant i_marshaller
            
            // Route based on destination: local service or remote transport
            auto dest = get_destination_handler(destination_zone_id);
            if (!dest)
            {
                dest = svc->create_pass_through( destination_channel_zone_id,
                                                destination_zone_id,
                                                caller_channel_zone_id,
                                                caller_zone_id,
                                                known_direction_zone_id);
                if (!dest)
                {                
                    CO_RETURN error::TRANSPORT_ERROR();
                }
            }
            
            CO_RETURN CO_AWAIT dest->add_ref(protocol_version,
                destination_channel_zone_id,
                destination_zone_id,
                object_id,
                caller_channel_zone_id,
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel,
                reference_count,
                in_back_channel,
                out_back_channel);
        }

        // else it is a special case that the service needs to deal with
        CO_RETURN CO_AWAIT svc->add_ref(protocol_version,
            destination_channel_zone_id,
            destination_zone_id,
            object_id,
            caller_channel_zone_id,
            caller_zone_id,
            known_direction_zone_id,
            build_out_param_channel,
            reference_count,
            in_back_channel,
            out_back_channel);
    }

    CORO_TASK(int)
    transport::inbound_release(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (!dest)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        CO_RETURN CO_AWAIT dest->release(protocol_version,
            destination_zone_id,
            object_id,
            caller_zone_id,
            options,
            reference_count,
            in_back_channel,
            out_back_channel);
    }
} // namespace rpc
