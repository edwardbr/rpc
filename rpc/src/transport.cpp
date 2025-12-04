/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

// Standard C++ headers
#include <algorithm>
#include <mutex>
#include <memory>

// RPC headers
#include <rpc/rpc.h>

namespace rpc
{
    // NOTE: The local service MUST be registered in pass_thoughs_ map
    // during transport initialization:
    //   pass_thoughs_[local_zone][local_zone] = service
    //
    // Pass-throughs are registered in BOTH directions:
    //   pass_thoughs_[A][B] = pass_through  (for A↔B communication)
    //   pass_thoughs_[B][A] = pass_through  (same pass-through)
    //
    // This provides O(1) lookup for all routing scenarios.
    // Destination management

    transport::transport(std::string name, std::shared_ptr<service> service, zone adjacent_zone_id)
        : name_(name)
        , zone_id_(service->get_zone_id())
        , adjacent_zone_id_(adjacent_zone_id)
        , service_(service)
    {
        // Register local service: pass_thoughs_[local][local] = service
        // auto local_zone = zone_id_.get_val();
        // pass_thoughs_[local_zone][adjacent_zone_id.get_val()] = std::static_pointer_cast<i_marshaller>(service);
    }

    transport::transport(std::string name, zone zone_id_, zone adjacent_zone_id)
        : name_(name)
        , zone_id_(zone_id_)
        , adjacent_zone_id_(adjacent_zone_id)
    {
    }

    void transport::set_service(std::shared_ptr<service> service)
    {
        RPC_ASSERT(service);
        service_ = service;
        // Register local service: pass_thoughs_[local][local] = service
        // auto local_zone = service->get_zone_id().get_val();
        // pass_thoughs_[local_zone][adjacent_zone_id_.get_val()] = std::static_pointer_cast<i_marshaller>(service);
    }

    bool transport::inner_add_destination(destination_zone dest, caller_zone caller, std::weak_ptr<i_marshaller> handler)
    {
        auto dest_val = dest.get_val();
        auto caller_val = caller.get_val();

        // Check if entry already exists
        auto outer_it = pass_thoughs_.find(dest_val);
        if (outer_it != pass_thoughs_.end())
        {
            auto inner_it = outer_it->second.find(caller_val);
            if (inner_it != outer_it->second.end())
            {
                RPC_ASSERT(false);
                return false; // Already exists
            }
        }

        // Add entry
        pass_thoughs_[dest_val][caller_val] = handler;
        inner_increment_outbound_proxy_count(caller.as_destination());

        // // If zones are different (pass-through case), also add reverse direction
        // // For local service (dest == caller), only register once
        // if (dest_val != caller_val)
        // {
        //     // Check if reverse entry already exists
        //     auto reverse_outer_it = pass_thoughs_.find(caller_val);
        //     if (reverse_outer_it == pass_thoughs_.end()
        //         || reverse_outer_it->second.find(dest_val) == reverse_outer_it->second.end())
        //     {
        //         pass_thoughs_[caller_val][dest_val] = handler;
        //     }
        // }

        return true;
    }

    bool transport::add_destination(destination_zone dest, caller_zone caller, std::weak_ptr<i_marshaller> handler)
    {
        std::unique_lock lock(destinations_mutex_);
        return inner_add_destination(dest, caller, handler);
    }

    void transport::increment_outbound_proxy_count(destination_zone dest)
    {
        std::unique_lock lock(destinations_mutex_);
        return inner_increment_outbound_proxy_count(dest);
    }
    void transport::decrement_outbound_proxy_count(destination_zone dest)
    {
        std::unique_lock lock(destinations_mutex_);
        return inner_decrement_outbound_proxy_count(dest);
    }

    void transport::increment_inbound_stub_count(caller_zone dest)
    {
        std::unique_lock lock(destinations_mutex_);
        return inner_increment_inbound_stub_count(dest);
    }
    void transport::decrement_inbound_stub_count(caller_zone dest)
    {
        std::unique_lock lock(destinations_mutex_);
        return inner_decrement_inbound_stub_count(dest);
    }

    void transport::inner_increment_outbound_proxy_count(destination_zone dest)
    {
        auto found = outbound_proxy_count_.find(dest);
        if (found == outbound_proxy_count_.end())
        {
            outbound_proxy_count_[dest] = 1;
        }
        else
        {
            found->second++;
        }
    }
    void transport::inner_decrement_outbound_proxy_count(destination_zone dest)
    {
        auto found = outbound_proxy_count_.find(dest);
        if (found == outbound_proxy_count_.end())
        {
            RPC_WARNING("inner_decrement_outbound_proxy_count: No outbound proxy count found for dest={}", dest.get_val());
        }
        else
        {
            auto count = --found->second;
            if (count == 0)
            {
                outbound_proxy_count_.erase(found);
            }
        }
    }

    void transport::inner_increment_inbound_stub_count(caller_zone dest)
    {
        auto found = inbound_stub_count_.find(dest);
        if (found == inbound_stub_count_.end())
        {
            inbound_stub_count_[dest] = 1;
        }
        else
        {
            found->second++;
        }
    }
    void transport::inner_decrement_inbound_stub_count(caller_zone dest)
    {
        auto found = inbound_stub_count_.find(dest);
        if (found == inbound_stub_count_.end())
        {
            RPC_WARNING("inner_decrement_outbound_proxy_count: No outbound proxy count found for dest={}", dest.get_val());
        }
        else
        {
            auto count = --found->second;
            if (count == 0)
            {
                inbound_stub_count_.erase(found);
            }
        }
    }

    void transport::remove_destination(destination_zone dest, caller_zone caller)
    {
        std::unique_lock lock(destinations_mutex_);
        auto dest_val = dest.get_val();
        auto caller_val = caller.get_val();

        auto outer_it = pass_thoughs_.find(dest_val);
        if (outer_it != pass_thoughs_.end())
        {
            outer_it->second.erase(caller_val);
            // Clean up outer map if inner map is empty
            if (outer_it->second.empty())
            {
                pass_thoughs_.erase(outer_it);
            }
        }

        inner_decrement_outbound_proxy_count(caller.as_destination());
    }

    std::shared_ptr<i_marshaller> transport::create_pass_through(std::shared_ptr<transport> forward,
        const std::shared_ptr<transport>& reverse,
        const std::shared_ptr<service>& service,
        destination_zone forward_dest,
        destination_zone reverse_dest)
    {
        // Validate: pass-through should only be created between different zones
        if (forward_dest == reverse_dest)
        {
            RPC_ERROR("create_pass_through: Invalid pass-through request - forward_dest and reverse_dest are the same "
                      "({})! Pass-throughs should only route between different zones.",
                forward_dest.get_val());
            return nullptr;
        }

        // Validate: forward and reverse transports should be different objects
        if (forward.get() == reverse.get())
        {
            RPC_ERROR("create_pass_through: Invalid pass-through request - forward and reverse transports are the same "
                      "object! forward_dest={}, reverse_dest={}",
                forward_dest.get_val(),
                reverse_dest.get_val());
            return nullptr;
        }

        std::shared_ptr<pass_through> pt(
            new rpc::pass_through(forward, // forward_transport: handles messages TO final destination
                reverse,                   // reverse_transport: handles messages back to caller
                service,                   // service
                forward_dest,
                reverse_dest // reverse_destination: where reverse messages go
                ));
        pt->self_ref_ = pt; // keep self alive based on reference counts

        // we need to lock both transports destination mutexes without deadlock when adding the destinations
        // we do this by locking them in zone id order
        std::unique_ptr<std::lock_guard<std::shared_mutex>> g1;
        std::unique_ptr<std::lock_guard<std::shared_mutex>> g2;

        if (forward->get_adjacent_zone_id() < reverse->get_adjacent_zone_id())
        {
            g1 = std::make_unique<std::lock_guard<std::shared_mutex>>(forward->destinations_mutex_);
            g2 = std::make_unique<std::lock_guard<std::shared_mutex>>(reverse->destinations_mutex_);
        }
        else
        {
            g1 = std::make_unique<std::lock_guard<std::shared_mutex>>(reverse->destinations_mutex_);
            g2 = std::make_unique<std::lock_guard<std::shared_mutex>>(forward->destinations_mutex_);
        }

        // Check if pass-through already exists for this zone pair
        auto forward_handler = forward->inner_get_destination_handler(reverse_dest, forward_dest.as_caller());
        auto reverse_handler = reverse->inner_get_destination_handler(forward_dest, reverse_dest.as_caller());

        // check that they are the same
        RPC_ASSERT(!forward_handler == !reverse_handler);

        if (forward_handler)
        {
            RPC_DEBUG("create_pass_through: Found existing pass-through for forward_dest={}, reverse_dest={}",
                forward_dest.get_val(),
                reverse_dest.get_val());
            return forward_handler;
        }
        else
        {
            RPC_DEBUG("create_pass_through: Creating NEW pass-through, forward_dest={}, reverse_dest={}, pt={}",
                forward_dest.get_val(),
                reverse_dest.get_val(),
                (void*)pt.get());
            // Register pass-through on both transports
            // inner_add_destination automatically registers both directions for pass-throughs
            forward->inner_add_destination(
                reverse_dest, forward_dest.as_caller(), std::static_pointer_cast<i_marshaller>(pt));
            reverse->inner_add_destination(
                forward_dest, reverse_dest.as_caller(), std::static_pointer_cast<i_marshaller>(pt));

            // Note: We do NOT register forward_dest in service->transports here because:
            // 1. The pass-through might fail to reach the destination (zone doesn't exist downstream)
            // 2. Registering it would create routing loops
            // 3. The registration should happen after successful routing, by the caller

            return std::static_pointer_cast<i_marshaller>(pt);
        }
        return pt;
    }

    // Status management
    transport_status transport::get_status() const
    {
        return status_.load(std::memory_order_acquire);
    }

    void transport::set_status(transport_status new_status)
    {
        status_.store(new_status, std::memory_order_release);
    }

    std::shared_ptr<i_marshaller> transport::inner_get_destination_handler(destination_zone dest, caller_zone caller) const
    {
        auto dest_val = dest.get_val();
        auto caller_val = caller.get_val();

        // O(1) nested map lookup
        auto outer_it = pass_thoughs_.find(dest_val);
        if (outer_it != pass_thoughs_.end())
        {
            auto inner_it = outer_it->second.find(caller_val);
            if (inner_it != outer_it->second.end())
            {
                auto handler = inner_it->second.lock();
                if (!handler)
                {
                    RPC_WARNING("inner_get_destination_handler: weak_ptr expired for dest={}, caller={} on transport "
                                "zone={} adjacent_zone={}",
                        dest_val,
                        caller_val,
                        zone_id_.get_val(),
                        adjacent_zone_id_.get_val());
                }
                return handler;
            }
        }
        return nullptr;
    }

    // Helper to route incoming messages to registered handlers
    std::shared_ptr<i_marshaller> transport::get_destination_handler(destination_zone dest, caller_zone caller) const
    {
        if (dest == zone_id_.as_destination())
        {
            RPC_DEBUG("get_destination_handler: Requested destination is local zone {}, returning local service",
                zone_id_.get_val());
            return service_.lock();
        }
        std::shared_lock lock(destinations_mutex_);
        auto handler = inner_get_destination_handler(dest, caller);
        RPC_DEBUG("get_destination_handler: dest={}, caller={}, transport zone={}, adjacent_zone={}, found={}",
            dest.get_val(),
            caller.get_val(),
            zone_id_.get_val(),
            adjacent_zone_id_.get_val(),
            handler != nullptr);
        return handler;
    }

    // Find any pass-through that has the specified destination, regardless of caller
    // O(1) lookup: just get pass_thoughs_[dest] and return first non-expired entry
    std::shared_ptr<i_marshaller> transport::inner_find_any_passthrough_for_destination(destination_zone dest) const
    {
        auto outer_it = pass_thoughs_.find(dest);
        if (outer_it != pass_thoughs_.end())
        {
            // Iterate through all callers for this destination
            for (const auto& [caller_val, handler_weak] : outer_it->second)
            {
                auto handler = handler_weak.lock();
                if (handler)
                {
                    RPC_DEBUG(
                        "inner_find_any_passthrough_for_destination: Found pass-through for dest={} with caller={}",
                        dest.get_val(),
                        caller_val.get_val());
                    return handler;
                }
            }
        }
        return nullptr;
    }

    std::shared_ptr<i_marshaller> transport::find_any_passthrough_for_destination(destination_zone dest) const
    {
        std::shared_lock lock(destinations_mutex_);
        return inner_find_any_passthrough_for_destination(dest);
    }

    void transport::notify_all_destinations_of_disconnect()
    {
        std::shared_lock lock(destinations_mutex_);
#ifdef BUILD_COROUTINE
        auto service = service_.lock();
        if (!service)
            return;

        auto scheduler = service->get_scheduler();
#endif
        // Iterate through nested map to notify all handlers
        for (const auto& [dest_zone, inner_map] : pass_thoughs_)
        {
            for (const auto& [caller_zone_val, handler_weak] : inner_map)
            {
                if (auto handler = handler_weak.lock())
                {
#ifdef BUILD_COROUTINE
                    scheduler->schedule(
#endif
                        // Send zone_terminating post
                        handler->post(VERSION_3,
                            encoding::yas_binary,
                            0,
                            rpc::caller_channel_zone{0},
                            rpc::caller_zone{0},
                            rpc::destination_zone{dest_zone},
                            object{0},
                            interface_ordinal{0},
                            method{0},
                            post_options::zone_terminating,
                            0,
                            nullptr,
                            {})
#ifdef BUILD_COROUTINE
                    )
#endif
                        ;
                }
            }
        }
    }

    // inbound i_marshaller interface implementation
    // Routes via pass_thoughs_ map (service is registered as a destination)
    CORO_TASK(int)
    transport::inbound_send(uint64_t protocol_version,
        encoding encoding,
        uint64_t tag,
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        // Try zone pair lookup first
        auto dest = get_destination_handler(destination_zone_id, caller_zone_id);
        if (!dest)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        CO_RETURN CO_AWAIT dest->send(protocol_version,
            encoding,
            tag,
            adjacent_zone_id_.as_caller_channel(),
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
        caller_zone caller_zone_id,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        method method_id,
        post_options options,
        size_t in_size_,
        const char* in_buf_,
        const std::vector<back_channel_entry>& in_back_channel)
    {
        // Try zone pair lookup
        auto dest = get_destination_handler(destination_zone_id, caller_zone_id);
        if (!dest)
        {
            CO_RETURN;
        }

        CO_AWAIT dest->post(protocol_version,
            encoding,
            tag,
            zone_id_.as_caller_channel(),
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
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id, {0});
        if (!dest)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        CO_RETURN CO_AWAIT dest->try_cast(
            protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
    }

    CORO_TASK(int)
    transport::inbound_add_ref(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        known_direction_zone known_direction_zone_id,
        add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        bool build_caller_channel = !!(build_out_param_channel & add_ref_options::build_caller_route);
        bool build_dest_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                                  || build_out_param_channel == add_ref_options::normal
                                  || build_out_param_channel == add_ref_options::optimistic;

        auto svc = service_.lock();
        if (!svc)
        {
            CO_RETURN error::TRANSPORT_ERROR();
        }

        RPC_DEBUG("inbound_add_ref: svc_zone={}, dest_zone={}, caller_zone={}, build_caller_channel={}, "
                  "build_dest_channel={}, known_direction_zone_id={}",
            svc->get_zone_id().get_val(),
            destination_zone_id.get_val(),
            caller_zone_id.get_val(),
            build_caller_channel,
            build_dest_channel,
            known_direction_zone_id.get_val());

        if (destination_zone_id != svc->get_zone_id().as_destination() && caller_zone_id != svc->get_zone_id().as_caller())
        {
            auto dest_transport = svc->get_transport(destination_zone_id);
            if (destination_zone_id == caller_zone_id.as_destination())
            {
                // caller and destination are the same zone, so we just call the transport to pass the call along and not involve a pass through
                if (!dest_transport)
                {
                    CO_RETURN error::ZONE_NOT_FOUND();
                }

                // here we
                auto erro_code = dest_transport->add_ref(protocol_version,
                    destination_zone_id,
                    object_id,
                    get_zone_id().as_caller_channel(),
                    caller_zone_id,
                    known_direction_zone_id,
                    build_out_param_channel,
                    reference_count,
                    in_back_channel,
                    out_back_channel);
            }
            if (!dest_transport)
            {
                if (build_dest_channel)
                {
                    dest_transport = svc->get_transport(known_direction_zone_id.as_destination());
                    if (!dest_transport)
                    {
                        CO_RETURN error::ZONE_NOT_FOUND();
                    }
                }
                else
                {
                    dest_transport = shared_from_this();
                }
                svc->add_transport(destination_zone_id, dest_transport);
            }

            // otherwise we are going to use or create a pass-through
            auto passthrough = dest_transport->get_destination_handler(destination_zone_id, caller_zone_id);
            if (passthrough)
            {
                CO_RETURN CO_AWAIT passthrough->add_ref(protocol_version,
                    destination_zone_id,
                    object_id,
                    get_zone_id().as_caller_channel(),
                    caller_zone_id,
                    known_direction_zone_id,
                    build_out_param_channel,
                    reference_count,
                    in_back_channel,
                    out_back_channel);
            }

            auto caller_transport = svc->get_transport(caller_zone_id.as_destination());
            if (!caller_transport)
            {
                if (!build_dest_channel && build_caller_channel)
                {
                    caller_transport = svc->get_transport(known_direction_zone_id.as_destination());
                    if (!dest_transport)
                    {
                        CO_RETURN error::ZONE_NOT_FOUND();
                    }
                }
                else
                {
                    caller_transport = shared_from_this();
                }
                svc->add_transport(caller_zone_id.as_destination(), caller_transport);
            }

            if (dest_transport == caller_transport)
            {
                // here we directly call the destination
                CO_RETURN CO_AWAIT dest_transport->add_ref(protocol_version,
                    destination_zone_id,
                    object_id,
                    get_zone_id().as_caller_channel(),
                    caller_zone_id,
                    known_direction_zone_id,
                    build_out_param_channel,
                    reference_count,
                    in_back_channel,
                    out_back_channel);
            }

            passthrough = transport::create_pass_through(
                dest_transport, caller_transport, svc, destination_zone_id, caller_zone_id.as_destination());

            CO_RETURN CO_AWAIT passthrough->add_ref(protocol_version,
                destination_zone_id,
                object_id,
                get_zone_id().as_caller_channel(),
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel,
                reference_count,
                in_back_channel,
                out_back_channel);
        }

        // else it is a special case that the service needs to deal with
        CO_RETURN CO_AWAIT svc->add_ref(protocol_version,
            destination_zone_id,
            object_id,
            adjacent_zone_id_.as_caller_channel(),
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
        const std::vector<back_channel_entry>& in_back_channel,
        std::vector<back_channel_entry>& out_back_channel)
    {
        // Try zone pair lookup
        auto dest = get_destination_handler(destination_zone_id, caller_zone_id);
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
