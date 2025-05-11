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
    
    template<class T> class proxy_impl;

    // non virtual class to allow for type erasure
    class proxy_base
    {
        std::shared_ptr<object_proxy> object_proxy_;

    protected:
        proxy_base(std::shared_ptr<object_proxy> object_proxy)
            : object_proxy_(object_proxy)
        {
        }
        virtual ~proxy_base() { }

        template<class T>
        interface_descriptor proxy_bind_in_param(
            uint64_t protocol_version, const rpc::shared_ptr<T>& iface, std::shared_ptr<rpc::object_stub>& stub);
        template<class T>
        interface_descriptor stub_bind_out_param(uint64_t protocol_version,
            caller_channel_zone caller_channel_zone_id,
            caller_zone caller_zone_id,
            const rpc::shared_ptr<T>& iface);

        template<class T1, class T2>
        friend rpc::shared_ptr<T1> dynamic_pointer_cast(const shared_ptr<T2>& from) noexcept;
        friend service;

    public:
        std::shared_ptr<object_proxy> get_object_proxy() const { return object_proxy_; }
    };
    
    NAMESPACE_INLINE_END
}