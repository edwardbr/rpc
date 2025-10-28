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

namespace rpc
{

    enum class transport_status
    {
        CONNECTING,   // Initial state, establishing connection
        CONNECTED,    // Fully operational
        RECONNECTING, // Attempting to recover connection
        DISCONNECTED  // Terminal state, no further traffic allowed
    };

    class transport : public i_marshaller
    {
    private:
    
        std::string name_;
    
        // Zone identity
        zone zone_id_;

        // the zone on the other side of the transport
        zone adjacent_zone_id_;

        // Local service reference
        std::weak_ptr<rpc::service> service_;

        // Map destination_zone to handler (service or pass_through)
        std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destinations_;
        mutable std::shared_mutex destinations_mutex_;

        std::atomic<transport_status> status_{transport_status::CONNECTING};

    protected:
        // Constructor for derived transport classes
        transport(std::string name, std::shared_ptr<rpc::service> service, zone adjacent_zone_id);
        // transport(std::string name, zone zone_id, zone adjacent_zone_id);

    public:
        virtual ~transport() = default;
        
        std::string get_name() const {return name_;}

        std::shared_ptr<rpc::service> get_service() const { return service_.lock(); }
        void set_service(std::shared_ptr<rpc::service> service);

        // Destination management to zones NOT going via the adjacent_zone
        void add_destination(destination_zone dest, std::weak_ptr<i_marshaller> handler);
        void remove_destination(destination_zone dest);

        // Status management
        transport_status get_status() const;

        // Zone identity accessor
        zone get_zone_id() const { return zone_id_; }
        zone get_adjacent_zone_id() const { return adjacent_zone_id_; }
        
        virtual CORO_TASK(int) connect(rpc::interface_descriptor input_descr, rpc::interface_descriptor& output_descr) = 0;

    protected:
        // Helper to route incoming messages to registered handlers
        std::shared_ptr<i_marshaller> get_destination_handler(destination_zone dest) const;
        void set_status(transport_status new_status);
        void notify_all_destinations_of_disconnect();

    public:
        // inbound i_marshaller interface abstraction
        // Routes to transport_ for remote zones or service_ for local zone
        CORO_TASK(int)
        inbound_send(uint64_t protocol_version,
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
            std::vector<rpc::back_channel_entry>& out_back_channel);

        CORO_TASK(void)
        inbound_post(uint64_t protocol_version,
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
            const std::vector<rpc::back_channel_entry>& in_back_channel);

        CORO_TASK(int)
        inbound_try_cast(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            interface_ordinal interface_id,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel);

        CORO_TASK(int)
        inbound_add_ref(uint64_t protocol_version,
            destination_channel_zone destination_channel_zone_id,
            destination_zone destination_zone_id,
            object object_id,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            known_direction_zone known_direction_zone_id,
            add_ref_options build_out_param_channel,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel);

        CORO_TASK(int)
        inbound_release(uint64_t protocol_version,
            destination_zone destination_zone_id,
            object object_id,
            caller_zone caller_zone_id,
            release_options options,
            uint64_t& reference_count,
            const std::vector<rpc::back_channel_entry>& in_back_channel,
            std::vector<rpc::back_channel_entry>& out_back_channel);
    };
    
    class transport_to_parent : public transport
    {
        std::shared_ptr<transport> parent_;
    public:
        transport_to_parent(std::string name, std::shared_ptr<rpc::service> service, std::shared_ptr<transport> parent) : 
            transport(name, service, parent->get_zone_id()),
            parent_(parent)
        {}
    };

} // namespace rpc
