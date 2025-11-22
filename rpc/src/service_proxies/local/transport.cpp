/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */

#include <rpc/service_proxies/local/transport.h>
#include <rpc/rpc.h>

namespace local
{
    parent_transport::parent_transport(
        std::string name, std::shared_ptr<rpc::service> service, std::shared_ptr<child_transport> parent)
        : rpc::transport(name, service, parent->get_zone_id())
        , parent_(parent)
    {
    }

    parent_transport::parent_transport(std::string name, rpc::zone zone_id, std::shared_ptr<child_transport> parent)
        : rpc::transport(name, zone_id, parent->get_zone_id())
        , parent_(parent)
    {
    }

    // Outbound i_marshaller interface - sends from child to parent
    CORO_TASK(int)
    parent_transport::send(uint64_t protocol_version,
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
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
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
        else
        {
            auto parent = parent_.get_nullable();
            if (!parent)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            // Forward to parent transport's inbound handler
            CO_RETURN CO_AWAIT parent->send(protocol_version,
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
    }

    CORO_TASK(void)
    parent_transport::post(uint64_t protocol_version,
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
        const std::vector<rpc::back_channel_entry>& in_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
            CO_RETURN CO_AWAIT dest->post(protocol_version,
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
        else
        {
            auto parent = parent_.get_nullable();
            if (!parent)
            {
                CO_RETURN;
            }

            CO_AWAIT parent->post(protocol_version,
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
    }

    CORO_TASK(int)
    parent_transport::try_cast(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
            CO_RETURN CO_AWAIT dest->try_cast(
                protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
        }
        else
        {
            auto parent = parent_.get_nullable();
            if (!parent)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT parent->try_cast(
                protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
        }
    }

    CORO_TASK(int)
    parent_transport::add_ref(uint64_t protocol_version,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::known_direction_zone known_direction_zone_id,
        rpc::add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
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
        else
        {
            auto parent = parent_.get_nullable();
            if (!parent)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT parent->add_ref(protocol_version,
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
    }

    CORO_TASK(int)
    parent_transport::release(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id,
        rpc::release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
            CO_RETURN CO_AWAIT dest->release(protocol_version,
                destination_zone_id,
                object_id,
                caller_zone_id,
                options,
                reference_count,
                in_back_channel,
                out_back_channel);
        }
        else
        {
            auto parent = parent_.get_nullable();
            if (!parent)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT parent->release(protocol_version,
                destination_zone_id,
                object_id,
                caller_zone_id,
                options,
                reference_count,
                in_back_channel,
                out_back_channel);
        }
    }

    // Transport from parent zone to child zone
    // Used by parent to communicate with child

    // Outbound i_marshaller interface - sends from parent to child
    CORO_TASK(int)
    child_transport::send(uint64_t protocol_version,
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
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
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
        else
        {
            auto child = child_.get_nullable();
            if (!child)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            // Forward to child service's inbound handler via its transport
            // Since child_service IS an i_marshaller, we can call directly
            CO_RETURN CO_AWAIT std::static_pointer_cast<rpc::i_marshaller>(child)->send(protocol_version,
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
    }

    CORO_TASK(void)
    child_transport::post(uint64_t protocol_version,
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
        const std::vector<rpc::back_channel_entry>& in_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
            CO_RETURN CO_AWAIT dest->post(protocol_version,
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
        else
        {
            auto child = child_.get_nullable();
            if (!child)
            {
                CO_RETURN;
            }

            CO_AWAIT std::static_pointer_cast<rpc::i_marshaller>(child)->post(protocol_version,
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
    }

    CORO_TASK(int)
    child_transport::try_cast(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::interface_ordinal interface_id,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
            CO_RETURN CO_AWAIT dest->try_cast(
                protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
        }
        else
        {
            auto child = child_.get_nullable();
            if (!child)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT std::static_pointer_cast<rpc::i_marshaller>(child)->try_cast(
                protocol_version, destination_zone_id, object_id, interface_id, in_back_channel, out_back_channel);
        }
    }

    CORO_TASK(int)
    child_transport::add_ref(uint64_t protocol_version,
        rpc::destination_channel_zone destination_channel_zone_id,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_channel_zone caller_channel_zone_id,
        rpc::caller_zone caller_zone_id,
        rpc::known_direction_zone known_direction_zone_id,
        rpc::add_ref_options build_out_param_channel,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler_or_create_passthrough(caller_zone_id, destination_zone_id);
        if (dest)
        {
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
        else
        {
            auto child = child_.get_nullable();
            if (!child)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT std::static_pointer_cast<rpc::i_marshaller>(child)->add_ref(protocol_version,
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
    }

    CORO_TASK(int)
    child_transport::release(uint64_t protocol_version,
        rpc::destination_zone destination_zone_id,
        rpc::object object_id,
        rpc::caller_zone caller_zone_id,
        rpc::release_options options,
        uint64_t& reference_count,
        const std::vector<rpc::back_channel_entry>& in_back_channel,
        std::vector<rpc::back_channel_entry>& out_back_channel)
    {
        auto dest = get_destination_handler(destination_zone_id);
        if (dest)
        {
            CO_RETURN CO_AWAIT dest->release(protocol_version,
                destination_zone_id,
                object_id,
                caller_zone_id,
                options,
                reference_count,
                in_back_channel,
                out_back_channel);
        }
        else
        {
            auto child = child_.get_nullable();
            if (!child)
            {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            CO_RETURN CO_AWAIT std::static_pointer_cast<rpc::i_marshaller>(child)->release(protocol_version,
                destination_zone_id,
                object_id,
                caller_zone_id,
                options,
                reference_count,
                in_back_channel,
                out_back_channel);
        }
    }
}
