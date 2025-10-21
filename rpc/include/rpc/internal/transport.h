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
    protected:
        // Map destination_zone to handler (service or pass_through)
        std::unordered_map<destination_zone, std::weak_ptr<i_marshaller>> destinations_;
        mutable std::shared_mutex destinations_mutex_;
        std::atomic<transport_status> status_{transport_status::CONNECTING};

    public:
        virtual ~transport() = default;

        // Destination management
        void add_destination(destination_zone dest, std::weak_ptr<i_marshaller> handler)
        {
            std::unique_lock lock(destinations_mutex_);
            destinations_[dest] = handler;
        }

        void remove_destination(destination_zone dest)
        {
            std::unique_lock lock(destinations_mutex_);
            destinations_.erase(dest);
        }

        // Status management
        transport_status get_status() const { return status_.load(std::memory_order_acquire); }

    protected:
        // Helper to route incoming messages to registered handlers
        std::shared_ptr<i_marshaller> get_destination_handler(destination_zone dest) const
        {
            std::shared_lock lock(destinations_mutex_);
            auto it = destinations_.find(dest);
            if (it != destinations_.end())
            {
                return it->second.lock();
            }
            return nullptr;
        }

        void set_status(transport_status new_status) { status_.store(new_status, std::memory_order_release); }

        void notify_all_destinations_of_disconnect()
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
    };

} // namespace rpc
