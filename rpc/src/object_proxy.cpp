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

    CORO_TASK(int) object_proxy::add_ref(add_ref_options options)
    {
        // Increment appropriate counter based on reference type
        bool is_optimistic = static_cast<bool>(options & add_ref_options::optimistic);
        int prev_count;

        if (is_optimistic)
        {
            prev_count = optimistic_count_.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            prev_count = shared_count_.fetch_add(1, std::memory_order_relaxed);
        }

        // Get service_proxy once for consistency
        auto service_proxy = service_proxy_.get_nullable();

#ifdef USE_RPC_LOGGING
        if (service_proxy)
        {
            RPC_DEBUG("object_proxy::add_ref: {} reference for service zone={} destination_zone={} object_id={} "
                      "(shared={}, optimistic={}) prev_count={}",
                is_optimistic ? "optimistic" : "shared",
                service_proxy->get_zone_id().get_val(),
                service_proxy->get_destination_zone_id().get_val(),
                object_id_.get_val(),
                shared_count_.load(),
                optimistic_count_.load(),
                prev_count);
        }
#endif

        // CRITICAL: On 0→1 transition, establish remote reference IMMEDIATELY
        // This ensures remote service's reference count ≥ 1 while local pointers exist
        if (prev_count == 0 && service_proxy)
        {
            service_proxy->add_external_ref();
            // Call service_proxy->sp_add_ref() to increment remote service's reference count
            // This MUST happen sequentially to ensure remote count ≥ 1 before constructor returns
            uint64_t ref_count = 0;
            auto err = CO_AWAIT service_proxy->sp_add_ref(object_id_, {0}, options, {0}, ref_count);
            if (err)
            {
                // Rollback local counter on failure
                if (is_optimistic)
                {
                    optimistic_count_.fetch_sub(1, std::memory_order_relaxed);
                }
                else
                {
                    shared_count_.fetch_sub(1, std::memory_order_relaxed);
                }

#ifdef USE_RPC_LOGGING
                RPC_ERROR("object_proxy::add_ref: Failed to establish remote reference: {} reference zone={} "
                         "destination_zone={} object_id={} error={}",
                    is_optimistic ? "optimistic" : "shared",
                    service_proxy->get_zone_id().get_val(),
                    service_proxy->get_destination_zone_id().get_val(),
                    object_id_.get_val(),
                    err);
#endif
                CO_RETURN err;
            }
        }

        CO_RETURN error::OK();
    }

    void object_proxy::release(bool is_optimistic)
    {
        // Decrement appropriate counter based on reference type
        int prev_count;
        if (is_optimistic)
        {
            prev_count = optimistic_count_.fetch_sub(1, std::memory_order_acq_rel);
        }
        else
        {
            prev_count = shared_count_.fetch_sub(1, std::memory_order_acq_rel);
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
                shared_count_.load(),
                optimistic_count_.load());
        }
#endif

        // When a specific reference type count reaches zero, notify the server immediately
        // This allows the server to delete the stub when shared_count=0 even if optimistic_count>0
        if (prev_count == 1) // We just decremented this specific counter to 0
        {
            if (service_proxy)
            {
#ifdef USE_RPC_LOGGING
                RPC_DEBUG("object_proxy::release: {} on_object_proxy_released cleanup for object_id={}", is_optimistic ? "optimistic" : "shared", object_id_.get_val());
#endif
                service_proxy->on_object_proxy_released(this->shared_from_this(), is_optimistic);
            }
        }
    }

    object_proxy::~object_proxy()
    {
        auto service_proxy = service_proxy_.get_nullable();

        int current_shared = shared_count_.load();
        int current_optimistic = optimistic_count_.load();

#ifdef USE_RPC_LOGGING
        if (service_proxy)
        {
            RPC_DEBUG("object_proxy destructor: service zone={} destination_zone={} object_id={} "
                      "(current: shared={}, optimistic={})",
                service_proxy->get_zone_id().get_val(),
                service_proxy->get_destination_zone_id().get_val(),
                object_id_.get_val(),
                current_shared,
                current_optimistic);
        }
#endif

#ifdef USE_RPC_TELEMETRY
        if (auto telemetry_service = rpc::get_telemetry_service(); telemetry_service && service_proxy)
        {
            telemetry_service->on_object_proxy_deletion(
                service_proxy->get_zone_id(), service_proxy->get_destination_zone_id(), object_id_);
        }
#endif

        RPC_ASSERT(current_shared == 0);
        RPC_ASSERT(current_optimistic == 0);

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
            // add_ref is async because object_proxy::add_ref() is async
            CORO_TASK(int) object_proxy_add_ref(const std::shared_ptr<rpc::object_proxy>& ob, rpc::add_ref_options options)
            {
                CO_RETURN CO_AWAIT ob->add_ref(options);
            }

            // release remains synchronous
            void object_proxy_release(const std::shared_ptr<rpc::object_proxy>& ob, bool is_optimistic)
            {
                ob->release(is_optimistic);
            }
            
            void get_object_proxy_reference_counts(const std::shared_ptr<rpc::object_proxy>& ob, int& shared_count, int& optimistic_count)
            {
                shared_count = ob->get_shared_count();
                optimistic_count = ob->get_optimistic_count();
            }

            // Synchronous direct increment for control block construction (no remote calls)
            void object_proxy_add_ref_shared(const std::shared_ptr<rpc::object_proxy>& ob)
            {
                ob->add_ref_shared();
            }
        }
    }
}
