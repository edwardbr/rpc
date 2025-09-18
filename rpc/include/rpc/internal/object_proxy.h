/*
 *   Copyright (c) 2025 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <vector>

#include <rpc/internal/types.h>
#include <rpc/internal/coroutine_support.h>
#include <rpc/internal/serialiser.h>
#include <rpc/internal/version.h>
#include <rpc/internal/remote_pointer.h>

// Forward declarations
namespace rpc
{
    class service;
    class service_proxy;
    class proxy_base;

    class object_proxy : public std::enable_shared_from_this<rpc::object_proxy>
    {
        object object_id_;
        stdex::member_ptr<service_proxy> service_proxy_;
        std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control_;
        std::atomic<int> inherited_reference_count_{0}; // Track inherited references from race conditions during destruction and their continued presence in the service other_zones collection

        object_proxy(object object_id, std::shared_ptr<rpc::service_proxy> service_proxy);

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(interface_ordinal interface_id, rpc::weak_ptr<proxy_base>& value);

        CORO_TASK(int) try_cast(std::function<interface_ordinal(uint64_t)> id_getter);

        friend service_proxy;

    public:
        virtual ~object_proxy();

        // Called when this object_proxy inherits a reference from a race condition during the destruction of a proxy but the service other_zones collection still has a record of it
        void inherit_extra_reference() { inherited_reference_count_++; }

        std::shared_ptr<rpc::service_proxy> get_service_proxy() const { return service_proxy_.get_nullable(); }
        object get_object_id() const { return {object_id_}; }
        destination_zone get_destination_zone_id() const;

        [[nodiscard]] CORO_TASK(int) send(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_);

        size_t get_proxy_count()
        {
            std::lock_guard guard(insert_control_);
            return proxy_map.size();
        }

        template<class T> void create_interface_proxy(rpc::shared_ptr<T>& inface);

        template<class T> CORO_TASK(int) query_interface(rpc::shared_ptr<T>& iface, bool do_remote_check = true)
        {
            auto create = [&](std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>>::iterator item) -> int
            {
                rpc::shared_ptr<proxy_base> proxy = item->second.lock();
                if (!proxy)
                {
                    // weak pointer needs refreshing
                    create_interface_proxy<T>(iface);
                    item->second = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                    return rpc::error::OK();
                }
                iface = rpc::reinterpret_pointer_cast<T>(proxy);
                return rpc::error::OK();
            };

            { // scope for the lock
                std::lock_guard guard(insert_control_);
                if (T::get_id(rpc::VERSION_2) == 0)
                {
                    CO_RETURN rpc::error::OK();
                }
                {
                    auto item = proxy_map.find(T::get_id(rpc::VERSION_2));
                    if (item != proxy_map.end())
                    {
                        CO_RETURN create(item);
                    }
                }
                if (!do_remote_check)
                {
                    create_interface_proxy<T>(iface);
                    proxy_map[T::get_id(rpc::VERSION_2)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                    CO_RETURN rpc::error::OK();
                }
            }

            // release the lock and then check for casting
            if (do_remote_check)
            {
                // see if object_id can implement interface
                int ret = CO_AWAIT try_cast(T::get_id);
                if (ret != rpc::error::OK())
                {
                    CO_RETURN ret;
                }
            }
            { // another scope for the lock
                std::lock_guard guard(insert_control_);

                // check again...
                {
                    auto item = proxy_map.find(T::get_id(rpc::VERSION_2));
                    if (item != proxy_map.end())
                    {
                        CO_RETURN create(item);
                    }
                }
                create_interface_proxy<T>(iface);
                proxy_map[T::get_id(rpc::VERSION_2)] = rpc::reinterpret_pointer_cast<proxy_base>(iface);
                CO_RETURN rpc::error::OK();
            }
        }
    };
}