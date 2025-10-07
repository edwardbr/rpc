/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#include <rpc/rpc.h>
#include <cstdio>
#include <limits>

namespace rpc
{
    object_proxy::object_proxy(object object_id, std::shared_ptr<rpc::service_proxy> service_proxy)
        : object_id_(object_id)
        , service_proxy_(service_proxy)
    {
    }

    void object_proxy::add_ref(add_ref_options options)
    {
        // Increment appropriate counter based on reference type
        bool is_optimistic = static_cast<bool>(options & add_ref_options::optimistic);
        if (is_optimistic)
        {
            inherited_optimistic_count_.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            inherited_shared_count_.fetch_add(1, std::memory_order_relaxed);
        }

        // Get service_proxy once for consistency
        auto service_proxy = service_proxy_.get_nullable();

#ifdef USE_RPC_LOGGING
        if (service_proxy)
        {
            RPC_DEBUG("object_proxy::add_ref: {} reference for service zone={} destination_zone={} object_id={} "
                      "(shared={}, optimistic={})",
                is_optimistic ? "optimistic" : "shared",
                service_proxy->get_zone_id().get_val(),
                service_proxy->get_destination_zone_id().get_val(),
                object_id_.get_val(),
                inherited_shared_count_.load(),
                inherited_optimistic_count_.load());
        }
#endif
    }

    void object_proxy::release(release_options options)
    {
        // Decrement appropriate counter based on reference type
        bool is_optimistic = static_cast<bool>(options & release_options::optimistic);
        int prev_count;
        if (is_optimistic)
        {
            prev_count = inherited_optimistic_count_.fetch_sub(1, std::memory_order_acq_rel);
        }
        else
        {
            prev_count = inherited_shared_count_.fetch_sub(1, std::memory_order_acq_rel);
        }

        // Get service_proxy once for consistency
        auto service_proxy = service_proxy_.get_nullable();

#ifdef USE_RPC_LOGGING
        if (service_proxy)
        {
            RPC_DEBUG("object_proxy::release: {} reference for service zone={} destination_zone={} object_id={} "
                      "(shared={}, optimistic={})",
                is_optimistic ? "optimistic" : "shared",
                service_proxy->get_zone_id().get_val(),
                service_proxy->get_destination_zone_id().get_val(),
                object_id_.get_val(),
                inherited_shared_count_.load(),
                inherited_optimistic_count_.load());
        }
#endif

        // When a specific reference type count reaches zero, notify the server immediately
        // This allows the server to delete the stub when shared_count=0 even if optimistic_count>0
        if (prev_count == 1) // We just decremented this specific counter to 0
        {
            if (service_proxy)
            {
#ifdef USE_RPC_LOGGING
                RPC_DEBUG("object_proxy::release: final cleanup for object_id={}", object_id_.get_val());
#endif
                if (is_optimistic)
                {
                    // All optimistic references released - send 1 to indicate the 1→0 transition
                    service_proxy->on_object_proxy_released(object_id_, 0, 1);
                }
                else
                {
                    // All shared references released - send 1 to indicate the 1→0 transition
                    service_proxy->on_object_proxy_released(object_id_, 1, 0);
                }
            }
        }
    }

    object_proxy::~object_proxy()
    {
        // Destructor is called when BOTH counters are 0
        // All releases have already been handled by release() method when each counter hit 0
        // But we MUST notify service_proxy to remove this proxy from the map
        auto service_proxy = service_proxy_.get_nullable();

        int inherited_shared = inherited_shared_count_.load();
        int inherited_optimistic = inherited_optimistic_count_.load();

#ifdef USE_RPC_LOGGING
        if (service_proxy)
        {
            RPC_DEBUG("object_proxy destructor: service zone={} destination_zone={} object_id={} "
                      "(inherited: shared={}, optimistic={})",
                service_proxy->get_zone_id().get_val(),
                service_proxy->get_destination_zone_id().get_val(),
                object_id_.get_val(),
                inherited_shared,
                inherited_optimistic);
        }
#endif

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service && service_proxy)
        {
            telemetry_service->on_object_proxy_deletion(
                service_proxy->get_zone_id(), service_proxy->get_destination_zone_id(), object_id_);
        }
#endif

        // Always notify service_proxy so it can remove this proxy from the map
        // Pass being_destroyed=true to enable race condition handling and map cleanup
        if (service_proxy)
        {
            service_proxy->on_object_proxy_released(object_id_, inherited_shared, inherited_optimistic, true);
        }

        service_proxy_ = nullptr;
    }

    CORO_TASK(int)
    object_proxy::send(uint64_t protocol_version,
        rpc::encoding encoding,
        uint64_t tag,
        rpc::interface_ordinal interface_id,
        rpc::method method_id,
        size_t in_size_,
        const char* in_buf_,
        std::vector<char>& out_buf_)
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            CO_RETURN rpc::error::ZONE_NOT_INITIALISED();
        CO_RETURN CO_AWAIT service_proxy->send_from_this_zone(
            protocol_version, encoding, tag, object_id_, interface_id, method_id, in_size_, in_buf_, out_buf_);
    }

    CORO_TASK(int) object_proxy::try_cast(std::function<interface_ordinal(uint64_t)> id_getter)
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            CO_RETURN rpc::error::ZONE_NOT_INITIALISED();
        CO_RETURN CO_AWAIT service_proxy->sp_try_cast(service_proxy->get_destination_zone_id(), object_id_, id_getter);
    }

    destination_zone object_proxy::get_destination_zone_id() const
    {
        auto service_proxy = service_proxy_.get_nullable();
        RPC_ASSERT(service_proxy);
        if (!service_proxy)
            return destination_zone{0};
        return service_proxy->get_destination_zone_id();
    }

    void object_proxy::register_interface(interface_ordinal interface_id, rpc::weak_ptr<casting_interface>& value)
    {
        std::lock_guard guard(insert_control_);
        proxy_map[interface_id] = value;
    }

    namespace __rpc_internal
    {
        namespace __shared_ptr_control_block
        {
            // forward declarations implemented in object_proxy.cpp
            void object_proxy_add_ref(const std::shared_ptr<rpc::object_proxy>& ob, rpc::add_ref_options options)
            {
                ob->add_ref(options);
            }
            
            void object_proxy_release(const std::shared_ptr<rpc::object_proxy>& ob, rpc::release_options options)
            {
                ob->release(options);
            }
        }
    }
}
