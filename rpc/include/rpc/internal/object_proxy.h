/*
 *   Copyright (c) 2024 Edward Boggis-Rolfe
 *   All rights reserved.
 */
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>


namespace rpc
{
    NAMESPACE_INLINE_BEGIN
    class object_proxy : public std::enable_shared_from_this<object_proxy>
    {
        object object_id_;
        std::shared_ptr<service_proxy> service_proxy_;
        std::unordered_map<interface_ordinal, rpc::weak_ptr<proxy_base>> proxy_map;
        std::mutex insert_control_;

        object_proxy(object object_id, std::shared_ptr<service_proxy> service_proxy);

        // note the interface pointer may change if there is already an interface inserted successfully
        void register_interface(interface_ordinal interface_id, rpc::weak_ptr<proxy_base>& value);

        int try_cast(std::function<interface_ordinal(uint64_t)> id_getter);

        friend service_proxy;

    public:
        virtual ~object_proxy();

        std::shared_ptr<service_proxy> get_service_proxy() const { return service_proxy_; }
        object get_object_id() const { return {object_id_}; }
        destination_zone get_destination_zone_id() const;

        [[nodiscard]] int send(uint64_t protocol_version,
            rpc::encoding encoding,
            uint64_t tag,
            rpc::interface_ordinal interface_id,
            rpc::method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_);

        [[nodiscard]] int send(uint64_t tag,
            std::function<interface_ordinal(uint64_t)> id_getter,
            method method_id,
            size_t in_size_,
            const char* in_buf_,
            std::vector<char>& out_buf_);

        size_t get_proxy_count()
        {
            std::lock_guard guard(insert_control_);
            return proxy_map.size();
        }

        void increment_remote_strong(){}
        void decrement_remote_strong_and_signal_if_appropriate(){}
        void increment_remote_weak_callable(){}
        void decrement_remote_weak_callable_and_signal_if_appropriate(){}

        template<class T> int query_interface(rpc::shared_ptr<T>& iface, bool do_remote_check = true);
    };
    
    NAMESPACE_INLINE_END
}