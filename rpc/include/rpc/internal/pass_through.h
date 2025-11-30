/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/internal/marshaller.h>
#include <rpc/internal/types.h>
#include <rpc/internal/coroutine_support.h>
#include <memory>
#include <atomic>

namespace rpc
{

    // Forward declarations
    class service;
    class transport;

    // Pass-through routes messages between two transports
    // Implements i_marshaller to receive and forward RPC calls
    class pass_through : public i_marshaller, public std::enable_shared_from_this<pass_through>
    {
    private:
        destination_zone forward_destination_; // Zone reached via forward_transport
        destination_zone reverse_destination_; // Zone reached via reverse_transport

        std::atomic<uint64_t> shared_count_{0};
        std::atomic<uint64_t> optimistic_count_{0};

        std::shared_ptr<transport> forward_transport_; // Transport to forward destination
        std::shared_ptr<transport> reverse_transport_; // Transport to reverse destination
        std::shared_ptr<service> service_;

        std::shared_ptr<pass_through> self_ref_; // Keep self alive based on reference counts

        pass_through(std::shared_ptr<transport> forward,
            std::shared_ptr<transport> reverse,
            std::shared_ptr<service> service,
            destination_zone forward_dest,
            destination_zone reverse_dest);

    public:
        static std::shared_ptr<pass_through> create(std::shared_ptr<transport> forward,
            std::shared_ptr<transport> reverse,
            std::shared_ptr<service> service,
            destination_zone forward_dest,
            destination_zone reverse_dest);
        ~pass_through();

        // i_marshaller implementations
        CORO_TASK(int)
        send(uint64_t protocol_version,
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
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(void)
        post(uint64_t protocol_version,
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
            const std::vector<rpc::back_channel_entry>& in_back_channel) override;

        CORO_TASK(int)
        try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int)
        add_ref(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        CORO_TASK(int)
        release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel) override;

        // Status monitoring
        uint64_t get_shared_count() const { return shared_count_.load(std::memory_order_acquire); }
        uint64_t get_optimistic_count() const { return optimistic_count_.load(std::memory_order_acquire); }

        // Access to transports for testing
        std::shared_ptr<transport> get_forward_transport() const { return forward_transport_; }
        std::shared_ptr<transport> get_reverse_transport() const { return reverse_transport_; }

        destination_zone get_forward_destination() const
        {
            return forward_destination_;
        } // Zone reached via forward_transport
        destination_zone get_reverse_destination() const
        {
            return reverse_destination_;
        } // Zone reached via reverse_transport

    private:
        std::shared_ptr<transport> get_directional_transport(destination_zone dest);
        void trigger_self_destruction();

        friend transport;
    };

} // namespace rpc
