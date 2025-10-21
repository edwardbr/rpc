#include <rpc/internal/transport.h>

namespace local
{
    class transport : public rpc::transport
    {
        std::shared_ptr<rpc::child_service> child;
        std::weak_ptr<rpc::service> parent;

        virtual ~transport() = default;

        CORO_TASK(int) send(uint64_t protocol_version,
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
            override
        {
            // Get the destination handler from parent class destinations_ map
            auto handler = get_destination_handler(destination_zone_id);
            if (!handler) {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            // Forward the call to the handler
            CO_RETURN CO_AWAIT handler->send(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id,
                in_size_, in_buf_, out_buf_,
                in_back_channel, out_back_channel);
        }

        CORO_TASK(void) post(uint64_t protocol_version,
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
            override
        {
            // Get the destination handler from parent class destinations_ map
            auto handler = get_destination_handler(destination_zone_id);
            if (!handler) {
                CO_RETURN;
            }

            // Forward the call to the handler
            CO_AWAIT handler->post(
                protocol_version, encoding, tag,
                caller_channel_zone_id, caller_zone_id, destination_zone_id,
                object_id, interface_id, method_id, options,
                in_size_, in_buf_, in_back_channel);
        }

        CORO_TASK(int) try_cast(
            uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel)
            override
        {
            // Get the destination handler from parent class destinations_ map
            auto handler = get_destination_handler(destination_zone_id);
            if (!handler) {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            // Forward the call to the handler
            CO_RETURN CO_AWAIT handler->try_cast(
                protocol_version, destination_zone_id,
                object_id, interface_id,
                in_back_channel, out_back_channel);
        }

        CORO_TASK(int) add_ref(uint64_t protocol_version,
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
            override
        {
            // Get the destination handler from parent class destinations_ map
            auto handler = get_destination_handler(destination_zone_id);
            if (!handler) {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            // Forward the call to the handler
            CO_RETURN CO_AWAIT handler->add_ref(
                protocol_version,
                destination_channel_zone_id, destination_zone_id, object_id,
                caller_channel_zone_id, caller_zone_id, known_direction_zone_id,
                build_out_param_channel, reference_count,
                in_back_channel, out_back_channel);
        }

        CORO_TASK(int) release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel)
            override
        {
            // Get the destination handler from parent class destinations_ map
            auto handler = get_destination_handler(destination_zone_id);
            if (!handler) {
                CO_RETURN rpc::error::ZONE_NOT_FOUND();
            }

            // Forward the call to the handler
            CO_RETURN CO_AWAIT handler->release(
                protocol_version, destination_zone_id, object_id,
                caller_zone_id, options, reference_count,
                in_back_channel, out_back_channel);
        }
    };
}