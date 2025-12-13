/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <rpc/internal/marshaller.h>
#include <rpc/internal/types.h>
#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <atomic>

// Forward declaration to avoid circular dependency
namespace rpc
{
    class service;
}

namespace rpc
{
    class pass_through;

    enum class transport_status
    {
        CONNECTING,   // Initial state, establishing connection
        CONNECTED,    // Fully operational
        RECONNECTING, // Attempting to recover connection
        DISCONNECTED  // Terminal state, no further traffic allowed
    };

    class transport : public i_marshaller, public std::enable_shared_from_this<transport>
    {
    private:
        std::string name_;

        // Zone identity
        zone zone_id_;

        // the zone on the other side of the transport
        zone adjacent_zone_id_;

        // Local service reference
        std::weak_ptr<service> service_;

        // passthrough map
        std::unordered_map<destination_zone, std::unordered_map<caller_zone, std::weak_ptr<i_marshaller>>> pass_thoughs_;

        // the list of zone ids that the ajacent zone knows about
        std::unordered_map<destination_zone, std::atomic<uint64_t>> outbound_proxy_count_;
        std::unordered_map<caller_zone, std::atomic<uint64_t>> inbound_stub_count_;

        mutable std::shared_mutex destinations_mutex_;

        std::atomic<transport_status> status_{transport_status::CONNECTING};

    protected:
        // Constructor for derived transport classes
        transport(std::string name, std::shared_ptr<service> service, zone adjacent_zone_id);
        transport(std::string name, zone zone_id, zone adjacent_zone_id);

        // lock free version of same function - adds handler for zone pair
        bool inner_add_destination(destination_zone dest, caller_zone caller, std::weak_ptr<i_marshaller> handler);

        // Helper to route incoming messages to registered handlers
        // Gets handler for specific zone pair
        std::shared_ptr<i_marshaller> inner_get_destination_handler(destination_zone dest, caller_zone caller) const;

        // Find any pass-through that has the specified destination, regardless of caller
        // Returns nullptr if not found
        std::shared_ptr<i_marshaller> inner_find_any_passthrough_for_destination(destination_zone dest) const;

        void inner_increment_outbound_proxy_count(destination_zone dest);
        void inner_decrement_outbound_proxy_count(destination_zone dest);

        void inner_increment_inbound_stub_count(caller_zone dest);
        void inner_decrement_inbound_stub_count(caller_zone dest);

        void set_status(transport_status new_status);
        void notify_all_destinations_of_disconnect();

    public:
        // Public version of find_any_passthrough_for_destination for use by service::get_zone_proxy
        std::shared_ptr<i_marshaller> find_any_passthrough_for_destination(destination_zone dest) const;
        virtual ~transport();

        std::string get_name() const { return name_; }

        std::shared_ptr<service> get_service() const { return service_.lock(); }
        void set_service(std::shared_ptr<service> service);

        // Destination management for zone pairs
        // For local service, use add_destination(local_zone, local_zone, service)
        std::shared_ptr<i_marshaller> get_destination_handler(destination_zone dest, caller_zone caller) const;

        bool add_destination(destination_zone dest, caller_zone caller, std::weak_ptr<i_marshaller> handler);
        void remove_destination(destination_zone dest, caller_zone caller);

        void increment_outbound_proxy_count(destination_zone dest);
        void decrement_outbound_proxy_count(destination_zone dest);

        void increment_inbound_stub_count(caller_zone dest);
        void decrement_inbound_stub_count(caller_zone dest);

        static std::shared_ptr<i_marshaller> create_pass_through(std::shared_ptr<transport> forward,
            const std::shared_ptr<transport>& reverse,
            const std::shared_ptr<service>& service,
            destination_zone forward_dest,
            destination_zone reverse_dest);

        // Status management
        transport_status get_status() const;

        // Zone identity accessor
        zone get_zone_id() const { return zone_id_; }
        zone get_adjacent_zone_id() const { return adjacent_zone_id_; }

        virtual CORO_TASK(int) connect(interface_descriptor input_descr, interface_descriptor& output_descr) = 0;

        // inbound i_marshaller interface abstraction
        // Routes to transport_ for remote zones or service_ for local zone
        CORO_TASK(int)
        inbound_send(uint64_t protocol_version,
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
            std::vector<back_channel_entry>& out_back_channel);

        CORO_TASK(void)
        inbound_post(uint64_t protocol_version,
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
            const std::vector<back_channel_entry>& in_back_channel);

        CORO_TASK(int)
        inbound_try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel);

        CORO_TASK(int)
        inbound_add_ref(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel);

        CORO_TASK(int)
        inbound_release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count,
            const std::vector<back_channel_entry>& in_back_channel,
            std::vector<back_channel_entry>& out_back_channel);
    };

    class transport_to_parent : public transport
    {
        std::shared_ptr<transport> parent_;

    public:
        transport_to_parent(std::string name, std::shared_ptr<service> service, std::shared_ptr<transport> parent)
            : transport(name, service, parent->get_zone_id())
            , parent_(parent)
        {
        }
    };

} // namespace rpc
