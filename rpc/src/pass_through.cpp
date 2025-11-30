/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>
#include <rpc/internal/pass_through.h>
#include <rpc/internal/transport.h>

namespace rpc
{

    pass_through::pass_through(std::shared_ptr<transport> forward,
        std::shared_ptr<transport> reverse,
        std::shared_ptr<service> service,
        destination_zone forward_dest,
        destination_zone reverse_dest)
        : forward_destination_(forward_dest)
        , reverse_destination_(reverse_dest)
        , forward_transport_(forward)
        , reverse_transport_(reverse)
        , service_(service)
    {
    }

    std::shared_ptr<pass_through> pass_through::create(std::shared_ptr<transport> forward,
        std::shared_ptr<transport> reverse,
        std::shared_ptr<service> service,
        destination_zone forward_dest,
        destination_zone reverse_dest)
    {
        std::shared_ptr<pass_through> pt(
            new rpc::pass_through(forward, // forward_transport: handles messages TO final destination
                reverse,                   // reverse_transport: handles messages back to caller
                service,                   // service
                forward_dest,
                reverse_dest // reverse_destination: where reverse messages go
                ));
        pt->self_ref_ = pt; // keep self alive based on reference counts
        return pt;
    }

    pass_through::~pass_through() { }

    std::shared_ptr<transport> pass_through::get_directional_transport(destination_zone dest)
    {
        if (dest.get_val() == forward_destination_.get_val())
        {
            return forward_transport_;
        }
        else if (dest.get_val() == reverse_destination_.get_val())
        {
            return reverse_transport_;
        }
        return nullptr;
    }

    CORO_TASK(int)
    pass_through::send(uint64_t protocol_version,
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
        // Determine target transport based on destination_zone
        auto target_transport = get_directional_transport(destination_zone_id);
        if (!target_transport)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        // Check transport status before routing
        if (target_transport->get_status() != transport_status::CONNECTED)
        {
            // Transport error - trigger self-deletion
            trigger_self_destruction();
            CO_RETURN error::TRANSPORT_ERROR();
        }

        // Forward the call directly to the transport
        auto result = CO_AWAIT target_transport->send(protocol_version,
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

        // If transport error, trigger self-deletion
        if (result == error::TRANSPORT_ERROR())
        {
            trigger_self_destruction();
        }

        CO_RETURN result;
    }

    CORO_TASK(void)
    pass_through::post(uint64_t protocol_version,
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
        // Check if this is a zone_terminating post
        bool is_zone_terminating = (options & post_options::zone_terminating) == post_options::zone_terminating;

        // Determine target transport based on destination_zone
        auto target_transport = get_directional_transport(destination_zone_id);
        if (!target_transport)
        {
            CO_RETURN;
        }

        // Check transport status before routing (unless zone_terminating)
        if (!is_zone_terminating && target_transport->get_status() != transport_status::CONNECTED)
        {
            // Transport error - trigger self-deletion
            trigger_self_destruction();
            CO_RETURN;
        }

        // Forward the post message directly to the transport
        CO_AWAIT target_transport->post(protocol_version,
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

        // If zone_terminating, delete ourselves after forwarding the message
        if (is_zone_terminating)
        {
            trigger_self_destruction();
        }
    }

    CORO_TASK(int)
    pass_through::try_cast(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        // Determine target transport based on destination_zone
        auto target_transport = get_directional_transport(destination_zone_id);
        if (!target_transport)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        // Check transport status before routing
        if (target_transport->get_status() != transport_status::CONNECTED)
        {
            // Transport error - trigger self-deletion
            trigger_self_destruction();
            CO_RETURN error::TRANSPORT_ERROR();
        }

        // Forward the call directly to the transport
        auto result = CO_AWAIT target_transport->try_cast(
            protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);

        // If transport error, trigger self-deletion
        if (result == error::TRANSPORT_ERROR())
        {
            trigger_self_destruction();
        }

        CO_RETURN result;
    }

    CORO_TASK(int)
    pass_through::add_ref(uint64_t protocol_version,
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
        bool build_caller_channel = !!(build_out_param_channel & add_ref_options::build_caller_route);
        bool build_dest_channel = !!(build_out_param_channel & add_ref_options::build_destination_route)
                                  || build_out_param_channel == add_ref_options::normal
                                  || build_out_param_channel == add_ref_options::optimistic;

        std::shared_ptr<rpc::transport> caller_transport;
        std::shared_ptr<rpc::transport> destination_transport;

        // Determine target transport based on destination_zone
        if (build_dest_channel)
        {
            destination_transport = get_directional_transport(destination_zone_id);
            if (!destination_transport)
            {
                CO_RETURN error::ZONE_NOT_FOUND();
            }
            // Check transport status before routing
            if (destination_transport->get_status() != transport_status::CONNECTED)
            {
                // Transport error - trigger self-deletion
                trigger_self_destruction();
                CO_RETURN error::TRANSPORT_ERROR();
            }
        }

        if (build_caller_channel)
        {
            caller_transport = get_directional_transport(caller_zone_id.as_destination());
            if (!caller_transport)
            {
                CO_RETURN error::ZONE_NOT_FOUND();
            }
            // Check transport status before routing
            if (caller_transport->get_status() != transport_status::CONNECTED)
            {
                // Transport error - trigger self-deletion
                trigger_self_destruction();
                CO_RETURN error::TRANSPORT_ERROR();
            }
        }

        if (build_dest_channel)
        {
            // Forward the add_ref call to the target transport
            auto result = CO_AWAIT destination_transport->add_ref(protocol_version,
                destination_zone_id,
                object_id,
                caller_channel_zone_id,
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel & ~add_ref_options::build_caller_route,
                reference_count,
                in_back_channel,
                out_back_channel);

            // ONLY increment pass_through reference count if the forward succeeded
            // This ensures our count matches the actual established references
            if (result != error::OK())
            {
                trigger_self_destruction();
                CO_RETURN result;
            }
        }

        if (build_caller_channel)
        {
            // Forward the add_ref call to the target transport
            auto result = CO_AWAIT caller_transport->add_ref(protocol_version,
                destination_zone_id,
                object_id,
                caller_channel_zone_id,
                caller_zone_id,
                known_direction_zone_id,
                build_out_param_channel & ~add_ref_options::build_destination_route,
                reference_count,
                in_back_channel,
                out_back_channel);

            // ONLY increment pass_through reference count if the forward succeeded
            // This ensures our count matches the actual established references
            if (result != error::OK())
            {
                trigger_self_destruction();
                CO_RETURN result;
            }
        }

        // Use bitwise AND to check flags, not exact equality
        // because build_out_param_channel may have additional build flags
        if (!!(build_out_param_channel & add_ref_options::build_destination_route)
            && !!(build_out_param_channel & add_ref_options::build_caller_route)
            && destination_zone_id.as_caller() == caller_zone_id)
        {
            // this is a passthrough addref and should not be included in either count
        }
        else if (!!(build_out_param_channel & add_ref_options::optimistic))
        {
            optimistic_count_.fetch_add(1, std::memory_order_acq_rel);
        }
        else
        {
            shared_count_.fetch_add(1, std::memory_order_acq_rel);
        }

        CO_RETURN error::OK();
    }

    CORO_TASK(int)
    pass_through::release(uint64_t protocol_version,
        destination_zone destination_zone_id,
        object object_id,
        caller_zone caller_zone_id,
        release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        // Determine target transport based on destination_zone
        auto target_transport = get_directional_transport(destination_zone_id);
        if (!target_transport)
        {
            CO_RETURN error::ZONE_NOT_FOUND();
        }

        // Check transport status before routing
        if (target_transport->get_status() != transport_status::CONNECTED)
        {
            // Transport error - trigger self-deletion
            trigger_self_destruction();
            CO_RETURN error::TRANSPORT_ERROR();
        }

        auto result = CO_AWAIT target_transport->release(protocol_version,
            destination_zone_id,
            object_id,
            caller_zone_id,
            options,
            reference_count,
            in_back_channel,
            out_back_channel);

        // If the forward failed, trigger cleanup and return the error
        if (result != error::OK())
        {
            trigger_self_destruction();
            CO_RETURN result;
        }

        // Update pass_through reference count ONLY if forward succeeded
        bool should_delete = false;

        // Use bitwise AND to check flags for consistency with add_ref
        if (!!(options & release_options::normal))
        {
            uint64_t prev = shared_count_.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1 && optimistic_count_.load(std::memory_order_acquire) == 0)
            {
                should_delete = true;
            }
        }
        else if (!!(options & release_options::optimistic))
        {
            uint64_t prev = optimistic_count_.fetch_sub(1, std::memory_order_acq_rel);
            if (prev == 1 && shared_count_.load(std::memory_order_acquire) == 0)
            {
                should_delete = true;
            }
        }

        // Trigger self-destruction if counts are zero
        if (should_delete)
        {
            trigger_self_destruction();
        }

        CO_RETURN result;
    }

    void pass_through::trigger_self_destruction()
    {
        RPC_WARNING("trigger_self_destruction: Destroying pass-through, forward_dest={}, reverse_dest={}, pt={}, "
                    "shared={}, optimistic={}",
            forward_destination_.get_val(),
            reverse_destination_.get_val(),
            (void*)this,
            shared_count_.load(),
            optimistic_count_.load());

        // Remove destinations from transports in BOTH directions
        if (forward_transport_)
        {
            forward_transport_->remove_destination(forward_destination_, reverse_destination_);
        }
        if (reverse_transport_)
        {
            reverse_transport_->remove_destination(reverse_destination_, forward_destination_);
        }

        // Release transport and service pointers
        forward_transport_.reset();
        reverse_transport_.reset();
        service_.reset();

        // Release self-reference - this will delete the pass_through when last external reference is gone
        self_ref_.reset();
    }

} // namespace rpc
